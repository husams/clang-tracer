#include "context_scheduler.hpp"

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <limits>
#include <new>
#include <string_view>

namespace tracer::context_scheduler {
namespace {
thread_local unsigned pthread_bypass_depth = 0;
struct Record {
  std::atomic<bool> occupied{false};
  std::atomic<unsigned> references{0};
  void* (*start)(void*) = nullptr;
  void* argument = nullptr;
  std::shared_ptr<context_engine::TaskCapture> capture;
  TaskMetadata metadata{};
  ActiveContextGetter current_context = nullptr;
  WorkerContextGetter worker_context = nullptr;
  ActiveContextSetter set_context = nullptr;
  context_execution::Diagnostics* diagnostics = nullptr;
};

static_assert(sizeof(Record) * NativePthreadAdapter::max_outstanding_wrappers <=
              NativePthreadAdapter::wrapper_budget_bytes);
std::array<Record, NativePthreadAdapter::max_outstanding_wrappers> records;
std::atomic<std::size_t> metadata_bytes{0};
std::atomic<std::size_t> outstanding_wrappers{0};
std::atomic<std::uint64_t> next_identity{1};
const std::uint64_t process_generation = [] {
  const auto ticks =
      static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
  return ticks ? ticks : 1;
}();

Identity new_identity() noexcept {
  auto value = next_identity.fetch_add(1, std::memory_order_relaxed);
  if (value == 0 || value == std::numeric_limits<std::uint64_t>::max()) {
    // Keep IDs generation-bearing instead of wrapping into an old identity.
    const auto generation = next_identity.fetch_add(1, std::memory_order_relaxed);
    return {process_generation ^ generation, 1};
  }
  return {process_generation, value};
}

struct ThreadMetadata {
  TaskMetadata value{};
  std::uint64_t sequence = 0;
  bool initialized = false;
};

thread_local ThreadMetadata thread_metadata;

void ensure_thread_metadata() noexcept {
  if (thread_metadata.initialized) {
    return;
  }
  thread_metadata.value.task_id = new_identity();
  thread_metadata.value.source_id = new_identity();
  thread_metadata.initialized = true;
}

Record* reserve_record() noexcept {
  auto used = metadata_bytes.load(std::memory_order_relaxed);
  while (used <= NativePthreadAdapter::wrapper_budget_bytes - sizeof(Record)) {
    if (metadata_bytes.compare_exchange_weak(used, used + sizeof(Record), std::memory_order_acq_rel,
                                             std::memory_order_relaxed)) {
      for (auto& record : records) {
        bool expected = false;
        if (record.occupied.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
          outstanding_wrappers.fetch_add(1, std::memory_order_relaxed);
          return &record;
        }
      }
      metadata_bytes.fetch_sub(sizeof(Record), std::memory_order_release);
      return nullptr;
    }
  }
  return nullptr;
}

void release_record(Record* record) noexcept {
  if (!record || record->references.fetch_sub(1, std::memory_order_acq_rel) != 1) {
    return;
  }
  record->capture.reset();
  record->start = nullptr;
  record->argument = nullptr;
  record->metadata = {};
  record->current_context = nullptr;
  record->worker_context = nullptr;
  record->set_context = nullptr;
  record->diagnostics = nullptr;
  record->occupied.store(false, std::memory_order_release);
  outstanding_wrappers.fetch_sub(1, std::memory_order_release);
  metadata_bytes.fetch_sub(sizeof(Record), std::memory_order_release);
}

struct RecordLease {
  Record* record;
  bool active = true;

  void release() noexcept {
    if (active) {
      active = false;
      release_record(record);
    }
  }

  ~RecordLease() { release(); }
};

struct MetadataRestore {
  ThreadMetadata prior;
  context_engine::Context* prior_context = nullptr;
  ActiveContextSetter setter = nullptr;
  bool active = true;

  ~MetadataRestore() noexcept { restore(); }

  void restore() noexcept {
    if (!active) {
      return;
    }
    active = false;
    thread_metadata = prior;
    if (setter) {
      (void)setter(prior_context);
    }
  }
};

struct WorkerCleanup {
  RecordLease* record_lease = nullptr;
  MetadataRestore* metadata_restore = nullptr;
  context_engine::TaskAttachment* attachment = nullptr;

  void run() noexcept {
    if (attachment) {
      attachment->~TaskAttachment();
      attachment = nullptr;
    }
    if (metadata_restore) {
      metadata_restore->restore();
    }
    if (record_lease) {
      record_lease->release();
    }
  }
};

struct WorkerCompletionGuard {
  WorkerCleanup *cleanup = nullptr;
  ~WorkerCompletionGuard() {
    if (cleanup) cleanup->run();
  }
};

void cleanup_worker(void* opaque) noexcept {
  static_cast<WorkerCleanup*>(opaque)->run();
}

void report_admission_failure(context_execution::Diagnostics* diagnostics,
                              std::string_view boundary) noexcept {
  if (diagnostics) {
    diagnostics->record(context_execution::Severity::warning,
                        context_execution::Reason::capture_failure,
                        context_execution::Phase::capture,
                        context_execution::Action::continued, 0, 0, boundary);
  }
}

void* thread_start(void* opaque) {
  auto* record = static_cast<Record*>(opaque);
  RecordLease record_lease{record};
  const auto prior_metadata = thread_metadata;
  auto* prior_context = record->current_context ? record->current_context() : nullptr;
  MetadataRestore restore{prior_metadata, prior_context, record->set_context};
  WorkerCleanup cleanup{&record_lease, &restore, nullptr};
  WorkerCompletionGuard completion{&cleanup};
  void* result = nullptr;
  pthread_cleanup_push(&cleanup_worker, &cleanup);

  thread_metadata = {};
  thread_metadata.value.task_id = record->metadata.task_id;
  thread_metadata.value.source_id = new_identity();
  thread_metadata.value.parent_task_id = record->metadata.parent_task_id;
  thread_metadata.value.parent_source_id = record->metadata.parent_source_id;
  thread_metadata.value.parent_submission_sequence = record->metadata.parent_submission_sequence;
  thread_metadata.value.inherited = false;
  thread_metadata.initialized = true;

  alignas(context_engine::TaskAttachment)
      std::byte attachment_storage[sizeof(context_engine::TaskAttachment)];
  if (record->capture && record->worker_context) {
    try {
      auto& context = record->worker_context();
      if (record->set_context) {
        restore.prior_context = record->set_context(&context);
      }
      try {
        cleanup.attachment =
            ::new (attachment_storage) context_engine::TaskAttachment(context, record->capture);
        thread_metadata.value.inherited = true;
      } catch (...) {
        if (record->set_context)
          (void)record->set_context(restore.prior_context);
        thread_metadata.value.parent_task_id = {};
        thread_metadata.value.parent_source_id = {};
        thread_metadata.value.parent_submission_sequence = 0;
        report_admission_failure(record->diagnostics,
                                 "pthread_task_attachment");
      }
    } catch (...) {
      thread_metadata.value.parent_task_id = {};
      thread_metadata.value.parent_source_id = {};
      thread_metadata.value.parent_submission_sequence = 0;
      report_admission_failure(record->diagnostics,
                               "pthread_worker_context_getter");
    }
  } else {
    thread_metadata.value.parent_task_id = {};
    thread_metadata.value.parent_source_id = {};
    thread_metadata.value.parent_submission_sequence = 0;
  }

  try {
    result = record->start(record->argument);
  } catch (...) {
    // A C++ exception escaping a POSIX start routine is fatal, but the C
    // pthread trampoline does not promise to unwind our C++ frame. Release
    // native task state before preserving that fatal application behavior.
    cleanup.run();
    throw;
  }
  pthread_cleanup_pop(1);
  return result;
}
} // namespace

TaskMetadata current_task_metadata() noexcept {
  ensure_thread_metadata();
  auto result = thread_metadata.value;
  result.source_sequence = thread_metadata.sequence;
  return result;
}

std::size_t NativePthreadAdapter::outstanding_wrapper_count() noexcept {
  return outstanding_wrappers.load(std::memory_order_acquire);
}

std::uint64_t next_source_sequence() noexcept {
  ensure_thread_metadata();
  if (thread_metadata.sequence == std::numeric_limits<std::uint64_t>::max()) {
    thread_metadata.value.source_id = new_identity();
    thread_metadata.sequence = 0;
  }
  return ++thread_metadata.sequence;
}

ScopedPthreadBypass::ScopedPthreadBypass() noexcept { ++pthread_bypass_depth; }
ScopedPthreadBypass::~ScopedPthreadBypass() noexcept { --pthread_bypass_depth; }

NativePthreadAdapter::NativePthreadAdapter(
    std::shared_ptr<context_engine::PropagationLedger> ledger, ActiveContextGetter current_context,
    WorkerContextGetter worker_context, ActiveContextSetter set_context,
    context_execution::Diagnostics& diagnostics) noexcept
    : ledger_(std::move(ledger))
    , current_context_(current_context)
    , worker_context_(worker_context)
    , set_context_(set_context)
    , diagnostics_(&diagnostics) {}

int NativePthreadAdapter::create(PthreadCreate original, pthread_t* thread,
                                 const pthread_attr_t* attributes, void* (*start)(void*),
                                 void* argument) noexcept {
  if (!original) {
    return EINVAL;
  }
  if (!start || pthread_bypass_depth ||
      context_execution::current().mode != context_execution::Mode::Application) {
    return original(thread, attributes, start, argument);
  }

  ensure_thread_metadata();
  auto* parent_context = current_context_ ? current_context_() : nullptr;
  // No context can be propagated from an unrelated runtime worker. Wrapping
  // such long-lived threads would make removal wait for their process lifetime.
  if (!parent_context)
    return original(thread, attributes, start, argument);
  if (parent_context && !parent_context->permits_cross_domain_capture()) {
    report_admission_failure(diagnostics_, "pthread_context_confinement");
    return original(thread, attributes, start, argument);
  }
  Record* record = reserve_record();
  if (!record) {
    report_admission_failure(diagnostics_, "pthread_wrapper_capacity");
    return original(thread, attributes, start, argument);
  }
  record->references.store(2, std::memory_order_release); // submitter + worker
  record->start = start;
  record->argument = argument;
  record->current_context = current_context_;
  record->worker_context = worker_context_;
  record->set_context = set_context_;
  record->diagnostics = diagnostics_;
  record->metadata.task_id = new_identity();
  record->metadata.parent_task_id = thread_metadata.value.task_id;
  record->metadata.parent_source_id = thread_metadata.value.source_id;
  record->metadata.parent_submission_sequence = next_source_sequence();
  if (parent_context && ledger_) {
    record->capture = context_engine::TaskCapture::capture(*parent_context, ledger_);
  }
  if (!record->capture) {
    record->metadata.parent_task_id = {};
    record->metadata.parent_source_id = {};
    record->metadata.parent_submission_sequence = 0;
    report_admission_failure(diagnostics_, "pthread_task_capture");
  }
  const int result = original(thread, attributes, &thread_start, record);
  if (result != 0) {
    if (record->capture) {
      (void)record->capture->cancel_before_start();
    }
    record->references.store(1, std::memory_order_release); // Worker never ran.
    release_record(record);
    return result;
  }
  release_record(record);
  return result;
}

} // namespace tracer::context_scheduler

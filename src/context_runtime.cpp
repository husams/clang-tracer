#include "context_runtime.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <deque>
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>
#include <thread>
#include <utility>

#include "context_scheduler.hpp"
#include "tracer/compat.hpp"

namespace tracer::context_runtime {
namespace {
struct WorkerContext final {
  context_engine::Diagnostics diagnostics;
  alignas(context_engine::Context) std::byte
      context_storage[sizeof(context_engine::Context)];
  context_engine::Context *context = nullptr;

  WorkerContext() noexcept
      : context(new (context_storage) context_engine::Context(diagnostics)) {}
  ~WorkerContext() noexcept {
    if (context)
      context->~Context();
  }
};

struct FrameToken final {
  SharedSourceRuntime *runtime = nullptr;
  context_engine::Context *context = nullptr;
  alignas(context_engine::InvocationFrame)
      std::byte storage[sizeof(context_engine::InvocationFrame)];
  context_engine::InvocationFrame *frame = nullptr;
  bool active = false;
};

struct HookToken final {
  SharedSourceRuntime *runtime = nullptr;
  context_engine::Context *context = nullptr;
  context_facade::NativeHookContext native{};
  const TraceContext *trace = nullptr;
  std::vector<context_engine::SlotRequest> requests{};
  std::size_t request_count = 0;
  std::deque<context_engine::ContextCapturePool::Lease> captures{};
  alignas(context_execution::CurrentHook) std::byte
      current_storage[sizeof(context_execution::CurrentHook)];
  context_execution::CurrentHook *current = nullptr;
  alignas(context_engine::HookExit) std::byte
      exit_storage[sizeof(context_engine::HookExit)];
  context_engine::HookExit *exit = nullptr;
  alignas(context_execution::AccessLease) std::byte
      lease_storage[sizeof(context_execution::AccessLease)];
  context_execution::AccessLease *access_lease = nullptr;
  alignas(context_engine::HookAccess) std::byte
      access_storage[sizeof(context_engine::HookAccess)];
  context_engine::HookAccess *access = nullptr;
  bool active = false;
  bool direct = false;
  bool events_planned = false;
};

struct AccessToken final {
  SharedSourceRuntime *runtime = nullptr;
  context_engine::Context *context = nullptr;
  std::vector<context_engine::SlotRequest> requests{};
  std::size_t request_count = 0;
  alignas(context_execution::AccessLease) std::byte
      lease_storage[sizeof(context_execution::AccessLease)];
  context_execution::AccessLease *lease = nullptr;
  alignas(context_engine::HookAccess) std::byte
      access_storage[sizeof(context_engine::HookAccess)];
  context_engine::HookAccess *access = nullptr;
  bool active = false;
  bool borrowed = false;
};

struct RuntimeThreadState final {
  std::deque<FrameToken> frames{};
  HookToken hook{};
  AccessToken access{};
};

struct WorkspaceSlot final {
  alignas(WorkerContext) std::byte worker_storage[sizeof(WorkerContext)];
  alignas(RuntimeThreadState) std::byte
      thread_state_storage[sizeof(RuntimeThreadState)];
  WorkerContext *worker = nullptr;
  RuntimeThreadState *thread_state = nullptr;

  ~WorkspaceSlot() { destroy(); }

  void construct() {
    worker = new (worker_storage) WorkerContext();
    thread_state = new (thread_state_storage) RuntimeThreadState();
  }
  void destroy() noexcept {
    if (thread_state)
      thread_state->~RuntimeThreadState();
    if (worker)
      worker->~WorkerContext();
    thread_state = nullptr;
    worker = nullptr;
  }
};

std::atomic<std::size_t> workspace_count{0};
std::atomic<std::uint64_t> native_event_sequence{0};
thread_local context_engine::Context *tls_context = nullptr;
thread_local const tracer::Call *tls_root_call = nullptr;
[[noreturn]] void engine_error(context_engine::ErrorCode code);

void release_workspace(WorkspaceSlot *slot) noexcept {
  if (!slot)
    return;
  slot->destroy();
  workspace_count.fetch_sub(1, std::memory_order_relaxed);
  std::unique_ptr<WorkspaceSlot> owned(slot);
}

WorkspaceSlot *try_acquire_workspace() noexcept {
  try {
    auto slot = std::make_unique<WorkspaceSlot>();
    slot->construct();
    workspace_count.fetch_add(1, std::memory_order_relaxed);
    return slot.release();
  } catch (...) {
    return nullptr;
  }
}

struct ThreadWorkspaceLease final {
  WorkspaceSlot *slot = nullptr;
  unsigned invocation_depth = 0;
  bool pinned = false;
  ~ThreadWorkspaceLease() noexcept { release_workspace(slot); }
};
thread_local ThreadWorkspaceLease thread_workspace;

WorkspaceSlot *ensure_workspace(bool pin) {
  // The anchor's TLS destructor destroys Core::Context and may drain the
  // engine's per-thread retirement queue. Register those TLS destructors first
  // so they outlive the workspace cleanup at thread exit.
  (void)context_execution::current();
  context_engine::prepare_thread_context();
  if (!thread_workspace.slot) {
    thread_workspace.slot = try_acquire_workspace();
    if (!thread_workspace.slot)
      engine_error(context_engine::ErrorCode::capacity);
  }
  if (pin)
    thread_workspace.pinned = true;
  return thread_workspace.slot;
}

void begin_workspace_invocation() {
  (void)ensure_workspace(false);
  ++thread_workspace.invocation_depth;
}

void end_workspace_invocation() noexcept {
  if (!thread_workspace.invocation_depth)
    return;
  --thread_workspace.invocation_depth;
  if (!thread_workspace.invocation_depth && !thread_workspace.pinned) {
    auto *slot = thread_workspace.slot;
    thread_workspace.slot = nullptr;
    release_workspace(slot);
  }
}

RuntimeThreadState &slots() noexcept {
  auto *slot = thread_workspace.slot;
  if (!slot) {
    // Native callbacks are admitted only inside Invocation, while public
    // worker_context() pins a slot before scheduler work starts.
    std::terminate();
  }
  return *slot->thread_state;
}

[[noreturn]] void engine_error(context_engine::ErrorCode code) {
  throw context_engine::Error(code);
}

void report_native_status(context_engine::ErrorCode code,
                          context_execution::Phase phase,
                          const TraceContext *trace) noexcept {
  if (code == context_engine::ErrorCode::ok)
    return;
  using context_execution::Reason;
  Reason reason = Reason::access_failure;
  switch (code) {
  case context_engine::ErrorCode::missing_state:
    reason = Reason::missing_state;
    break;
  case context_engine::ErrorCode::incompatible_registration:
    reason = Reason::incompatible_state;
    break;
  case context_engine::ErrorCode::borrowed_end:
  case context_engine::ErrorCode::repeated_end:
  case context_engine::ErrorCode::end_order:
    reason = Reason::invalid_ownership;
    break;
  case context_engine::ErrorCode::no_active_hook:
    reason = Reason::no_active_hook;
    break;
  case context_engine::ErrorCode::construction_failed:
    reason = Reason::constructor_failure;
    break;
  case context_engine::ErrorCode::lifecycle_access:
    reason = Reason::lifecycle_access;
    break;
  default:
    break;
  }
  context_execution::runtime_diagnostics().record(
      context_execution::Severity::warning, reason, phase,
      context_execution::Action::callback_skipped, trace ? trace->call_id : 0);
}

// Catch in the native runtime that threw the exception. The callback module
// has its own C++ runtime and must receive a plain status instead.
template <class Operation>
context_facade::Status native_status(Operation &&operation,
                                     context_execution::Phase phase) noexcept {
  context_engine::ErrorCode code;
  try {
    operation();
    return 0;
  } catch (const context_engine::Error &error) {
    code = error.code();
  } catch (...) {
    code = context_engine::ErrorCode::capacity;
  }
  report_native_status(code, phase,
      tls_root_call ? context_facade::NativeHookContextFactory::trace(*tls_root_call)
                    : nullptr);
  return static_cast<context_facade::Status>(code);
}

std::string_view capture_status_name(context_engine::CaptureStatus status) noexcept {
  using context_engine::CaptureStatus;
  switch (status) {
  case CaptureStatus::ok: return "capture_ok";
  case CaptureStatus::bytes_exceeded: return "capture_bytes";
  case CaptureStatus::string_exceeded: return "capture_string";
  case CaptureStatus::invalid_field: return "capture_invalid_field";
  case CaptureStatus::depth_exceeded: return "capture_depth";
  case CaptureStatus::visits_exceeded: return "capture_visits";
  case CaptureStatus::items_exceeded: return "capture_items";
  case CaptureStatus::invalid_cap: return "capture_invalid_cap";
  case CaptureStatus::incomplete_message: return "capture_incomplete";
  case CaptureStatus::work_exceeded: return "capture_work";
  }
  return "capture_unknown";
}

void report_capture_drop(const TraceContext *trace,
                         std::string_view reason) noexcept {
  context_execution::runtime_diagnostics().record(
      context_execution::Severity::warning,
      context_execution::Reason::capture_failure,
      context_execution::Phase::capture,
      context_execution::Action::event_dropped,
      trace ? trace->call_id : 0, 0, reason);
}

const context_engine::SlotRequest &
resolve_request(SharedSourceRuntime &runtime, context_engine::Identity state,
                context_engine::SlotRequest &storage) {
  const auto *entry = runtime.find_state(state);
  if (!entry || !entry->owner || !entry->registered)
    engine_error(context_engine::ErrorCode::incompatible_registration);
  storage = {entry->owner->registration, entry->state};
  return storage;
}

struct Captured final {
  SharedSourceRuntime *runtime = nullptr;
  std::vector<context_engine::CaptureSchemaView> schemas{};
  std::vector<context_engine::CaptureStateView> states{};
  std::size_t schema_count = 0;
  std::size_t state_count = 0;
  bool failed = false;
};
static_assert(sizeof(Captured) <= 16 * 1024);

void collect_state(void *opaque, context_engine::Identity runtime_scope,
                   context_engine::Identity state_identity,
                   const context_engine::ScopeRegistration &registration,
                   void *payload) noexcept {
  auto &out = *static_cast<Captured *>(opaque);
  if (out.failed) {
    out.failed = true;
    return;
  }
  const auto *entry = out.runtime->find_state(state_identity);
  const auto *registered =
      entry && entry->registered && entry->owner &&
              entry->owner->registration.get() == &registration
          ? entry->registered
          : registered_context_state(registration, state_identity);
  if (!registered || !registered->schema) {
    out.failed = true;
    return;
  }
  const auto base_epoch = out.runtime->metadata().epoch_schema_id;
  auto schema = registered->schema_view(base_epoch);
  const auto duplicate =
      std::find_if(out.schemas.begin(), out.schemas.begin() + out.schema_count,
                   [&](const auto &existing) {
                     return existing.identity == schema.identity;
                   });
  std::uint64_t state_epoch = base_epoch;
  if (duplicate == out.schemas.begin() + out.schema_count) {
    if (out.schema_count > std::numeric_limits<std::uint64_t>::max() - base_epoch) {
      out.failed = true;
      return;
    }
    state_epoch += out.schema_count;
    schema.epoch_schema_id = state_epoch;
    try {
      out.schemas.push_back(schema);
      ++out.schema_count;
    } catch (...) {
      out.failed = true;
      return;
    }
  } else {
    state_epoch = duplicate->epoch_schema_id;
  }
  try {
    out.states.push_back(registered->capture_view(runtime_scope, state_epoch, payload));
    ++out.state_count;
  } catch (...) {
    out.failed = true;
  }
}

std::uint64_t next_native_event_sequence() noexcept {
  auto current = native_event_sequence.load(std::memory_order_relaxed);
  // A bounded CAS loop fails closed under extreme contention and, at counter
  // exhaustion, never wraps into an identifier that was already issued.
  for (unsigned attempt = 0; attempt < 16; ++attempt) {
    if (current == std::numeric_limits<std::uint64_t>::max())
      return 0;
    if (native_event_sequence.compare_exchange_weak(current, current + 1,
                                                    std::memory_order_relaxed,
                                                    std::memory_order_relaxed))
      return current + 1;
  }
  return 0;
}

std::uint64_t native_process_generation() noexcept {
  // The module-local counter address is fixed for this loaded process
  // generation and requires no lazy initialization on the guarded path.
  return static_cast<std::uint64_t>(
      reinterpret_cast<std::uintptr_t>(&native_event_sequence));
}

void make_event_id(std::array<char, 16> &out, std::uint64_t generation,
                   std::uint64_t sequence) noexcept {
  for (unsigned i = 0; i < 8; ++i) {
    out[i] = static_cast<char>(generation >> ((7 - i) * 8));
    out[i + 8] = static_cast<char>(sequence >> ((7 - i) * 8));
  }
}

std::string_view encode_identity(context_scheduler::Identity identity,
                                 std::array<char, 16> &storage) noexcept {
  if (!identity.generation && !identity.value)
    return {};
  for (unsigned i = 0; i < 8; ++i) {
    storage[i] = static_cast<char>(identity.generation >> (i * 8));
    storage[i + 8] = static_cast<char>(identity.value >> (i * 8));
  }
  return {storage.data(), storage.size()};
}

void encode_u64(std::uint64_t value, char *out) noexcept {
  for (unsigned i = 0; i < 8; ++i)
    out[i] = static_cast<char>(value >> (i * 8));
}

} // namespace

context_engine::Context *active_context() noexcept { return tls_context; }

context_engine::Context &worker_context() {
  auto *slot = ensure_workspace(true);
  return *slot->worker->context;
}

std::size_t workspace_slots_in_use() noexcept {
  return workspace_count.load(std::memory_order_acquire);
}

context_engine::Context *
exchange_active_context(context_engine::Context *next) noexcept {
  auto *prior = tls_context;
  tls_context = next;
  return prior;
}

const context_facade::NativeFunctionTable SharedSourceRuntime::table_{
    context_facade::NativeFunctionTable::version,
    &SharedSourceRuntime::native_current_context,
    [](void *runtime) noexcept -> void * {
      void *token = nullptr;
      native_status([&] { token = native_frame_enter(runtime); },
                    context_execution::Phase::construction);
      return token;
    },
    &SharedSourceRuntime::native_frame_exit,
    [](void *runtime, void *frame, context_facade::StateId state) noexcept {
      return native_status([&] { native_create_or_attach(runtime, frame, state); },
                           context_execution::Phase::construction);
    },
    [](void *runtime, const void *call, const TraceContext *trace,
       const context_facade::RequestDescriptor *requests, std::size_t count,
       bool all_visible, bool direct) noexcept -> void * {
      void *token = nullptr;
      native_status([&] {
        token = native_hook_begin(runtime, call, trace, requests, count,
                                  all_visible, direct);
      }, context_execution::Phase::access);
      return token;
    },
    &SharedSourceRuntime::native_hook_end,
    [](void *runtime, const context_facade::RequestDescriptor *requests,
       std::size_t count, context_facade::AccessKind kind,
       context_facade::Status *status) noexcept -> void * {
      void *token = nullptr;
      const auto failure = native_status([&] {
        token = native_access_begin(runtime, requests, count, kind, status);
      }, context_execution::Phase::access);
      if (failure && status) *status = failure;
      return token;
    },
    &SharedSourceRuntime::native_access_end,
    &SharedSourceRuntime::native_get,
    [](void *runtime, context_facade::ScopeId scope,
       context_facade::StateId payload, const void *source) noexcept {
      return native_status([&] { native_init_scope(runtime, scope, payload, source); },
                           context_execution::Phase::construction);
    },
    &SharedSourceRuntime::native_end_scope,
    &SharedSourceRuntime::native_event,
    &SharedSourceRuntime::native_thread_id};

SharedSourceRuntime::SharedSourceRuntime(
    std::vector<std::shared_ptr<const RegisteredContextScope>> scopes,
    context_engine::ContextCapturePool &capture_pool, EventMetadata metadata,
    DeliveryPublish publish, void *publish_context)
    : scopes_(std::move(scopes)), capture_pool_(capture_pool),
      metadata_(std::move(metadata)), publish_(publish),
      publish_context_(publish_context) {
  if (!metadata_.epoch_schema_id || metadata_.source_id.size() != 16 ||
      metadata_.task_id.size() != 16 ||
      (!metadata_.parent_task_id.empty() && metadata_.parent_task_id.size() != 16) ||
      (!metadata_.invocation_id.empty() && metadata_.invocation_id.size() != 16) ||
      (!metadata_.probe_id.empty() && metadata_.probe_id.size() != 16) || !publish_) {
    throw std::invalid_argument("invalid shared-context event metadata");
  }
  for (const auto &scope : scopes_) {
    if (!scope || !scope->registration)
      throw std::invalid_argument(
          "incomplete frozen context registry generation");
    for (const auto &state : scope->states)
      states_.push_back({scope->identity, state.identity, scope, &state});
  }
  std::sort(states_.begin(), states_.end(),
            [](const StateEntry &a, const StateEntry &b) {
              return a.state < b.state;
            });
  for (std::size_t i = 1; i < states_.size(); ++i)
    if (states_[i - 1].state == states_[i].state)
      throw std::invalid_argument(
          "duplicate state identity in runtime registry");
}

const context_facade::NativeFunctionTable &
SharedSourceRuntime::function_table() const noexcept {
  return table_;
}

SharedSourceRuntime::Invocation::Invocation(SharedSourceRuntime &runtime,
                                            const tracer::Call *root_call)
    : prior_context_(active_context()), prior_root_call_(tls_root_call),
      root_call_(root_call),
      tracking_(std::in_place, context_execution::Mode::Tracking) {
  if (!tracking_->entered())
    throw std::logic_error("context runtime could not enter Tracking mode");
  begin_workspace_invocation();
  workspace_lease_ = true;
  auto *slot = thread_workspace.slot;
  installed_context_ = prior_context_ ? prior_context_ : slot->worker->context;
  owns_context_ = prior_context_ == nullptr;
  if (owns_context_)
    exchange_active_context(installed_context_);
  tls_root_call = root_call_;
  try {
    binding_.emplace(&runtime.function_table(), &runtime);
  } catch (...) {
    tls_root_call = prior_root_call_;
    if (owns_context_)
      exchange_active_context(prior_context_);
    installed_context_ = nullptr;
    owns_context_ = false;
    tracking_.reset();
    end_workspace_invocation();
    workspace_lease_ = false;
    throw;
  }
}

SharedSourceRuntime::Invocation::~Invocation() noexcept {
  binding_.reset();
  tls_root_call = prior_root_call_;
  if (owns_context_)
    exchange_active_context(prior_context_);
  tracking_.reset();
  if (workspace_lease_)
    end_workspace_invocation();
}

bool SharedSourceRuntime::Invocation::entered() const noexcept {
  return tracking_ && tracking_->entered() && binding_.has_value() &&
         installed_context_ && workspace_lease_;
}

const SharedSourceRuntime::StateEntry *
SharedSourceRuntime::find_state(context_engine::Identity id) const noexcept {
  const auto found = std::lower_bound(
      states_.begin(), states_.end(), id,
      [](const StateEntry &entry, context_engine::Identity key) {
        return entry.state < key;
      });
  return found != states_.end() && found->state == id ? &*found : nullptr;
}

const std::shared_ptr<const RegisteredContextScope> *
SharedSourceRuntime::find_scope(context_engine::Identity id) const noexcept {
  const auto found =
      std::find_if(scopes_.begin(), scopes_.end(),
                   [&](const auto &scope) { return scope->identity == id; });
  return found == scopes_.end() ? nullptr : &*found;
}

void SharedSourceRuntime::native_publish_hook(void *opaque, bool) noexcept {
  auto &hook = *static_cast<HookToken *>(opaque);
  if (!hook.runtime)
    return;
  for (auto &capture : hook.captures) {
    if (!capture)
      continue;
    if (capture.bytes().empty()) {
      capture = {};
      continue;
    }
    if (!capture.publish_leased(hook.runtime->publish_,
                                hook.runtime->publish_context_))
      hook.runtime->capture_drops_.fetch_add(1, std::memory_order_relaxed);
  }
}

void *SharedSourceRuntime::native_current_context(void *opaque) noexcept {
  if (!opaque)
    return nullptr;
  auto *context = active_context();
  if (context)
    return context;
  return nullptr;
}

void *SharedSourceRuntime::native_frame_enter(void *opaque) {
  auto &runtime = *static_cast<SharedSourceRuntime *>(opaque);
  auto *context = active_context();
  if (!context)
    context = &worker_context();
  auto& frames = slots().frames;
  if (std::none_of(frames.begin(), frames.end(), [](const auto& slot) { return !slot.active; })) {
    frames.emplace_back();
  }
  for (auto& slot : frames) {
    if (slot.active)
      continue;
    slot.runtime = &runtime;
    slot.context = context;
    try {
      slot.frame = new (slot.storage) context_engine::InvocationFrame(*context);
      slot.active = true;
      return &slot;
    } catch (...) {
      slot.runtime = nullptr;
      slot.context = nullptr;
      throw;
    }
  }
  engine_error(context_engine::ErrorCode::capacity);
}

void SharedSourceRuntime::native_frame_exit(void *opaque,
                                            void *token) noexcept {
  (void)opaque;
  auto *slot = static_cast<FrameToken *>(token);
  if (!slot || !slot->active || !slot->frame)
    return;
  slot->frame->pop_all_and_restore();
  slot->frame->~InvocationFrame();
  slot->frame = nullptr;
  slot->runtime = nullptr;
  slot->context = nullptr;
  slot->active = false;
}

void SharedSourceRuntime::native_create_or_attach(
    void *opaque, void *frame_token, context_facade::StateId state_id) {
  (void)opaque;
  auto *frame = static_cast<FrameToken *>(frame_token);
  if (!frame || !frame->active || !frame->runtime || !frame->frame)
    engine_error(context_engine::ErrorCode::invalid_frame);
  const auto *entry = frame->runtime->find_state(state_id);
  if (!entry || !entry->owner)
    engine_error(context_engine::ErrorCode::incompatible_registration);
  context_engine::ScopeInit init;
  init.root_call = tls_root_call;
  frame->frame->create_or_attach(entry->owner->registration, init);
}

void *SharedSourceRuntime::native_hook_begin(
    void *opaque, const void *call, const TraceContext *trace,
    const context_facade::RequestDescriptor *descriptors, std::size_t count,
    bool all_visible, bool direct) {
  auto &runtime = *static_cast<SharedSourceRuntime *>(opaque);
  auto *context = active_context();
  if (!context)
    context = &worker_context();
  auto &hook = slots().hook;
  if (hook.active || (count && !descriptors)) {
    engine_error(context_engine::ErrorCode::capacity);
  }
  hook.requests.resize(count);
  hook.runtime = &runtime;
  hook.context = context;
  hook.trace = trace;
  hook.direct = direct;
  hook.events_planned = all_visible;
  hook.request_count = 0;
  hook.native = {context_facade::NativeHookContext::kMagic,
                 {&runtime.function_table(), &runtime},
                 call,
                 trace,
                 &hook};
  for (auto &capture : hook.captures)
    capture = {};

  for (std::size_t i = 0; i < count; ++i) {
    context_engine::SlotRequest request;
    resolve_request(runtime, descriptors[i].state_id, request);
    if (request.registration->identity != descriptors[i].scope_id)
      engine_error(context_engine::ErrorCode::incompatible_registration);
    hook.requests[hook.request_count++] = std::move(request);
  }

  hook.captures.clear();

  try {
    hook.current =
        new (hook.current_storage) context_execution::CurrentHook(&hook.native);
    if (!hook.current->installed())
      engine_error(context_engine::ErrorCode::no_active_hook);
    hook.exit = new (hook.exit_storage) context_engine::HookExit(
        *context, &SharedSourceRuntime::native_publish_hook, &hook);
    if (direct) {
      hook.access_lease =
          new (hook.lease_storage) context_execution::AccessLease();
      if (!hook.access_lease->entered())
        engine_error(context_engine::ErrorCode::lifecycle_access);
      hook.access = new (hook.access_storage) context_engine::HookAccess(
          *context, ::tracer::compat::span(hook.requests.data(), hook.request_count), all_visible,
          context_engine::HookAccess::Kind::direct);
    }
    hook.active = true;
    return &hook;
  } catch (...) {
    if (hook.access) {
      hook.access->~HookAccess();
      hook.access = nullptr;
    }
    if (hook.access_lease) {
      hook.access_lease->~AccessLease();
      hook.access_lease = nullptr;
    }
    context_engine::drain_lifecycle();
    if (hook.exit) {
      hook.exit->~HookExit();
      hook.exit = nullptr;
    }
    if (hook.current) {
      hook.current->~CurrentHook();
      hook.current = nullptr;
    }
    for (auto &capture : hook.captures)
      capture = {};
    hook.runtime = nullptr;
    hook.context = nullptr;
    hook.trace = nullptr;
    throw;
  }
}

void SharedSourceRuntime::native_hook_end(void *opaque, void *token,
                                          bool failed) noexcept {
  (void)opaque;
  auto *hook = static_cast<HookToken *>(token);
  if (!hook || !hook->runtime || !hook->active)
    return;
  if (failed && hook->exit)
    hook->exit->capture_failure();
  if (hook->access) {
    hook->access->~HookAccess();
    hook->access = nullptr;
  }
  if (hook->access_lease) {
    hook->access_lease->~AccessLease();
    hook->access_lease = nullptr;
  }
  context_engine::drain_lifecycle();
  if (hook->exit) {
    hook->exit->~HookExit();
    hook->exit = nullptr;
  }
  if (hook->current) {
    hook->current->~CurrentHook();
    hook->current = nullptr;
  }
  for (auto &capture : hook->captures)
    capture = {};
  hook->runtime = nullptr;
  hook->context = nullptr;
  hook->trace = nullptr;
  hook->request_count = 0;
  hook->active = false;
}

void *SharedSourceRuntime::native_access_begin(
    void *opaque, const context_facade::RequestDescriptor *descriptors,
    std::size_t count, context_facade::AccessKind,
    context_facade::Status *status) {
  if (status)
    *status = 0;
  auto &runtime = *static_cast<SharedSourceRuntime *>(opaque);
  auto *context = active_context();
  if (!context)
    context = &worker_context();
  auto &access = slots().access;
  const bool had_active_access = access.active;
  const bool borrowing = slots().hook.active &&
                         slots().hook.runtime == &runtime &&
                         slots().hook.access;
  const auto reject = [&](context_engine::ErrorCode code) -> void * {
    if (!had_active_access) {
      access.runtime = nullptr;
      access.context = nullptr;
      access.request_count = 0;
      access.requests.clear();
      access.borrowed = false;
      access.active = false;
    }
    if (status)
      *status = static_cast<context_facade::Status>(code);
    report_native_status(code, context_execution::Phase::access,
                         slots().hook.active ? slots().hook.trace : nullptr);
    return nullptr;
  };
  if (access.active || (count && !descriptors)) {
    if (borrowing)
      return reject(context_engine::ErrorCode::active_access);
    engine_error(context_engine::ErrorCode::active_access);
  }
  access.runtime = &runtime;
  access.context = context;
  access.request_count = 0;
  access.requests.clear();
  for (std::size_t i = 0; i < count; ++i) {
    context_engine::SlotRequest request;
    if (borrowing) {
      const auto *entry = runtime.find_state(descriptors[i].state_id);
      if (!entry || !entry->owner || !entry->registered)
        return reject(context_engine::ErrorCode::incompatible_registration);
      request = {entry->owner->registration, entry->state};
    } else {
      resolve_request(runtime, descriptors[i].state_id, request);
    }
    if (request.registration->identity != descriptors[i].scope_id) {
      if (borrowing)
        return reject(context_engine::ErrorCode::incompatible_registration);
      engine_error(context_engine::ErrorCode::incompatible_registration);
    }
    access.requests.push_back(std::move(request));
    ++access.request_count;
  }
  // A visitor nested in a direct hook borrows that hook's already planned and
  // locked set. The generated lifetime analysis guarantees the visitor cannot
  // retain the reference beyond this synchronous call.
  if (borrowing) {
    for (std::size_t i = 0; i < access.request_count; ++i) {
      context_engine::ErrorCode code = context_engine::ErrorCode::ok;
      if (!slots().hook.access->try_ref(access.requests[i], code)) {
        return reject(code);
      }
    }
    access.borrowed = true;
    access.active = true;
    return &access;
  }
  try {
    access.lease = new (access.lease_storage) context_execution::AccessLease();
    if (!access.lease->entered()) {
      engine_error(context_engine::ErrorCode::lifecycle_access);
    }
    access.access = new (access.access_storage) context_engine::HookAccess(
        *context, ::tracer::compat::span(access.requests.data(), access.request_count), false,
        context_engine::HookAccess::Kind::independent);
    access.active = true;
    return &access;
  } catch (...) {
    if (access.access) {
      access.access->~HookAccess();
      access.access = nullptr;
    }
    if (access.lease) {
      access.lease->~AccessLease();
      access.lease = nullptr;
    }
    access.runtime = nullptr;
    access.context = nullptr;
    access.borrowed = false;
    throw;
  }
}

void SharedSourceRuntime::native_access_end(void *opaque,
                                            void *token) noexcept {
  (void)opaque;
  auto *access = static_cast<AccessToken *>(token);
  if (!access || !access->active)
    return;
  if (access->borrowed) {
    access->runtime = nullptr;
    access->context = nullptr;
    access->request_count = 0;
    access->borrowed = false;
    access->active = false;
    return;
  }
  if (access->access) {
    access->access->~HookAccess();
    access->access = nullptr;
  }
  if (access->lease) {
    access->lease->~AccessLease();
    access->lease = nullptr;
  }
  context_engine::drain_lifecycle();
  access->runtime = nullptr;
  access->context = nullptr;
  access->request_count = 0;
  access->active = false;
}

context_facade::Status
SharedSourceRuntime::native_get(void *opaque, context_facade::StateId state_id,
                                void **result) noexcept {
  if (result)
    *result = nullptr;
  if (!result)
    return static_cast<context_facade::Status>(
        context_engine::ErrorCode::incompatible_registration);
  auto &runtime = *static_cast<SharedSourceRuntime *>(opaque);
  auto &thread = slots();
  const auto *entry = runtime.find_state(state_id);
  if (!entry || !entry->owner) {
    report_native_status(context_engine::ErrorCode::incompatible_registration,
                         context_execution::Phase::access,
                         thread.hook.active ? thread.hook.trace : nullptr);
    return static_cast<context_facade::Status>(
        context_engine::ErrorCode::incompatible_registration);
  }
  context_engine::SlotRequest request{entry->owner->registration, state_id};
  if (thread.hook.active && thread.hook.runtime == &runtime &&
      thread.hook.context && thread.hook.access) {
    context_engine::ErrorCode code = context_engine::ErrorCode::ok;
    *result = thread.hook.context->try_get(request, code);
    report_native_status(code, context_execution::Phase::access,
                         thread.hook.trace);
    return static_cast<context_facade::Status>(code);
  }
  if (thread.access.active && thread.access.runtime == &runtime &&
      thread.access.access) {
    context_engine::ErrorCode code = context_engine::ErrorCode::ok;
    *result = thread.access.access->try_ref(request, code);
    report_native_status(code, context_execution::Phase::access,
                         thread.hook.active ? thread.hook.trace : nullptr);
    return static_cast<context_facade::Status>(code);
  }
  report_native_status(context_engine::ErrorCode::unplanned_access,
                       context_execution::Phase::access,
                       thread.hook.active ? thread.hook.trace : nullptr);
  return static_cast<context_facade::Status>(
      context_engine::ErrorCode::unplanned_access);
}

void SharedSourceRuntime::native_init_scope(void *opaque,
                                            context_facade::ScopeId scope_id,
                                            context_facade::StateId payload_id,
                                            const void *source) {
  auto &runtime = *static_cast<SharedSourceRuntime *>(opaque);
  auto *context = active_context();
  if (!context)
    engine_error(context_engine::ErrorCode::invalid_frame);
  const auto *scope = runtime.find_scope(scope_id);
  if (!scope)
    engine_error(context_engine::ErrorCode::incompatible_registration);
  std::uint64_t initializer_identity = 0;
  if (source) {
    const auto *payload = runtime.find_state(payload_id);
    if (!payload || !payload->registered || !payload->owner ||
        !payload->registered->dependency.generic ||
        payload->scope != scope_id ||
        payload->registered->scope_identity != scope_id ||
        payload->owner->registration.get() != (*scope)->registration.get())
      engine_error(context_engine::ErrorCode::incompatible_registration);
    initializer_identity = payload->registered->initializer_identity;
  }
  context_engine::ScopeInit init;
  if (const auto *hook = context_facade::active_native_hook())
    init.root_call = hook->call;
  init.payload_identity = initializer_identity;
  init.payload_source = source;
  context->init_scope((*scope)->registration, init);
}

context_facade::Status SharedSourceRuntime::native_end_scope(
    void *opaque, context_facade::ScopeId scope_id) noexcept {
  (void)opaque;
  auto *context = active_context();
  if (!context) {
    report_native_status(context_engine::ErrorCode::invalid_frame,
                         context_execution::Phase::lifecycle,
                         slots().hook.active ? slots().hook.trace : nullptr);
    return static_cast<context_facade::Status>(
        context_engine::ErrorCode::invalid_frame);
  }
  const auto status = context->try_end_scope(scope_id);
  report_native_status(status, context_execution::Phase::access,
                       slots().hook.active ? slots().hook.trace : nullptr);
  return static_cast<context_facade::Status>(status);
}

bool SharedSourceRuntime::native_event(void *opaque, const TraceContext *trace,
                                       std::string_view name,
                                       std::size_t max_items) noexcept {
  auto &runtime = *static_cast<SharedSourceRuntime *>(opaque);
  auto &hook = slots().hook;
  if (max_items > 4096) {
    runtime.capture_drops_.fetch_add(1, std::memory_order_relaxed);
    context_execution::runtime_diagnostics().record(
        context_execution::Severity::warning,
        context_execution::Reason::capture_failure,
        context_execution::Phase::capture,
        context_execution::Action::event_dropped, trace ? trace->call_id : 0, 0,
        name);
    return false;
  }
  if (!trace || !hook.active || hook.runtime != &runtime || hook.trace != trace) {
    runtime.capture_drops_.fetch_add(1, std::memory_order_relaxed);
    report_capture_drop(trace, "capture_hook_mismatch");
    return false;
  }
  // A visitor is an independent short interval. Its lock set cannot be
  // extended by event(), even when the event snapshot would need more slots.
  if (slots().access.active) {
    runtime.capture_drops_.fetch_add(1, std::memory_order_relaxed);
    context_execution::runtime_diagnostics().record(
        context_execution::Severity::warning,
        context_execution::Reason::access_failure,
        context_execution::Phase::capture,
        context_execution::Action::event_dropped, trace->call_id, 0, name);
    return false;
  }
  if (!hook.events_planned) {
    runtime.capture_drops_.fetch_add(1, std::memory_order_relaxed);
    report_capture_drop(trace, "capture_unplanned_event");
    return false;
  }
  try {
    hook.captures.push_back(runtime.capture_pool_.try_reserve(
        runtime.metadata_.capture_hook_id, runtime.metadata_.capture_event_bytes));
  } catch (...) {
    runtime.capture_drops_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  auto& lease = hook.captures.back();
  if (!lease) {
    runtime.capture_drops_.fetch_add(1, std::memory_order_relaxed);
    report_capture_drop(trace, "capture_allocation_failed");
    return false;
  }
  const auto capture = [&](context_engine::HookAccess &access) noexcept {
    Captured captured;
    captured.runtime = &runtime;
    if (!access.visit_all(&collect_state, &captured) || captured.failed) {
      runtime.capture_drops_.fetch_add(1, std::memory_order_relaxed);
      report_capture_drop(trace, "capture_state_snapshot");
      return false;
    }
    (void)context_scheduler::next_source_sequence();
    const auto task = context_scheduler::current_task_metadata();
    const auto sequence = task.source_sequence;
    std::array<char, 16> event_id{};
    const auto unique_sequence = next_native_event_sequence();
    if (unique_sequence == 0) {
      runtime.capture_drops_.fetch_add(1, std::memory_order_relaxed);
      report_capture_drop(trace, "capture_event_id_exhausted");
      return false;
    }
    make_event_id(event_id, native_process_generation(), unique_sequence);
    std::array<char, 16> source_id{}, task_id{}, parent_task_id{},
        parent_source_id{}, invocation_id{};
    const auto source_view = encode_identity(task.source_id, source_id);
    const auto task_view = encode_identity(task.task_id, task_id);
    const auto parent_task_view =
        encode_identity(task.parent_task_id, parent_task_id);
    const auto parent_source_view =
        encode_identity(task.parent_source_id, parent_source_id);
    encode_u64(trace->call_id, invocation_id.data());
    const auto generation =
        static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&runtime));
    encode_u64(generation ^ trace->probe_id ^ runtime.metadata_.capture_hook_id,
               invocation_id.data() + 8);
    const auto &metadata = runtime.metadata_;
    const auto max = static_cast<std::uint32_t>(max_items);
    const context_engine::CaptureEventMetadata event{
        std::string_view(event_id.data(), event_id.size()),
        source_view,
        sequence,
        task_view,
        parent_task_view,
        parent_source_view,
        task.parent_submission_sequence,
        name,
        metadata.recording_position,
        metadata.opaque_metadata,
        max,
        std::string_view(invocation_id.data(), invocation_id.size()),
        metadata.probe_id,
        trace,
        metadata.print_arguments,
        metadata.print_result};
    const auto result =
        lease.capture(event, ::tracer::compat::span(captured.schemas.data(), captured.schema_count),
                      ::tracer::compat::span(captured.states.data(), captured.state_count));
    if (!result) {
      runtime.capture_drops_.fetch_add(1, std::memory_order_relaxed);
      report_capture_drop(trace, capture_status_name(result.status));
      return false;
    }
    return true;
  };
  if (hook.access)
    return capture(*hook.access);

  // Event-only hooks and hooks that have finished a visitor use a fresh
  // complete snapshot interval. Capture storage grows with the event request.
  bool captured = false;
  try {
    context_execution::AccessLease lease_guard;
    if (!lease_guard.entered())
      engine_error(context_engine::ErrorCode::lifecycle_access);
    {
      context_engine::HookAccess snapshot(
          *hook.context, ::tracer::compat::span<const context_engine::SlotRequest>{}, true,
          context_engine::HookAccess::Kind::independent);
      captured = capture(snapshot);
    }
  } catch (...) {
    runtime.capture_drops_.fetch_add(1, std::memory_order_relaxed);
    report_capture_drop(trace, "capture_snapshot_exception");
    captured = false;
  }
  context_engine::drain_lifecycle();
  return captured;
}

std::uint64_t SharedSourceRuntime::native_thread_id(void *opaque) noexcept {
  (void)opaque;
  return static_cast<std::uint64_t>(
      std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

} // namespace tracer::context_runtime

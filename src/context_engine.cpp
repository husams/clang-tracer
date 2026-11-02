#include "context_engine.hpp"

#include <algorithm>
#include <optional>
#include <utility>

#include "context_execution.hpp"
#include "tracer/compat.hpp"

namespace tracer::context_engine {
namespace {
std::atomic<Identity> next_identity{1};
// One short admission gate serializes closure and whole-chain reservations.
// No holder waits for a payload mutex, allocates, calls user code or finalizes.
std::mutex admission_gate;

Identity identity() noexcept {
  return next_identity.fetch_add(1, std::memory_order_relaxed);
}

struct Execution {
  Identity domain = identity();
  unsigned guards = 0;
  bool lifecycle = false;
  std::unique_ptr<ScopeRecord> retired;
  ~Execution();
};

thread_local Execution execution;

class Lifecycle final {
public:
  Lifecycle() noexcept : previous_(std::exchange(execution.lifecycle, true)) {
    if (context_execution::current().mode !=
        context_execution::Mode::LifecycleNoTrace)
      mode_.emplace(context_execution::Mode::LifecycleNoTrace);
  }

  ~Lifecycle() { execution.lifecycle = previous_; }

  Lifecycle(const Lifecycle &) = delete;
  Lifecycle &operator=(const Lifecycle &) = delete;

private:
  bool previous_;
  std::optional<context_execution::Scope> mode_;
};

[[noreturn]] void fail(ErrorCode code) { throw Error(code); }

bool invoke_owner(const ScopeRegistration &registration,
                  void (*function)(void *), void *argument) noexcept {
  if (registration.invoke)
    return registration.invoke(registration.invocation_context, function,
                               argument);
  function(argument);
  return true;
}
void destroy_payload(const ScopeRegistration &registration,
                     const StateRegistration &state,
                     std::any &payload) noexcept {
  struct Args {
    const StateRegistration &state;
    std::any &payload;
  } args{state, payload};
  const auto destroy = [](void *opaque) {
    auto &args = *static_cast<Args *>(opaque);
    args.state.destroy(args.payload);
    // Make the lifetime boundary explicit even for owner managers whose
    // destroy thunk only destroys an in-place value.
    args.payload.reset();
  };
  if (registration.invoke_cleanup) {
    // Cleanup uses a separately admitted private boundary: unlike construction
    // or callback entry it must remain available when all throw-capable lanes
    // are occupied by application calls.
    (void)registration.invoke_cleanup(registration.invocation_context,
                                      destroy, &args);
    return;
  }
  invoke_owner(registration, destroy, &args);
}
void *payload_address(const ScopeRegistration &registration,
                      const StateRegistration &state,
                      std::any &payload) noexcept {
  struct Args {
    const StateRegistration &state;
    std::any &payload;
    void *result = nullptr;
  } args{state, payload};
  return invoke_owner(
             registration,
             [](void *opaque) {
               auto &args = *static_cast<Args *>(opaque);
               args.result = args.state.address(args.payload);
             },
             &args)
             ? args.result
             : nullptr;
}
} // namespace

class ScopeRecord final {
public:
  struct Slot {
    std::any payload;
    std::mutex mutex;
  };

  // Declared first, released last, after erased payload managers have returned.
  const std::shared_ptr<const ScopeRegistration> registration;
  const Identity id = identity();
  const Identity domain = execution.domain;
  std::atomic<bool> closed{false};
  bool confined = false;
  std::size_t outstanding_tasks = 0; // Protected by admission_gate.
  std::vector<std::shared_ptr<ScopeRecord>> ancestry;
  std::unique_ptr<ScopeRecord> retired_next;
  std::array<Slot, kMaxSlots> slots;
  std::size_t constructed = 0;

  explicit ScopeRecord(std::shared_ptr<const ScopeRegistration> r)
      : registration(std::move(r)) {}

  ~ScopeRecord() noexcept {
    // This destructor and its deleter are native; no module can unload while
    // its finalizer or erased std::any manager is still returning from JIT
    // code.
    Lifecycle lifecycle;
    while (constructed) {
      --constructed;
      destroy_payload(*registration, registration->states[constructed],
                      slots[constructed].payload);
      slots[constructed].payload.reset();
    }
  }

  std::size_t index(Identity state) const {
    for (std::size_t i = 0; i < registration->states.size(); ++i) {
      if (registration->states[i].identity == state) {
        return i;
      }
    }
    fail(ErrorCode::incompatible_registration);
  }

  void check_domain() const {
    if (confined && domain != execution.domain) {
      fail(ErrorCode::wrong_domain);
    }
  }
};

bool Context::permits_cross_domain_capture() const noexcept {
  if (domain_ != execution.domain)
    return false;
  for (std::size_t i = 0; i < size_; ++i) {
    const auto &binding = bindings_[i];
    if (!binding.record || binding.retired)
      continue;
    if (binding.record->confined)
      return false;
    for (const auto &ancestor : binding.record->ancestry)
      if (ancestor->confined)
        return false;
  }
  return true;
}

namespace {
void drain_retired() noexcept {
  while (!execution.guards && execution.retired &&
         !context_execution::current().access_depth &&
         !context_execution::current().control_depth) {
    auto record = std::move(execution.retired);
    execution.retired = std::move(record->retired_next);
    record.reset();
  }
}

Execution::~Execution() { drain_retired(); }

void retire(ScopeRecord *record) noexcept {
  auto owned = std::unique_ptr<ScopeRecord>(record);
  if (execution.guards || context_execution::current().access_depth ||
      context_execution::current().control_depth) {
    owned->retired_next = std::move(execution.retired);
    execution.retired = std::move(owned);
  }
}

void validate(const ScopeRegistration &registration) {
  if (!registration.identity || !registration.owner_code ||
      registration.states.empty()) {
    fail(ErrorCode::incompatible_registration);
  }
  if (registration.states.size() > kMaxSlots) {
    fail(ErrorCode::capacity);
  }
  for (std::size_t i = 0; i < registration.states.size(); ++i) {
    const auto &s = registration.states[i];
    if (!s.identity || !s.construct || !s.destroy || !s.address) {
      fail(ErrorCode::incompatible_registration);
    }
    for (std::size_t j = 0; j < i; ++j) {
      if (registration.states[j].identity == s.identity) {
        fail(ErrorCode::incompatible_registration);
      }
    }
  }
}
} // namespace

void drain_lifecycle() noexcept { drain_retired(); }
void prepare_thread_context() noexcept { (void)execution; }

const char *Error::what() const noexcept {
  static constexpr const char *names[] = {"ok",
                                          "missing_state",
                                          "incompatible_registration",
                                          "wrong_domain",
                                          "capacity",
                                          "invalid_frame",
                                          "active_access",
                                          "unplanned_access",
                                          "borrowed_end",
                                          "repeated_end",
                                          "end_order",
                                          "construction_failed",
                                          "no_active_hook",
                                          "lifecycle_access"};
  const auto i = static_cast<unsigned>(code_);
  return i < static_cast<unsigned>(ErrorCode::count) ? names[i]
                                                     : "unknown_context_error";
}

void Diagnostics::count(ErrorCode code) noexcept {
  const auto i = static_cast<unsigned>(code);
  if (i < counts_.size()) {
    counts_[i].fetch_add(1, std::memory_order_relaxed);
  }
}

std::uint64_t Diagnostics::count_for(ErrorCode code) const noexcept {
  const auto i = static_cast<unsigned>(code);
  return i < counts_.size() ? counts_[i].load(std::memory_order_relaxed) : 0;
}

Context::Context(Diagnostics &diagnostics) noexcept
    : diagnostics_(diagnostics), domain_(execution.domain) {}

Context::~Context() = default;

void Context::check_domain() const {
  if (domain_ != execution.domain) {
    fail(ErrorCode::wrong_domain);
  }
}

void Context::check_lifecycle() const {
  check_domain();
  if (execution.lifecycle) {
    fail(ErrorCode::lifecycle_access);
  }
}

InvocationFrame::InvocationFrame(Context &context)
    : context_(context), parent_(context.frame_), saved_(context.size_) {
  context.check_lifecycle();
  if (execution.guards || context.hook_) {
    fail(ErrorCode::active_access);
  }
  context.frame_ = this;
}

InvocationFrame::~InvocationFrame() noexcept { pop_all_and_restore(); }

void InvocationFrame::pop_all_and_restore() noexcept {
  if (!restored_) {
    restored_ = context_.restore(*this);
  }
}

void InvocationFrame::create_or_attach(
    std::shared_ptr<const ScopeRegistration> registration,
    const ScopeInit &init) {
  try {
    context_.push(*this, std::move(registration), init, false);
  } catch (...) {
    // A failure in the second dependency must roll back the entire constructor,
    // not just its last push. A later frame destructor is an idempotent
    // fallback.
    pop_all_and_restore();
    throw;
  }
}

void Context::push(InvocationFrame &frame,
                   std::shared_ptr<const ScopeRegistration> registration,
                   const ScopeInit &init, bool fresh) {
  check_lifecycle();
  if (execution.guards) {
    fail(ErrorCode::active_access);
  }
  if (&frame != frame_ || frame.restored_) {
    fail(ErrorCode::invalid_frame);
  }
  if (!registration) {
    fail(ErrorCode::incompatible_registration);
  }
  validate(*registration);
  if (!fresh) {
    for (std::size_t i = frame.saved_; i < size_; ++i) {
      const auto &b = bindings_[i];
      if (b.scope == registration->identity) {
        if (b.retired || b.pending) {
          fail(ErrorCode::repeated_end);
        }
        if (b.record->registration != registration) {
          fail(ErrorCode::incompatible_registration);
        }
        return; // Same-scope dependencies in this invocation are deduplicated.
      }
    }
  }
  if (size_ == kMaxBindings) {
    fail(ErrorCode::capacity);
  }
  std::shared_ptr<ScopeRecord> record;
  if (!fresh) {
    for (std::size_t i = size_; i; --i) {
      const auto &b = bindings_[i - 1];
      if (!b.record || b.retired ||
          b.record->registration->identity != registration->identity) {
        continue;
      }
      if (b.record->registration != registration) {
        fail(ErrorCode::incompatible_registration);
      }
      b.record->check_domain();
      // An existing binding is an already accepted execution capability.
      // Attaching during that work does not reopen propagation admission.
      record = b.record;
      break;
    }
  }
  const bool owned = !record;
  if (owned) {
    record = std::shared_ptr<ScopeRecord>(
        std::make_unique<ScopeRecord>(registration).release(), retire);
    // Descendants pin whole ancestor records. Metadata preparation precedes
    // user construction and publication; allocation failure changes no cursor.
    record->ancestry.reserve(std::min(size_, kMaxSlots));
    for (std::size_t i = 0; i < size_; ++i) {
      const auto &b = bindings_[i];
      if (!b.record || b.retired ||
          std::find(record->ancestry.begin(), record->ancestry.end(),
                    b.record) != record->ancestry.end()) {
        continue;
      }
      if (record->ancestry.size() == kMaxSlots) {
        fail(ErrorCode::capacity);
      }
      record->ancestry.push_back(b.record);
    }
    Lifecycle lifecycle;
    for (const auto &state : registration->states) {
      record->confined |= state.policy == Policy::confined;
      auto &slot = record->slots[record->constructed];
      struct Args {
        const StateRegistration &state;
        const ScopeInit &init;
        std::any &payload;
        ErrorCode status = ErrorCode::construction_failed;
      } args{state, init, slot.payload};
      invoke_owner(
          *registration,
          [](void *opaque) {
            auto &args = *static_cast<Args *>(opaque);
            args.status = args.state.construct(args.init, args.payload);
          },
          &args);
      const auto status = args.status;
      if (status != ErrorCode::ok) {
        if (slot.payload.has_value()) {
          destroy_payload(*registration, state, slot.payload);
        }
        slot.payload.reset();
        fail(status);
      }
      ++record->constructed;
    }
  }
  // This is the only publication point; no throwing operation follows it.
  bindings_[size_++] = {identity(), registration->identity,
                        &frame,     std::move(record),
                        owned,      false,
                        false};
}

bool Context::restore(InvocationFrame &frame) noexcept {
  if (frame_ != &frame || size_ < frame.saved_) {
    diagnostics_.count(ErrorCode::invalid_frame);
    return false;
  }
  while (size_ > frame.saved_) {
    auto &binding = bindings_[--size_];
    if (binding.record && binding.owned) {
      std::lock_guard gate(admission_gate);
      binding.record->closed.store(true, std::memory_order_release);
    }
    binding = {};
  }
  frame_ = frame.parent_;
  return true;
}

std::shared_ptr<ScopeRecord>
Context::resolve(const SlotRequest &request) const {
  check_lifecycle();
  if (!request.registration) {
    fail(ErrorCode::incompatible_registration);
  }
  for (std::size_t i = size_; i; --i) {
    const auto &b = bindings_[i - 1];
    if (!b.record || b.retired ||
        b.record->registration->identity != request.registration->identity) {
      continue;
    }
    if (b.record->registration != request.registration) {
      fail(ErrorCode::incompatible_registration);
    }
    b.record->check_domain();
    (void)b.record->index(request.state);
    return b.record;
  }
  fail(ErrorCode::missing_state);
}

void HookAccess::add(std::shared_ptr<ScopeRecord> record, Identity state) {
  record->check_domain();
  const auto slot = record->index(state);
  for (std::size_t i = 0; i < size_; ++i) {
    if (entries_[i].record == record && entries_[i].slot == slot) {
      return;
    }
  }
  if (size_ == kMaxSlots) {
    fail(ErrorCode::capacity);
  }
  entries_[size_].record = std::move(record);
  entries_[size_++].slot = slot;
}

HookAccess::HookAccess(Context& context, ::tracer::compat::span<const SlotRequest> requests,
                       bool all_visible, Kind kind)
    : context_(context)
    , kind_(kind) {
  context.check_lifecycle();
  if (!context.hook_) {
    fail(ErrorCode::no_active_hook);
  }
  if (execution.guards || context.access_) {
    fail(ErrorCode::active_access);
  }
  for (const auto &request : requests) {
    add(context.resolve(request), request.state);
  }
  if (all_visible) {
    std::array<Identity, kMaxSlots> seen{};
    std::size_t seen_count = 0;
    for (std::size_t i = context.size_; i; --i) {
      const auto &b = context.bindings_[i - 1];
      if (!b.record || b.retired) {
        continue;
      }
      for (const auto &s : b.record->registration->states) {
        if (std::find(seen.begin(), seen.begin() + seen_count, s.identity) !=
            seen.begin() + seen_count) {
          continue;
        }
        if (seen_count == kMaxSlots) {
          fail(ErrorCode::capacity);
        }
        seen[seen_count++] = s.identity;
        add(b.record, s.identity);
      }
    }
  }
  std::sort(entries_.begin(), entries_.begin() + size_,
            [](const Entry &a, const Entry &b) {
              return a.record->id < b.record->id ||
                     (a.record->id == b.record->id && a.slot < b.slot);
            });
  try {
    for (std::size_t i = 0; i < size_; ++i) {
      auto &entry = entries_[i];
      if (entry.record->registration->states[entry.slot].policy ==
          Policy::shared) {
        entry.lock = std::unique_lock(entry.record->slots[entry.slot].mutex);
      }
    }
  } catch (...) {
    // Unlock the complete partial set BEFORE any member pin can be destroyed.
    for (std::size_t i = size_; i; --i) {
      if (entries_[i - 1].lock.owns_lock()) {
        entries_[i - 1].lock.unlock();
      }
    }
    throw;
  }
  ++execution.guards;
  context.access_ = this;
}

HookAccess::~HookAccess() noexcept {
  for (std::size_t i = size_; i; --i) {
    if (entries_[i - 1].lock.owns_lock()) {
      entries_[i - 1].lock.unlock();
    }
  }
  context_.access_ = nullptr;
  --execution.guards;
  for (std::size_t i = size_; i; --i) {
    entries_[i - 1].record.reset();
  }
  drain_retired();
}

void *HookAccess::ref(const SlotRequest &request) const {
  ErrorCode status = ErrorCode::ok;
  auto *result = try_ref(request, status);
  if (!result)
    fail(status == ErrorCode::ok ? ErrorCode::unplanned_access : status);
  return result;
}

void *HookAccess::try_ref(const SlotRequest &request,
                          ErrorCode &status) const noexcept {
  status = ErrorCode::ok;
  if (context_.domain_ != execution.domain) {
    status = ErrorCode::wrong_domain;
    return nullptr;
  }
  if (execution.lifecycle) {
    status = ErrorCode::lifecycle_access;
    return nullptr;
  }
  for (std::size_t i = 0; i < size_; ++i) {
    const auto &e = entries_[i];
    const auto &s = e.record->registration->states[e.slot];
    if (e.record->registration == request.registration &&
        s.identity == request.state) {
      auto *address = payload_address(*e.record->registration, s,
                                      e.record->slots[e.slot].payload);
      if (!address) {
        status = ErrorCode::incompatible_registration;
        return nullptr;
      }
      return address;
    }
  }
  status = ErrorCode::unplanned_access;
  return nullptr;
}

bool HookAccess::visit_all(Visit visitor, void *argument) const noexcept {
  if (!visitor || context_.access_ != this || execution.lifecycle)
    return false;
  for (std::size_t i = 0; i < size_; ++i) {
    const auto &entry = entries_[i];
    const auto &registration = *entry.record->registration;
    const auto &state = registration.states[entry.slot];
    auto *payload = payload_address(registration, state,
                                    entry.record->slots[entry.slot].payload);
    if (!payload)
      return false;
    visitor(argument, entry.record->id, state.identity, registration, payload);
  }
  return true;
}

void *Context::get(const SlotRequest &request) {
  ErrorCode status = ErrorCode::ok;
  auto *result = try_get(request, status);
  if (!result)
    fail(status == ErrorCode::ok ? ErrorCode::unplanned_access : status);
  return result;
}

void *Context::try_get(const SlotRequest &request,
                       ErrorCode &status) noexcept {
  status = ErrorCode::ok;
  if (domain_ != execution.domain) {
    status = ErrorCode::wrong_domain;
    return nullptr;
  }
  if (execution.lifecycle) {
    status = ErrorCode::lifecycle_access;
    return nullptr;
  }
  if (!access_ || access_->kind_ != HookAccess::Kind::direct) {
    status = ErrorCode::unplanned_access;
    return nullptr;
  }
  return access_->try_ref(request, status);
}

void Context::with(const SlotRequest &request, void (*visitor)(void *, void *),
                   void *argument) {
  check_lifecycle();
  if (!hook_) {
    fail(ErrorCode::no_active_hook);
  }
  if (!visitor) {
    fail(ErrorCode::incompatible_registration);
  }
  if (access_) {
    if (access_->kind_ != HookAccess::Kind::direct) {
      fail(ErrorCode::active_access);
    }
    visitor(access_->ref(request), argument);
    return;
  }
  HookAccess access(*this, ::tracer::compat::span(&request, 1), false,
                    HookAccess::Kind::independent);
  visitor(access.ref(request), argument);
}

void Context::init_scope(std::shared_ptr<const ScopeRegistration> registration,
                         const ScopeInit &init) {
  check_lifecycle();
  if (!hook_) {
    fail(ErrorCode::no_active_hook);
  }
  if (!frame_) {
    fail(ErrorCode::invalid_frame);
  }
  push(*frame_, std::move(registration), init, true);
}

void Context::end_scope(Identity scope) {
  const auto status = try_end_scope(scope);
  if (status != ErrorCode::ok)
    fail(status);
}

ErrorCode Context::try_end_scope(Identity scope) noexcept {
  if (domain_ != execution.domain) return ErrorCode::wrong_domain;
  if (execution.lifecycle) return ErrorCode::lifecycle_access;
  if (!hook_) return ErrorCode::no_active_hook;
  for (std::size_t i = size_; i; --i) {
    auto &b = bindings_[i - 1];
    // Retired tombstones keep repeated requests from selecting an ancestor.
    if (b.scope != scope) {
      continue;
    }
    if (b.retired || b.pending) {
      return ErrorCode::repeated_end;
    }
    if (b.frame != frame_ || !b.owned) {
      return ErrorCode::borrowed_end;
    }
    for (std::size_t j = i; j < size_; ++j) {
      if (bindings_[j].owned && !bindings_[j].retired &&
          !bindings_[j].pending) {
        return ErrorCode::end_order;
      }
    }
    b.pending = true;
    {
      std::unique_lock gate(admission_gate, std::try_to_lock);
      if (!gate.owns_lock()) {
        b.pending = false;
        return ErrorCode::active_access;
      }
      b.record->closed.store(true, std::memory_order_release);
    }
    return ErrorCode::ok;
  }
  return ErrorCode::missing_state;
}

void Context::commit_ends() noexcept {
  for (std::size_t i = size_; i; --i) {
    auto &b = bindings_[i - 1];
    if (!b.pending || b.retired) {
      continue;
    }
    b.pending = false;
    b.retired = true;
    // Keep a non-owning tombstone identity without retaining closed payloads.
    b.record.reset();
  }
}

HookExit::HookExit(Context &context, Publish publish, void *argument) noexcept
    : context_(context), publish_(publish), argument_(argument) {
  if (execution.lifecycle || execution.domain != context.domain_ ||
      context.hook_ || execution.guards) {
    context.diagnostics_.count(ErrorCode::no_active_hook);
    return;
  }
  context.hook_ = this;
  installed_ = true;
}

HookExit::~HookExit() noexcept {
  if (!installed_) {
    return;
  }
  if (context_.access_ || execution.guards) {
    context_.diagnostics_.count(ErrorCode::active_access);
    context_.hook_ = nullptr;
    return;
  }
  context_.hook_ = nullptr;
  context_.commit_ends();
  if (publish_) {
    publish_(argument_, failed_);
  }
}

std::size_t PropagationLedger::outstanding() const noexcept {
  std::lock_guard gate(admission_gate);
  return tasks_;
}

std::shared_ptr<TaskCapture>
TaskCapture::capture(Context &context,
                     std::shared_ptr<PropagationLedger> ledger) noexcept {
  // Reserve queued AND running metadata at admission, including the full
  // worker-context restoration frame and shared ownership bookkeeping.
  constexpr std::size_t charge =
      sizeof(TaskCapture) + sizeof(TaskAttachment) + 64;
  try {
    context.check_lifecycle();
    if (execution.guards) {
      fail(ErrorCode::active_access);
    }
    if (!ledger) {
      fail(ErrorCode::incompatible_registration);
    }
    auto result = std::make_shared<TaskCapture>(std::move(ledger));
    const auto add = [&](const std::shared_ptr<ScopeRecord> &record) {
      if (std::find(result->records_.begin(),
                    result->records_.begin() + result->size_,
                    record) != result->records_.begin() + result->size_) {
        return;
      }
      if (result->size_ >=
          std::min(kMaxSlots, result->ledger_->limits_.ancestry)) {
        fail(ErrorCode::capacity);
      }
      result->records_[result->size_++] = record;
    };
    for (std::size_t i = 0; i < context.size_; ++i) {
      const auto &b = context.bindings_[i];
      if (!b.record || b.retired) {
        continue;
      }
      for (const auto &ancestor : b.record->ancestry) {
        add(ancestor);
      }
      add(b.record);
    }
    {
      std::lock_guard gate(admission_gate);
      auto &budget = *result->ledger_;
      if (budget.tasks_ >= budget.limits_.tasks ||
          charge > budget.limits_.bytes -
                       std::min(budget.bytes_, budget.limits_.bytes)) {
        fail(ErrorCode::capacity);
      }
      for (std::size_t i = 0; i < result->size_; ++i) {
        const auto &record = result->records_[i];
        if (record->closed.load(std::memory_order_relaxed)) {
          fail(ErrorCode::repeated_end);
        }
        if (record->outstanding_tasks >= budget.limits_.per_scope) {
          fail(ErrorCode::capacity);
        }
      }
      ++budget.tasks_;
      budget.bytes_ += charge;
      for (std::size_t i = 0; i < result->size_; ++i) {
        ++result->records_[i]->outstanding_tasks;
      }
      result->phase_.store(Phase::queued, std::memory_order_release);
    }
    return result;
  } catch (const Error &error) {
    context.diagnostics_.count(error.code());
  } catch (...) {
    context.diagnostics_.count(ErrorCode::capacity);
  }
  return {};
}

TaskCapture::~TaskCapture() noexcept { cancel_before_start(); }

bool TaskCapture::cancel_before_start() noexcept {
  auto expected = Phase::queued;
  if (!phase_.compare_exchange_strong(expected, Phase::terminal,
                                      std::memory_order_acq_rel)) {
    return false;
  }
  release();
  return true;
}

void TaskCapture::finish() noexcept {
  auto expected = Phase::running;
  if (phase_.compare_exchange_strong(expected, Phase::terminal,
                                     std::memory_order_acq_rel)) {
    release();
  }
}

void TaskCapture::release() noexcept {
  std::array<std::shared_ptr<ScopeRecord>, kMaxSlots> released;
  {
    std::lock_guard gate(admission_gate);
    --ledger_->tasks_;
    ledger_->bytes_ -= sizeof(TaskCapture) + sizeof(TaskAttachment) + 64;
    for (std::size_t i = 0; i < size_; ++i) {
      --records_[i]->outstanding_tasks;
      released[i] = std::move(records_[i]);
    }
    size_ = 0;
  } // Potential final release happens after the admission gate has released.
}

TaskAttachment::TaskAttachment(Context &context,
                               std::shared_ptr<TaskCapture> task)
    : context_(context), task_(std::move(task)) {
  context.check_lifecycle();
  if (execution.guards || context.hook_) {
    fail(ErrorCode::active_access);
  }
  if (!task_) {
    fail(ErrorCode::invalid_frame);
  }
  auto expected = TaskCapture::Phase::queued;
  if (!task_->phase_.compare_exchange_strong(
          expected, TaskCapture::Phase::running, std::memory_order_acq_rel)) {
    fail(ErrorCode::invalid_frame);
  }
  try {
    // Validate the COMPLETE chain before publishing one worker binding.
    for (std::size_t i = 0; i < task_->size_; ++i) {
      task_->records_[i]->check_domain();
    }
    saved_size_ = context.size_;
    saved_frame_ = context.frame_;
    for (std::size_t i = 0; i < saved_size_; ++i) {
      saved_[i] = std::move(context.bindings_[i]);
    }
    context.size_ = 0;
    context.frame_ = nullptr;
    for (std::size_t i = 0; i < task_->size_; ++i) {
      const auto &record = task_->records_[i];
      context.bindings_[context.size_++] = {
          identity(), record->registration->identity,
          nullptr,    record,
          false,      false,
          false};
    }
  } catch (...) {
    task_->finish();
    throw;
  }
}

TaskAttachment::~TaskAttachment() noexcept {
  // Generated scheduler wrappers destroy all task invocation frames first.
  if (context_.frame_ || context_.hook_ || execution.guards) {
    std::terminate();
  }
  while (context_.size_) {
    context_.bindings_[--context_.size_] = {};
  }
  context_.frame_ = saved_frame_;
  for (std::size_t i = 0; i < saved_size_; ++i) {
    context_.bindings_[i] = std::move(saved_[i]);
  }
  context_.size_ = saved_size_;
  task_->finish();
}

} // namespace tracer::context_engine

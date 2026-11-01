#pragma once

// Private callback-JIT facade. The public tracer::Context stays stateless;
// every operation reaches the owner through this opaque per-generation table.
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "context_analysis.hpp"
#include "context_execution.hpp"
#include "tracer/callback.hpp"
#include "tracer/context.hpp"

namespace tracer::context_facade {

using StateId = std::uint64_t;
using ScopeId = std::uint64_t;

// Signals a completed hook whose user callback failed. The generated
// per-hook wrapper catches this after HookExit has committed cleanup and
// retained events, then reports a failed PRE so dispatch can continue with
// the original target without running later hooks or POST.
struct HookFailure final {};
// Small private exception used only after a no-throw native status return.
// It carries no message and never asks the host standard library to allocate.
struct ContextError final { unsigned code; };
using Opaque = void *;
using Status = unsigned;

struct RequestDescriptor final {
  StateId state_id = 0;
  ScopeId scope_id = 0;
  const char *state_type = nullptr;
  const char *scope_type = nullptr;
  const char *payload_type = nullptr;
  bool generic = false;
  bool shared = false;
};

enum class AccessKind : std::uint8_t { independent };

// Function pointers are installed by the native agent for one pinned code
// generation. No owner object, std::any, shared_ptr, or payload crosses this
// table. Functions may throw engine errors except where marked noexcept.
struct NativeFunctionTable final {
  std::uint32_t abi_version = 1;
  Opaque (*current_context)(Opaque generation_runtime) = nullptr;
  Opaque (*frame_enter)(Opaque generation_runtime) = nullptr;
  void (*frame_exit)(Opaque generation_runtime, Opaque frame) noexcept = nullptr;
  void (*create_or_attach)(Opaque generation_runtime, Opaque frame,
                           StateId state) = nullptr;
  Opaque (*hook_begin)(Opaque generation_runtime, const void *call,
                       const TraceContext *trace_context,
                       const RequestDescriptor *requests, std::size_t count,
                       bool all_visible_events, bool direct) = nullptr;
  void (*hook_end)(Opaque generation_runtime, Opaque hook,
                   bool callback_failed) noexcept = nullptr;
  Opaque (*access_begin)(Opaque generation_runtime,
                         const RequestDescriptor *requests, std::size_t count,
                         AccessKind kind, Status *status) = nullptr;
  void (*access_end)(Opaque generation_runtime, Opaque access) noexcept = nullptr;
  Status (*get)(Opaque generation_runtime, StateId state,
                void **result) noexcept = nullptr;
  void (*init_scope)(Opaque generation_runtime, ScopeId scope, StateId payload,
                     const void *source) = nullptr;
  Status (*end_scope)(Opaque generation_runtime, ScopeId scope) noexcept = nullptr;
  bool (*event)(Opaque generation_runtime, const TraceContext *trace_context,
                std::string_view name, std::size_t max_items) noexcept = nullptr;
  std::uint64_t (*thread_id)(Opaque generation_runtime) noexcept = nullptr;
};

struct GenerationBinding final {
  const NativeFunctionTable *table = nullptr;
  Opaque runtime = nullptr;
};

// Native dispatch installs this address in context_execution::CurrentHook for
// each tracking invocation. LifecycleNoTrace masks it; cleanup still has the
// generation binding retained by its generated InvocationFrame.
struct NativeHookContext final {
  static constexpr std::uint64_t kMagic = 0x435458484f4f4b31ULL;
  std::uint64_t magic = kMagic;
  GenerationBinding generation{};
  const void *call = nullptr;
  const TraceContext *trace_context = nullptr;
  void *hook_state = nullptr;
};

struct NativeHookContextFactory final {
  static const TraceContext *trace(const tracer::Call &call) noexcept {
    return &call.context_;
  }
};

bool valid(const NativeFunctionTable &) noexcept;
std::uint64_t stable_identity(std::string_view lineage,
                              std::string_view canonical_type) noexcept;
const NativeHookContext *active_native_hook() noexcept;
const GenerationBinding *active_generation() noexcept;
void require_valid(const NativeFunctionTable &);
inline const GenerationBinding &require_active_generation() {
  const auto *binding = active_generation();
  if (!binding) throw ContextError{1U};
  return *binding;
}
void emit_event(std::string_view, std::size_t) noexcept;
std::uint64_t active_thread_id() noexcept;

// These traits are specialized in an engine-owned compilation copy after all
// user declarations and before generated hook wrappers are emitted.
template <class T> struct StateTraits;
template <class Scope> struct ScopeTraits;
template <class Scope, class Payload> struct PayloadTraits;

template <class Scope, class Payload>
struct PayloadAccess {};

template <class T> struct DescriptorFor {
  static RequestDescriptor get() noexcept { return StateTraits<T>::descriptor; }
};
template <class Scope, class Payload> struct DescriptorFor<PayloadAccess<Scope, Payload>> {
  static RequestDescriptor get() noexcept {
    return PayloadTraits<Scope, Payload>::descriptor;
  }
};

template <class T> T *request() {
  const auto *binding = active_generation();
  if (!binding) throw ContextError{1U};
  void *pointer = nullptr;
  const auto status = binding->table->get(
      binding->runtime, StateTraits<T>::descriptor.state_id, &pointer);
  if (status != 0 || !pointer)
    throw ContextError{status ? status : 1U};
  return static_cast<T *>(pointer);
}

template <class Scope, class Payload> Payload *request_payload() {
  const auto *binding = active_generation();
  if (!binding) throw ContextError{1U};
  void *pointer = nullptr;
  const auto status = binding->table->get(
      binding->runtime, PayloadTraits<Scope, Payload>::descriptor.state_id,
      &pointer);
  if (status != 0 || !pointer)
    throw ContextError{status ? status : 1U};
  return static_cast<Payload *>(pointer);
}

class AccessInterval final {
public:
  AccessInterval(const GenerationBinding &binding,
                 const RequestDescriptor &request, AccessKind kind)
      : binding_(binding), active_(true) {
    if (!binding.table || !binding.runtime)
      throw ContextError{1U};
    Status status = 0;
    token_ = binding.table->access_begin(binding.runtime, &request, 1, kind,
                                         &status);
    if (!token_) throw ContextError{status ? status : 1U};
  }
  ~AccessInterval() noexcept {
    if (active_ && token_)
      binding_.table->access_end(binding_.runtime, token_);
  }
  AccessInterval(const AccessInterval &) = delete;
  AccessInterval &operator=(const AccessInterval &) = delete;
private:
  GenerationBinding binding_{};
  Opaque token_ = nullptr;
  bool active_ = false;
};

template <class T, class Visitor> void with_state(Visitor &&visitor) {
  const auto descriptor = StateTraits<T>::descriptor;
  const auto *binding = active_generation();
  if (!binding) throw ContextError{1U};
  AccessInterval interval(*binding, descriptor, AccessKind::independent);
  std::invoke(std::forward<Visitor>(visitor), *request<T>());
}

template <class Scope, class Payload, class Visitor>
void with_payload(Visitor &&visitor) {
  const auto descriptor = PayloadTraits<Scope, Payload>::descriptor;
  const auto *binding = active_generation();
  if (!binding) throw ContextError{1U};
  AccessInterval interval(*binding, descriptor, AccessKind::independent);
  std::invoke(std::forward<Visitor>(visitor), *request_payload<Scope, Payload>());
}

class InvocationFrame final {
public:
  explicit InvocationFrame(const GenerationBinding &binding)
      : binding_(binding) {
    if (!binding.table || !binding.runtime)
      throw ContextError{1U};
    token_ = binding.table->frame_enter(binding.runtime);
    if (!token_) throw ContextError{1U};
  }
  // The public Context object is stateless. The native dispatcher installs
  // the generation binding in native TLS before lifecycle entrypoints run.
  explicit InvocationFrame(tracer::Context &context)
      : InvocationFrame(require_active_generation()) { (void)context; }
  ~InvocationFrame() noexcept { pop_all_and_restore(); }
  InvocationFrame(const InvocationFrame &) = delete;
  InvocationFrame &operator=(const InvocationFrame &) = delete;
  InvocationFrame(InvocationFrame &&) = delete;
  InvocationFrame &operator=(InvocationFrame &&) = delete;
  template <class T> void push_create_or_attach() {
    if (!token_)
      throw ContextError{1U};
    binding_.table->create_or_attach(
        binding_.runtime, token_, StateTraits<T>::descriptor.state_id);
  }
  void pop_all_and_restore() noexcept {
    if (!token_) return;
    binding_.table->frame_exit(binding_.runtime, token_);
    token_ = nullptr;
  }
private:
  GenerationBinding binding_{};
  Opaque token_ = nullptr;
};

// Entry templates live in the callback module. The hook boundary catches only
// callback exceptions; begin/acquisition errors propagate to native dispatch.
template <class... Requests, class Callback>
void run_direct_hook(const tracer::Call &call, bool all_visible_events,
                     Callback &&callback) {
  const auto *active = active_generation();
  if (!active) throw ContextError{1U};
  const auto binding = *active;
  const RequestDescriptor descriptors[] = {DescriptorFor<Requests>::get()...};
  constexpr std::size_t count = sizeof...(Requests);
  const auto token = binding.table->hook_begin(
      binding.runtime, &call, NativeHookContextFactory::trace(call),
      descriptors, count, all_visible_events, true);
  bool failed = false;
  try {
    std::invoke(std::forward<Callback>(callback));
  } catch (...) {
    failed = true;
  }
  binding.table->hook_end(binding.runtime, token, failed);
  if (failed) throw HookFailure{};
}

template <class Callback>
void run_short_hook(const tracer::Call &call, bool all_visible_events,
                    Callback &&callback) {
  const auto *active = active_generation();
  if (!active) throw ContextError{1U};
  const auto binding = *active;
  const auto token = binding.table->hook_begin(
      binding.runtime, &call, NativeHookContextFactory::trace(call),
      nullptr, 0, all_visible_events, false);
  bool failed = false;
  try {
    std::invoke(std::forward<Callback>(callback));
  } catch (...) {
    failed = true;
  }
  binding.table->hook_end(binding.runtime, token, failed);
  if (failed) throw HookFailure{};
}

template <class Callback>
void run_short_hook(const tracer::Call &call, Callback &&callback) {
  run_short_hook(call, false, std::forward<Callback>(callback));
}

template <class Scope> void init_scope() {
  const auto *binding = active_generation();
  if (!binding) throw ContextError{1U};
  binding->table->init_scope(binding->runtime, ScopeTraits<Scope>::id, 0, nullptr);
}
template <class Scope, class Payload> void init_scope(Payload &&payload) {
  const auto *binding = active_generation();
  if (!binding) throw ContextError{1U};
  binding->table->init_scope(
      binding->runtime, ScopeTraits<Scope>::id,
      PayloadTraits<Scope, std::remove_cvref_t<Payload>>::descriptor.state_id,
      static_cast<const void *>(std::addressof(payload)));
}
template <class Scope> void end_scope() {
  const auto *binding = active_generation();
  if (!binding) throw ContextError{1U};
  const auto status = binding->table->end_scope(
      binding->runtime, ScopeTraits<Scope>::id);
  if (status != 0) throw ContextError{status};
}

// The host calls this generation-local symbol after private ORC publication.
void bind_generation(const NativeFunctionTable *, Opaque runtime);
void clear_thread_generation() noexcept;
class ThreadBinding final {
public:
  ThreadBinding(const NativeFunctionTable *table, Opaque runtime) {
    bind_generation(table, runtime);
    active_ = true;
  }
  ~ThreadBinding() noexcept { if (active_) clear_thread_generation(); }
  ThreadBinding(const ThreadBinding &) = delete;
  ThreadBinding &operator=(const ThreadBinding &) = delete;
private:
  bool active_ = false;
};
const GenerationBinding *generation_binding() noexcept;
tracer::Context &current_context() noexcept;

struct FacadeIdentity final {
  ContextDependency dependency;
  StateId state_id = 0;
  ScopeId scope_id = 0;
};
struct GeneratedContextFacade final {
  std::string definitions;
  std::string bind_entrypoint;
  std::vector<RequestDescriptor> descriptors;
  std::vector<std::string> descriptor_strings;
  std::string error;
};
GeneratedContextFacade generate_context_facade(
    const std::string &facade_namespace,
    const std::string &bind_entrypoint,
    const std::vector<FacadeIdentity> &identities);

} // namespace tracer::context_facade

// Native entrypoints are inline and delegate to the documented helper imports;
// Context templates are emitted only after the generated type traits below the
// user declarations so no callback specialization is instantiated too early.
namespace tracer {
inline const Context &Call::context() const {
  return context_facade::current_context();
}
inline void Context::event(std::string_view name, std::size_t max_items) const {
  context_facade::emit_event(name, max_items);
}
inline std::uint64_t Context::thread_id() const noexcept {
  return context_facade::active_thread_id();
}
} // namespace tracer

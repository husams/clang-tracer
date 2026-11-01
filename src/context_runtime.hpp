#pragma once

#include "context_facade.hpp"
#include "context_registry.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace tracer::context_runtime {

// Stable native TLS surface shared by probes and the pthread adapter. Runtime
// contexts and native token arenas come from a bounded process-wide pool.
context_engine::Context *active_context() noexcept;
context_engine::Context &worker_context();
context_engine::Context *exchange_active_context(
    context_engine::Context *) noexcept;
std::size_t workspace_slots_in_use() noexcept;

struct EventMetadata final {
  std::string source_id;
  std::string task_id;
  std::string parent_task_id;
  std::string invocation_id;
  std::string probe_id;
  std::string opaque_metadata;
  std::uint64_t recording_position = 0;
  std::uint64_t epoch_schema_id = 1;
  std::uint32_t capture_hook_id = 0;
  bool print_arguments = false;
  bool print_result = false;
};

using DeliveryPublish = context_engine::ContextCapturePool::Lease::LeasedPublish;

// One immutable source generation, including its complete state registry
// generation, schema metadata and capture/delivery configuration.
class SharedSourceRuntime final {
public:
  struct StateEntry {
    context_engine::Identity scope = 0;
    context_engine::Identity state = 0;
    std::shared_ptr<const RegisteredContextScope> owner;
    const RegisteredContextState *registered = nullptr;
  };
  SharedSourceRuntime(
      std::vector<std::shared_ptr<const RegisteredContextScope>> scopes,
      context_engine::ContextCapturePool &capture_pool, EventMetadata metadata,
      DeliveryPublish publish, void *publish_context);
  SharedSourceRuntime(const SharedSourceRuntime &) = delete;
  SharedSourceRuntime &operator=(const SharedSourceRuntime &) = delete;

  const context_facade::NativeFunctionTable &function_table() const noexcept;
  const EventMetadata &metadata() const noexcept { return metadata_; }
  const StateEntry *find_state(context_engine::Identity) const noexcept;
  std::uint64_t capture_drops() const noexcept {
    return capture_drops_.load(std::memory_order_relaxed);
  }

  // Entered for the whole native PRE/original/POST transaction. Nested source
  // generations reuse the already active context and restore generation TLS.
  class Invocation final {
  public:
    explicit Invocation(SharedSourceRuntime &,
                        const tracer::Call *root_call = nullptr);
    ~Invocation() noexcept;
    Invocation(const Invocation &) = delete;
    Invocation &operator=(const Invocation &) = delete;
    Invocation(Invocation &&) = delete;
    Invocation &operator=(Invocation &&) = delete;
    bool entered() const noexcept;
  private:
    context_engine::Context *prior_context_ = nullptr;
    context_engine::Context *installed_context_ = nullptr;
    const tracer::Call *prior_root_call_ = nullptr;
    const tracer::Call *root_call_ = nullptr;
    std::optional<context_execution::Scope> tracking_;
    std::optional<context_facade::ThreadBinding> binding_;
    bool owns_context_ = false;
    bool workspace_lease_ = false;
  };

private:
  const std::shared_ptr<const RegisteredContextScope> *
  find_scope(context_engine::Identity) const noexcept;
  static void native_publish_hook(void *, bool) noexcept;

  static const context_facade::NativeFunctionTable table_;
  static void *native_current_context(void *) noexcept;
  static void *native_frame_enter(void *);
  static void native_frame_exit(void *, void *) noexcept;
  static void native_create_or_attach(void *, void *, context_facade::StateId);
  static void *native_hook_begin(void *, const void *, const TraceContext *,
                                 const context_facade::RequestDescriptor *,
                                 std::size_t, bool, bool);
  static void native_hook_end(void *, void *, bool) noexcept;
  static void *native_access_begin(void *,
                                  const context_facade::RequestDescriptor *,
                                  std::size_t, context_facade::AccessKind,
                                  context_facade::Status *);
  static void native_access_end(void *, void *) noexcept;
  static context_facade::Status native_get(void *, context_facade::StateId,
                                           void **) noexcept;
  static void native_init_scope(void *, context_facade::ScopeId,
                                context_facade::StateId, const void *);
  static context_facade::Status native_end_scope(
      void *, context_facade::ScopeId) noexcept;
  static bool native_event(void *, const TraceContext *, std::string_view,
                           std::size_t) noexcept;
  static std::uint64_t native_thread_id(void *) noexcept;

  friend class Invocation;
  friend struct RuntimeThreadState;
  friend struct RuntimeHookToken;
  std::vector<std::shared_ptr<const RegisteredContextScope>> scopes_;
  std::vector<StateEntry> states_;
  context_engine::ContextCapturePool &capture_pool_;
  EventMetadata metadata_;
  DeliveryPublish publish_ = nullptr;
  void *publish_context_ = nullptr;
  std::atomic<std::uint64_t> capture_drops_{0};
};

class LifecycleBoundary final {
public:
  LifecycleBoundary() noexcept
      : scope_(context_execution::Mode::LifecycleNoTrace) {}
  bool entered() const noexcept { return scope_.entered(); }
private:
  context_execution::Scope scope_;
};

} // namespace tracer::context_runtime

#pragma once

#include "context_scheduler.hpp"

#include <atomic>
#include <memory>

namespace tracer::context_scheduler {

// Process-local owner for the ABI-stable pthread_create entry wrapper. The
// external injector installs/restores the target text patch; this object owns
// the wrapper dispatch and remains alive until every accepted task completes.
class ContextPthreadDetour final {
public:
  ContextPthreadDetour(std::shared_ptr<context_engine::PropagationLedger> ledger,
                       ActiveContextGetter current_context,
                       WorkerContextGetter worker_context,
                       ActiveContextSetter set_context = nullptr,
                       context_execution::Diagnostics &diagnostics =
                           context_execution::runtime_diagnostics()) noexcept;
  ~ContextPthreadDetour();
  ContextPthreadDetour(const ContextPthreadDetour &) = delete;
  ContextPthreadDetour &operator=(const ContextPthreadDetour &) = delete;

  bool activate(PthreadCreate original_trampoline) noexcept;
  bool enable_propagation() noexcept;
  void disable_propagation() noexcept;
  bool deactivate_after_quiescence() noexcept;
  std::size_t outstanding_tasks() const noexcept;
  bool active() const noexcept;

private:
  friend int pthread_create_detour(pthread_t *, const pthread_attr_t *,
                                   void *(*)(void *), void *) noexcept;
  PthreadCreate original_ = nullptr;
  NativePthreadAdapter adapter_;
  std::atomic<bool> accepting_{false};
  std::atomic<bool> active_{false};
};

// Function address returned to the injector. Its Itanium/Linux x86_64 ABI is
// exactly pthread_create; callers remain unmodified on admission failure.
extern "C" __attribute__((visibility("default"))) int
tracer_context_pthread_create_detour(
    pthread_t *thread, const pthread_attr_t *attributes,
    void *(*start)(void *), void *argument) noexcept;

} // namespace tracer::context_scheduler

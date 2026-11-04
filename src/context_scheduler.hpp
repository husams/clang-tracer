#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include <pthread.h>

#include "context_engine.hpp"
#include "context_execution.hpp"

namespace tracer::context_scheduler {

struct Identity {
  std::uint64_t generation = 0;
  std::uint64_t value = 0;
  friend bool operator==(Identity, Identity) = default;
};

// Fixed metadata copied into event records by the schema/export bridge.
struct TaskMetadata {
  Identity task_id{};
  Identity parent_task_id{};
  Identity source_id{};
  std::uint64_t source_sequence = 0;
  Identity parent_source_id{};
  std::uint64_t parent_submission_sequence = 0;
  bool inherited = false;
};

TaskMetadata current_task_metadata() noexcept;
// Event capture and supported scheduling boundaries share this sequence.
std::uint64_t next_source_sequence() noexcept;

using PthreadCreate = int (*)(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);
using ActiveContextGetter = context_engine::Context* (*)() noexcept;
// Worker-context pool admission may fail; pthread propagation treats that as
// a tracing decline and still invokes the application's start routine.
using WorkerContextGetter = context_engine::Context& (*)();
// Installs the supplied thread-local active context and returns the previous
// value so the adapter can restore the worker exactly after task completion.
using ActiveContextSetter = context_engine::Context* (*)(context_engine::Context*) noexcept;

// Agent/compiler threads must not inherit application context or keep the
// application pthread detour alive for the lifetime of a JIT worker.
class ScopedPthreadBypass final {
public:
  ScopedPthreadBypass() noexcept;
  ~ScopedPthreadBypass() noexcept;
  ScopedPthreadBypass(const ScopedPthreadBypass &) = delete;
  ScopedPthreadBypass &operator=(const ScopedPthreadBypass &) = delete;
};

class NativePthreadAdapter final {
public:
  explicit NativePthreadAdapter(std::shared_ptr<context_engine::PropagationLedger> ledger,
                                ActiveContextGetter current_context,
                                WorkerContextGetter worker_context, ActiveContextSetter set_context,
                                context_execution::Diagnostics& diagnostics =
                                    context_execution::runtime_diagnostics()) noexcept;

  NativePthreadAdapter(const NativePthreadAdapter&) = delete;
  NativePthreadAdapter& operator=(const NativePthreadAdapter&) = delete;

  // Call from the native pthread_create detour. The original is invoked once
  // in every path; tracing admission failures never reject application work.
  int create(PthreadCreate original, pthread_t* thread, const pthread_attr_t* attributes,
             void* (*start)(void*), void* argument) noexcept;

  static constexpr std::size_t max_outstanding_wrappers = 4096;
  static constexpr std::size_t metadata_budget_bytes = 4 * 1024 * 1024;
  static constexpr std::size_t wrapper_budget_bytes = 1 * 1024 * 1024;
  static std::size_t outstanding_wrapper_count() noexcept;

private:
  std::shared_ptr<context_engine::PropagationLedger> ledger_;
  ActiveContextGetter current_context_ = nullptr;
  WorkerContextGetter worker_context_ = nullptr;
  ActiveContextSetter set_context_ = nullptr;
  context_execution::Diagnostics* diagnostics_ = nullptr;
};

} // namespace tracer::context_scheduler

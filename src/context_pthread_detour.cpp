#include "context_pthread_detour.hpp"

#include <pthread.h>

#include <cerrno>
#include <thread>

namespace tracer::context_scheduler {
namespace {
std::atomic<ContextPthreadDetour *> active_detour{nullptr};
std::atomic<PthreadCreate> fallback_original{nullptr};
std::atomic<std::size_t> wrapper_calls{0};
} // namespace

ContextPthreadDetour::ContextPthreadDetour(
    std::shared_ptr<context_engine::PropagationLedger> ledger,
    ActiveContextGetter current_context, WorkerContextGetter worker_context,
    ActiveContextSetter set_context,
    context_execution::Diagnostics &diagnostics) noexcept
    : adapter_(std::move(ledger), current_context, worker_context, set_context,
               diagnostics) {}

ContextPthreadDetour::~ContextPthreadDetour() {
  disable_propagation();
  while (outstanding_tasks() != 0)
    std::this_thread::yield();
  (void)deactivate_after_quiescence();
}

bool ContextPthreadDetour::activate(PthreadCreate original_trampoline) noexcept {
  if (!original_trampoline || active_.load(std::memory_order_acquire))
    return false;
  original_ = original_trampoline;
  fallback_original.store(original_trampoline, std::memory_order_release);
  ContextPthreadDetour *expected = nullptr;
  if (!active_detour.compare_exchange_strong(expected, this,
                                              std::memory_order_acq_rel)) {
    original_ = nullptr;
    return expected == this;
  }
  accepting_.store(true, std::memory_order_release);
  active_.store(true, std::memory_order_release);
  return true;
}

bool ContextPthreadDetour::enable_propagation() noexcept {
  if (!active_.load(std::memory_order_acquire) || !original_)
    return false;
  accepting_.store(true, std::memory_order_release);
  return true;
}

void ContextPthreadDetour::disable_propagation() noexcept {
  accepting_.store(false, std::memory_order_release);
}

bool ContextPthreadDetour::deactivate_after_quiescence() noexcept {
  if (outstanding_tasks() != 0)
    return false;
  auto *expected = this;
  if (!active_detour.compare_exchange_strong(expected, nullptr,
                                              std::memory_order_acq_rel) &&
      expected != nullptr)
    return false;
  while (wrapper_calls.load(std::memory_order_acquire) != 0)
    std::this_thread::yield();
  active_.store(false, std::memory_order_release);
  return true;
}

std::size_t ContextPthreadDetour::outstanding_tasks() const noexcept {
  return NativePthreadAdapter::outstanding_wrapper_count();
}

bool ContextPthreadDetour::active() const noexcept {
  return active_.load(std::memory_order_acquire);
}

int pthread_create_detour(pthread_t *thread, const pthread_attr_t *attributes,
                          void *(*start)(void *), void *argument) noexcept {
  wrapper_calls.fetch_add(1, std::memory_order_acq_rel);
  struct Exit {
    ~Exit() { wrapper_calls.fetch_sub(1, std::memory_order_release); }
  } exit;
  auto *detour = active_detour.load(std::memory_order_acquire);
  if (detour && detour->original_) {
    if (detour->accepting_.load(std::memory_order_acquire))
      return detour->adapter_.create(detour->original_, thread, attributes,
                                     start, argument);
    return detour->original_(thread, attributes, start, argument);
  }
  const auto original = fallback_original.load(std::memory_order_acquire);
  return original ? original(thread, attributes, start, argument) : EAGAIN;
}

extern "C" int tracer_context_pthread_create_detour(
    pthread_t *thread, const pthread_attr_t *attributes,
    void *(*start)(void *), void *argument) noexcept {
  return pthread_create_detour(thread, attributes, start, argument);
}

} // namespace tracer::context_scheduler

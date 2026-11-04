#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

#include <sys/wait.h>
#include <unistd.h>

#include "context_pthread_detour.hpp"
#include "context_scheduler.hpp"
#include "tracer/compat.hpp"

namespace {
namespace ce = tracer::context_engine;
namespace cs = tracer::context_scheduler;
namespace cx = tracer::context_execution;

struct State {
  int value = 41;
};

struct ScopeTag {};

ce::Diagnostics engine_diagnostics;
thread_local ce::Context worker_context(engine_diagnostics);
thread_local ce::Context* active_context = &worker_context;
std::mutex getter_mutex;
std::condition_variable getter_cv;
std::atomic<bool> pause_worker_getter{false};
bool worker_getter_waiting = false, release_worker_getter = false;

ce::Context* get_context() noexcept {
  if (pause_worker_getter && active_context == &worker_context) {
    std::unique_lock lock(getter_mutex);
    worker_getter_waiting = true;
    getter_cv.notify_all();
    getter_cv.wait(lock, [] { return release_worker_getter; });
  }
  return active_context;
}

ce::Context& get_worker_context() noexcept { return worker_context; }

ce::Context& throw_worker_context() {
  throw std::runtime_error("worker context pool exhausted");
}

ce::Context* set_context(ce::Context* context) noexcept {
  return std::exchange(active_context, context);
}

std::shared_ptr<const ce::ScopeRegistration>
make_registration(ce::Policy policy = ce::Policy::shared) {
  auto registration = std::make_shared<ce::ScopeRegistration>();
  registration->identity = 501;
  registration->owner_code = std::make_shared<int>(1);
  registration->states.push_back({502, policy,
                                  [](const ce::ScopeInit&, std::any& payload) noexcept {
                                    try {
                                      payload = std::make_shared<State>();
                                      return ce::ErrorCode::ok;
                                    } catch (...) {
                                      return ce::ErrorCode::construction_failed;
                                    }
                                  },
                                  [](std::any& payload) noexcept { payload.reset(); },
                                  [](std::any& payload) noexcept -> void* {
                                    auto* state = std::any_cast<std::shared_ptr<State>>(&payload);
                                    return state ? state->get() : nullptr;
                                  }});
  return registration;
}

struct Fixture {
  ce::Diagnostics diagnostics;
  ce::Context parent{diagnostics};
  std::shared_ptr<const ce::ScopeRegistration> registration = make_registration();
  ce::SlotRequest request{registration, 502};
  std::shared_ptr<ce::PropagationLedger> ledger =
      std::make_shared<ce::PropagationLedger>(ce::PropagationLimits{64, 4096, 3 * 1024 * 1024, 64});
  cs::NativePthreadAdapter adapter{ledger, get_context, get_worker_context, set_context,
                                   cx::runtime_diagnostics()};
  std::unique_ptr<ce::InvocationFrame> owner;
  ce::Context* previous = nullptr;

  Fixture() {
    previous = set_context(&parent);
    owner = std::make_unique<ce::InvocationFrame>(parent);
    owner->create_or_attach(registration);
  }

  ~Fixture() {
    if (owner) {
      owner->pop_all_and_restore();
    }
    set_context(previous);
  }
};

void inspect_inherited_state(ce::Context& context, const ce::SlotRequest& request) {
  cx::Scope tracking(cx::Mode::Tracking);
  assert(tracking.entered());
  int hook = 0;
  cx::CurrentHook current_hook(&hook);
  assert(current_hook.installed());
  ce::HookExit exit(context);
  ce::HookAccess access(context, ::tracer::compat::span(&request, 1));
  assert(static_cast<State*>(access.ref(request))->value == 41);
}

struct Work {
  Fixture* fixture;
  std::mutex mutex;
  std::condition_variable cv;
  bool started = false, release = false;
  std::atomic<unsigned> calls{0};
  std::atomic<bool> metadata_ok{false};
};

void* wait_then_inspect(void* opaque) {
  auto& work = *static_cast<Work*>(opaque);
  const auto metadata = cs::current_task_metadata();
  work.metadata_ok = metadata.inherited && metadata.task_id.value != 0 &&
                     metadata.parent_task_id.value != 0 && metadata.source_id.value != 0 &&
                     metadata.parent_source_id.value != 0 &&
                     metadata.parent_submission_sequence != 0;
  ++work.calls;
  {
    std::unique_lock lock(work.mutex);
    work.started = true;
    work.cv.notify_all();
    work.cv.wait(lock, [&] { return work.release; });
  }
  inspect_inherited_state(*get_context(), work.fixture->request);
  return nullptr;
}

int fail_create(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*) {
  return EAGAIN;
}

int create_and_join_before_return(pthread_t* thread, const pthread_attr_t* attributes,
                                  void* (*start)(void*), void* argument) {
  const int result = pthread_create(thread, attributes, start, argument);
  if (result == 0) {
    return pthread_join(*thread, nullptr);
  }
  return result;
}

struct ThrowWork {
  std::atomic<unsigned> destroyed{0};
};

struct DestructionWitness {
  std::atomic<unsigned>& destroyed;

  ~DestructionWitness() { ++destroyed; }
};

void* throws_and_catches(void* opaque) {
  auto& work = *static_cast<ThrowWork*>(opaque);
  try {
    DestructionWitness witness{work.destroyed};
    throw std::runtime_error("application exception");
  } catch (const std::runtime_error&) {
  }
  return nullptr;
}

std::atomic<unsigned> uncaught_cleanup_witness{0};
void* throws_out_of_start(void*) {
  DestructionWitness witness{uncaught_cleanup_witness};
  throw std::runtime_error("uncaught application exception");
}

std::shared_ptr<ce::PropagationLedger> *terminate_ledger = nullptr;
void verify_task_cleanup_before_terminate() {
  if (cs::NativePthreadAdapter::outstanding_wrapper_count() != 0) ::_exit(81);
  if (!terminate_ledger || (*terminate_ledger)->outstanding() != 0) ::_exit(82);
  if (uncaught_cleanup_witness.load() != 1) ::_exit(83);
  ::_exit(0);
}

void* cancellable(void*) {
  while (true) {
    pthread_testcancel();
    std::this_thread::yield();
  }
}

void* exits_thread(void*) {
  pthread_exit(nullptr);
}

void* plain_state_fallback(void* opaque) {
  ++*static_cast<std::atomic<unsigned>*>(opaque);
  return nullptr;
}

struct AdmissionFailureWork {
  std::atomic<unsigned> calls{0};
  std::atomic<bool> inherited{true};
};

void* admission_failure_start(void* opaque) {
  auto& work = *static_cast<AdmissionFailureWork*>(opaque);
  work.inherited = cs::current_task_metadata().inherited;
  ++work.calls;
  return nullptr;
}

int require_unwrapped_start(pthread_t*, const pthread_attr_t*,
                            void* (*start)(void*), void*) {
  assert(start == &admission_failure_start);
  return EAGAIN;
}

} // namespace

void verify_context_scheduler() {
  using namespace std::chrono_literals;
  {
    Fixture fixture;
    auto* previous = set_context(nullptr);
    assert(fixture.adapter.create(&require_unwrapped_start, nullptr, nullptr,
                                  &admission_failure_start, nullptr) == EAGAIN);
    set_context(previous);
    assert(cs::NativePthreadAdapter::outstanding_wrapper_count() == 0);
  }
  {
    Fixture fixture;
    AdmissionFailureWork work;
    pthread_t thread{};
    {
      cs::ScopedPthreadBypass compiler_threads;
      cs::ScopedPthreadBypass nested_compiler_work;
      assert(fixture.adapter.create(&pthread_create, &thread, nullptr,
                                    &admission_failure_start, &work) == 0);
      assert(pthread_join(thread, nullptr) == 0);
      assert(!work.inherited && fixture.ledger->outstanding() == 0);
    }
    assert(fixture.adapter.create(&pthread_create, &thread, nullptr,
                                  &admission_failure_start, &work) == 0);
    assert(pthread_join(thread, nullptr) == 0);
    assert(work.inherited && fixture.ledger->outstanding() == 0);
  }
  {
    Fixture fixture;
    Work work{};
    work.fixture = &fixture;
    pause_worker_getter = true;
    worker_getter_waiting = false;
    release_worker_getter = false;
    pthread_t thread{};
    assert(fixture.adapter.create(&pthread_create, &thread, nullptr, &wait_then_inspect, &work) ==
           0);
    {
      std::unique_lock lock(work.mutex);
      std::unique_lock getter_lock(getter_mutex);
      assert(getter_cv.wait_for(getter_lock, 5s, [] { return worker_getter_waiting; }));
    }
    fixture.owner->pop_all_and_restore();
    fixture.owner.reset();
    {
      std::lock_guard lock(getter_mutex);
      release_worker_getter = true;
      pause_worker_getter = false;
      getter_cv.notify_all();
    }
    {
      std::unique_lock lock(work.mutex);
      assert(work.cv.wait_for(lock, 5s, [&] { return work.started; }));
    }
    {
      std::lock_guard lock(work.mutex);
      work.release = true;
      work.cv.notify_all();
    }
    assert(pthread_join(thread, nullptr) == 0);
    assert(work.calls == 1 && work.metadata_ok);
    assert(fixture.ledger->outstanding() == 0);
  }
  {
    Fixture fixture;
    std::atomic<unsigned> calls{0};
    assert(fixture.adapter.create(&fail_create, nullptr, nullptr, &plain_state_fallback, &calls) ==
           EAGAIN);
    assert(calls == 0 && fixture.ledger->outstanding() == 0);
  }
  {
    Fixture fixture;
    std::atomic<unsigned> calls{0};
    pthread_t thread{};
    // The worker finishes and releases its ownership before the original
    // pthread_create call returns to the adapter.
    assert(fixture.adapter.create(&create_and_join_before_return, &thread, nullptr,
                                  &plain_state_fallback, &calls) == 0);
    assert(calls == 1 && fixture.ledger->outstanding() == 0);
  }
  {
    Fixture fixture;
    cs::NativePthreadAdapter rejecting_adapter(
        fixture.ledger, get_context, throw_worker_context, set_context,
        cx::runtime_diagnostics());
    AdmissionFailureWork work;
    pthread_t thread{};
    assert(rejecting_adapter.create(&pthread_create, &thread, nullptr,
                                   &admission_failure_start, &work) == 0);
    assert(pthread_join(thread, nullptr) == 0);
    assert(work.calls == 1 && !work.inherited &&
           fixture.ledger->outstanding() == 0);
  }
  {
    Fixture fixture;
    ThrowWork work;
    pthread_t thread{};
    assert(fixture.adapter.create(&pthread_create, &thread, nullptr, &throws_and_catches, &work) ==
           0);
    assert(pthread_join(thread, nullptr) == 0);
    assert(work.destroyed == 1 && fixture.ledger->outstanding() == 0);
  }
  {
    const pid_t child = ::fork();
    assert(child >= 0);
    if (child == 0) {
      Fixture fixture;
      terminate_ledger = &fixture.ledger;
      std::set_terminate(&verify_task_cleanup_before_terminate);
      pthread_t thread{};
      if (fixture.adapter.create(&pthread_create, &thread, nullptr,
                                 &throws_out_of_start, nullptr) != 0)
        ::_exit(78);
      (void)::pthread_join(thread, nullptr);
      ::_exit(79);
    }
    int status = 0;
    assert(::waitpid(child, &status, 0) == child);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
      std::fprintf(stderr, "uncaught pthread cleanup child status=%d\n", status);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
  }
  {
    Fixture fixture;
    pthread_t thread{};
    assert(fixture.adapter.create(&pthread_create, &thread, nullptr, &cancellable, nullptr) == 0);
    assert(pthread_cancel(thread) == 0);
    void* result = nullptr;
    assert(pthread_join(thread, &result) == 0 && result == PTHREAD_CANCELED);
    assert(fixture.ledger->outstanding() == 0);
  }
  {
    Fixture fixture;
    pthread_t thread{};
    assert(fixture.adapter.create(&pthread_create, &thread, nullptr, &exits_thread, nullptr) == 0);
    void* result = reinterpret_cast<void*>(1);
    assert(pthread_join(thread, &result) == 0 && result == nullptr);
    assert(fixture.ledger->outstanding() == 0);
  }
  {
    Fixture fixture;
    auto plain = make_registration(ce::Policy::confined);
    ce::Context parent(fixture.diagnostics);
    ce::InvocationFrame owner(parent);
    owner.create_or_attach(plain);
    auto* prior = set_context(&parent);
    std::atomic<unsigned> calls{0};
    pthread_t thread{};
    assert(fixture.adapter.create(&pthread_create, &thread, nullptr, &plain_state_fallback,
                                  &calls) == 0);
    assert(pthread_join(thread, nullptr) == 0);
    set_context(prior);
    owner.pop_all_and_restore();
    assert(calls == 1); // A refused whole-chain attachment still runs work.
    assert(fixture.ledger->outstanding() == 0);
  }
  {
    const auto before = cs::current_task_metadata();
    const auto submission = cs::next_source_sequence();
    const auto after = cs::current_task_metadata();
    assert(submission == before.source_sequence + 1);
    assert(after.source_id == before.source_id && after.source_sequence == submission);
  }
  {
    Fixture fixture;
    cs::ContextPthreadDetour detour(fixture.ledger, get_context, get_worker_context,
                                    set_context);
    assert(detour.activate(&pthread_create));
    Work work{};
    work.fixture = &fixture;
    work.release = true;
    pthread_t thread{};
    assert(cs::tracer_context_pthread_create_detour(
               &thread, nullptr, &wait_then_inspect, &work) == 0);
    assert(pthread_join(thread, nullptr) == 0);
    assert(work.calls == 1 && work.metadata_ok);
    assert(detour.outstanding_tasks() == 0);
    detour.disable_propagation();
    assert(detour.deactivate_after_quiescence());
    assert(!detour.active());
  }
}

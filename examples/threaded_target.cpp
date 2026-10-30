#include <array>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <thread>

#include <sys/syscall.h>
#include <unistd.h>

#include "shared_stack.hpp"

namespace threaded {

namespace {

void pause_for_call() {
  timespec delay{0, 5'000'000};
  while (::nanosleep(&delay, &delay) != 0 && errno == EINTR) {
  }
}

} // namespace

__attribute__((visibility("default"), noinline)) int
namespaced_work(int value) {
  volatile int saved_value = value;
  volatile int call_input = saved_value;
  pause_for_call();
  volatile int result = call_input * 7 + 13;
  return result;
}

class Worker {
public:
  explicit Worker(int identity) : identity_(identity) {}

  __attribute__((noinline)) int work(int value) {
    pause_for_call();
    volatile int result = value + identity_ * 1009;
    return result;
  }

private:
  int identity_;
};

} // namespace threaded

namespace {

constexpr std::size_t kWorkerCount = 4;
alignas(64) volatile std::uint32_t callback_gate = 0;
alignas(64) volatile std::uint32_t callbacks_active = 0;

struct WorkerState {
  std::atomic<std::uint64_t> calls{0};
  std::atomic<std::uint64_t> failures{0};
  std::atomic<std::uint32_t> tid{0};
  std::atomic<std::uintptr_t> object_address{0};
};

std::array<WorkerState, kWorkerCount> states;
std::atomic<bool> running{true};
alignas(64) volatile std::uint64_t main_calls = 0;
alignas(64) volatile std::uint64_t main_failures = 0;

class NestedLeaf {
public:
  __attribute__((noinline)) void spin(volatile int *errno_address) const {
    std::uint64_t iteration = 1;
    for (;;) {
      volatile std::uint64_t integer_value =
          iteration * 0x9e3779b97f4a7c15ULL + 0x632be59bd9b4e019ULL;
      volatile std::uint64_t integer_check =
          (integer_value - 0x632be59bd9b4e019ULL) -
          iteration * 0x9e3779b97f4a7c15ULL;
      volatile double floating_value =
          static_cast<double>(iteration & 0xffffU) * 0.125 + 0.375;
      volatile double floating_check = floating_value * 8.0;
      if (integer_check != 0 ||
          floating_check != static_cast<double>(iteration & 0xffffU) + 3.0 ||
          *errno_address != EDOM) {
        __atomic_add_fetch(&main_failures, 1ULL, __ATOMIC_RELAXED);
      }
      __atomic_add_fetch(&main_calls, 1ULL, __ATOMIC_RELAXED);
      ++iteration;
    }
  }
};

class NestedMethod {
public:
  __attribute__((noinline)) void run(volatile int *errno_address) const {
    NestedLeaf leaf;
    leaf.spin(errno_address);
    volatile int keep_frame = 1;
    (void)keep_frame;
  }
};

__attribute__((noinline)) void enter_nested_main(volatile int *errno_address) {
  NestedMethod method;
  method.run(errno_address);
  volatile int keep_frame = 1;
  (void)keep_frame;
}

void run_worker(std::size_t index) {
  const int identity = static_cast<int>(index + 1);
  threaded::Worker object(identity);
  auto &state = states[index];
  state.tid.store(static_cast<std::uint32_t>(::syscall(SYS_gettid)),
                  std::memory_order_release);
  state.object_address.store(reinterpret_cast<std::uintptr_t>(&object),
                             std::memory_order_release);
  int value = identity * 17;
  while (running.load(std::memory_order_relaxed)) {
    const int free_result = threaded::namespaced_work(value);
    const int method_result = object.work(value);
    if (free_result != value * 7 + 13 ||
        method_result != value + identity * 1009) {
      state.failures.fetch_add(1, std::memory_order_relaxed);
    }
    state.calls.fetch_add(1, std::memory_order_relaxed);
    ++value;
    timespec delay{0, 2'000'000};
    while (::nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
  }
}

void run_churn() {
  int identity = 100;
  while (running.load(std::memory_order_relaxed)) {
    std::thread short_lived([identity] {
      threaded::Worker object(identity);
      const int value = identity * 3;
      (void)threaded::namespaced_work(value);
      (void)object.work(value);
    });
    short_lived.join();
    ++identity;
    if (identity == 10000) {
      identity = 100;
    }
    timespec delay{0, 8'000'000};
    while (::nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
  }
}

void print_snapshot() {
  for (std::size_t index = 0; index < kWorkerCount; ++index) {
    const auto &state = states[index];
    std::printf("WORKER id=%zu tid=%u object=%p calls=%llu failures=%llu\n",
                index + 1, state.tid.load(std::memory_order_acquire),
                reinterpret_cast<void *>(
                    state.object_address.load(std::memory_order_acquire)),
                static_cast<unsigned long long>(
                    state.calls.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(
                    state.failures.load(std::memory_order_relaxed)));
  }
  std::fflush(stdout);
}

void run_reporter() {
  while (running.load(std::memory_order_relaxed)) {
    std::printf("MAIN calls=%llu failures=%llu\n",
                static_cast<unsigned long long>(
                    __atomic_load_n(&main_calls, __ATOMIC_RELAXED)),
                static_cast<unsigned long long>(
                    __atomic_load_n(&main_failures, __ATOMIC_RELAXED)));
    print_snapshot();
    timespec delay{0, 50'000'000};
    while (::nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
  }
}

} // namespace

extern "C" __attribute__((visibility("default"), noinline)) void
threaded_callback_enter() {
  __atomic_add_fetch(&callbacks_active, 1U, __ATOMIC_ACQ_REL);
}

extern "C" __attribute__((visibility("default"), noinline)) void
threaded_callback_leave() {
  __atomic_sub_fetch(&callbacks_active, 1U, __ATOMIC_ACQ_REL);
}

int main(int argc, char **argv) {
  bool busy_main = false;
  bool nested_main = false;
  bool shared_main = false;
  for (int index = 1; index < argc; ++index) {
    if (std::strcmp(argv[index], "--busy-main") == 0) {
      busy_main = true;
    } else if (std::strcmp(argv[index], "--nested-main") == 0) {
      busy_main = true;
      nested_main = true;
    } else if (std::strcmp(argv[index], "--shared-main") == 0) {
      busy_main = true;
      shared_main = true;
    } else {
      return 2;
    }
  }
  std::signal(SIGTERM, SIG_DFL);
  if (busy_main) {
    sigset_t busy_mask;
    if (::sigemptyset(&busy_mask) != 0 ||
        ::sigaddset(&busy_mask, SIGUSR1) != 0 ||
        ::sigprocmask(SIG_BLOCK, &busy_mask, nullptr) != 0) {
      return 3;
    }
  }
  const auto free_entry = reinterpret_cast<void *>(
      reinterpret_cast<std::uintptr_t>(&threaded::namespaced_work));
  using Method = int (threaded::Worker::*)(int);
  const Method method = &threaded::Worker::work;
  static_assert(sizeof(method) >= sizeof(std::uintptr_t));
  std::uintptr_t method_entry = 0;
  __builtin_memcpy(&method_entry, &method, sizeof(method_entry));
  std::printf(
      "TARGET pid=%ld free=%p method=%p gate=%p active=%p busy=%d mode=%s\n",
      static_cast<long>(::getpid()), free_entry,
      reinterpret_cast<void *>(method_entry),
      const_cast<std::uint32_t *>(&callback_gate),
      const_cast<std::uint32_t *>(&callbacks_active), busy_main ? 1 : 0,
      shared_main
          ? "shared"
          : (nested_main ? "nested" : (busy_main ? "busy" : "sleeping")));
  std::fflush(stdout);

  std::array<std::thread, kWorkerCount> workers;
  for (std::size_t index = 0; index < workers.size(); ++index) {
    workers[index] = std::thread(run_worker, index);
  }
  std::thread churn(run_churn);
  std::thread reporter;
  if (busy_main) {
    reporter = std::thread(run_reporter);
    // Resolve this thread's TLS errno slot once. The busy loop below performs
    // only arithmetic, direct TLS reads, and inline atomic instructions.
    volatile int *const errno_address = &errno;
    *errno_address = EDOM;
    if (shared_main) {
      SharedStackState state{&main_calls, &main_failures, errno_address};
      shared_stack_enter(&state);
    } else if (nested_main) {
      enter_nested_main(errno_address);
    } else {
      // Keep the main thread in a call-free loop until SIGTERM.
      std::uint64_t iteration = 1;
      while (true) {
        volatile std::uint64_t integer_value =
            iteration * 0x9e3779b97f4a7c15ULL + 0x632be59bd9b4e019ULL;
        volatile std::uint64_t integer_check =
            (integer_value - 0x632be59bd9b4e019ULL) -
            iteration * 0x9e3779b97f4a7c15ULL;
        volatile double floating_value =
            static_cast<double>(iteration & 0xffffU) * 0.125 + 0.375;
        volatile double floating_check = floating_value * 8.0;
        if (integer_check != 0 ||
            floating_check != static_cast<double>(iteration & 0xffffU) + 3.0 ||
            *errno_address != EDOM) {
          __atomic_add_fetch(&main_failures, 1ULL, __ATOMIC_RELAXED);
        }
        __atomic_add_fetch(&main_calls, 1ULL, __ATOMIC_RELAXED);
        ++iteration;
      }
    }
  } else {
    while (running.load(std::memory_order_relaxed)) {
      print_snapshot();
      timespec delay{0, 100'000'000};
      while (::nanosleep(&delay, &delay) != 0 && errno == EINTR) {
      }
    }
  }
  for (auto &worker : workers) {
    worker.join();
  }
  churn.join();
  if (reporter.joinable()) {
    reporter.join();
  }
  return 0;
}

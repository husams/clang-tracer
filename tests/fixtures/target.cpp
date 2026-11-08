#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <stdexcept>
#include <pthread.h>
#include <atomic>

#include <unistd.h>

extern "C" __attribute__((visibility("default"), noinline)) int
traced_work(int argument) {
  volatile int scaled = argument * 3;
  volatile int result = scaled + 7;
  return result;
}

extern "C" __attribute__((visibility("default"), noinline)) double
traced_mixed(double left, double right) {
  volatile double result = left * 2.0 + right;
  return result;
}

extern "C" __attribute__((visibility("default"), noinline)) int
invalid_source_target(int value) {
  volatile int result = value + 23;
  return result;
}

extern "C" __attribute__((visibility("default"), noinline)) int
wrong_callback_target(int value) {
  volatile int result = value + 29;
  return result;
}

volatile int last_void_argument = 0;

extern "C" __attribute__((visibility("default"), noinline)) long long
traced_int64(long long value) {
  volatile long long result = value + 31;
  return result;
}

extern "C" __attribute__((visibility("default"), noinline)) float
traced_float32(float value) {
  volatile float saved = value;
  volatile float result = saved + 0.75F;
  return result;
}

extern "C" __attribute__((visibility("default"), noinline)) void *
traced_pointer(void *value) {
  void *volatile result = value;
  return result;
}

extern "C" __attribute__((visibility("default"), noinline)) void
traced_void(int value) {
  last_void_argument = value;
}

namespace fixture {
__attribute__((visibility("default"), noinline)) int
namespaced_work(int argument) {
  volatile int result = argument * 5 + 3;
  return result;
}
} // namespace fixture

class Fixture {
public:
  __attribute__((noinline)) int overloaded(int argument) {
    volatile int result = argument + 11;
    return result;
  }

  __attribute__((noinline)) int overloaded(long argument) {
    volatile int result = static_cast<int>(argument * 2);
    return result;
  }

  __attribute__((noinline)) double inspect(double argument) const {
    volatile double saved = argument;
    volatile double result = saved + 0.5;
    return result;
  }

  static __attribute__((noinline)) unsigned long
  static_work(unsigned long argument) {
    volatile unsigned long result = argument + 17;
    return result;
  }
};

extern "C" int shared_child_target(int);

namespace {
std::atomic<unsigned> shared_child_calls{0};

void *shared_context_child_entry(void *opaque) {
  const auto encoded = reinterpret_cast<std::intptr_t>(opaque);
  const bool delayed = encoded < 0;
  const int value = static_cast<int>(delayed ? -encoded : encoded);
  if (delayed) {
    timespec delay{1, 500'000'000};
    while (::nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
  }
  const int result = shared_child_target(value);
  return reinterpret_cast<void *>(static_cast<std::intptr_t>(result));
}
} // namespace

extern "C" __attribute__((visibility("default"), noinline)) int
shared_child_target(int value) {
  volatile int saved = value;
  volatile int result = saved + 31;
  shared_child_calls.fetch_add(1, std::memory_order_relaxed);
  return result;
}

extern "C" __attribute__((visibility("default"), noinline)) int
shared_context_spawn_join(int value) {
  pthread_t child{};
  const int created = ::pthread_create(
      &child, nullptr, &shared_context_child_entry,
      reinterpret_cast<void *>(static_cast<std::intptr_t>(value)));
  if (created != 0) return -created;
  void *result = nullptr;
  if (::pthread_join(child, &result) != 0) return -1;
  return static_cast<int>(reinterpret_cast<std::intptr_t>(result));
}

extern "C" __attribute__((visibility("default"), noinline)) int
shared_context_spawn_early(int value) {
  pthread_t child{};
  const int created = ::pthread_create(
      &child, nullptr, &shared_context_child_entry,
      reinterpret_cast<void *>(-static_cast<std::intptr_t>(value)));
  if (created != 0) return -created;
  (void)::pthread_detach(child);
  volatile int result = value + 47;
  return result;
}

extern "C" __attribute__((visibility("default"), noinline)) int
shared_context_native_throw(int value) {
  throw std::runtime_error("shared-context native target exception");
  return value;
}

// Starts with a branch so the server rejects the deliberately unsafe patch
// site.
extern "C" __attribute__((visibility("default"), noinline, naked)) int
unsafe_target(int) {
#if defined(__x86_64__)
  asm volatile("jmp 1f\n"
               "1: leal 1(%%rdi), %%eax\n"
               "ret\n"
               :
               :
               : "memory");
#else
#error "unsafe_target fixture requires x86_64"
#endif
}

int main() {
  std::signal(SIGTERM, SIG_DFL);
  Fixture fixture;
  int argument = 1;
  for (;;) {
    const int free_result = traced_work(argument);
    const double mixed_result =
        traced_mixed(static_cast<double>(argument), 0.25);
    const int method_result = fixture.overloaded(argument);
    const int long_method_result =
        fixture.overloaded(static_cast<long>(argument));
    const int const_result =
        static_cast<int>(fixture.inspect(static_cast<double>(argument)));
    const unsigned long static_result =
        Fixture::static_work(static_cast<unsigned long>(argument));
    const int namespaced_result = fixture::namespaced_work(argument);
    int shared_join_result = 0;
    int shared_early_result = 0;
    int shared_exception_catches = 0;
    if (argument % 10 == 1) {
      shared_join_result = shared_context_spawn_join(1);
      shared_early_result = shared_context_spawn_early(1);
      try {
        (void)shared_context_native_throw(argument);
      } catch (const std::runtime_error &) {
        shared_exception_catches = 1;
      }
    }
    const long long int64_result =
        traced_int64(static_cast<long long>(argument));
    const float float32_result = traced_float32(static_cast<float>(argument));
    void *const pointer_result = traced_pointer(&argument);
    traced_void(argument);
    std::printf(
        "TARGET pid=%ld entry=%p arg=%d free=%d mixed=%.2f method=%d "
        "long_method=%d const=%d "
        "static=%lu namespaced=%d int64=%lld float32=%.2f pointer=%d void=%d "
        "shared_join=%d shared_early=%d shared_exceptions=%d "
        "shared_children=%u\n",
        static_cast<long>(::getpid()),
        reinterpret_cast<void *>(
            reinterpret_cast<std::uintptr_t>(&traced_work)),
        argument, free_result, mixed_result, method_result, long_method_result,
        const_result, static_result, namespaced_result, int64_result,
        float32_result, pointer_result == &argument ? 1 : 0,
        static_cast<int>(last_void_argument), shared_join_result,
        shared_early_result, shared_exception_catches,
        shared_child_calls.load(std::memory_order_relaxed));
    std::fflush(stdout);

    ++argument;
    timespec delay{0, 200'000'000};
    while (::nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
  }
}

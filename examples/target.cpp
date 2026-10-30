#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <ctime>

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
    const long long int64_result =
        traced_int64(static_cast<long long>(argument));
    const float float32_result = traced_float32(static_cast<float>(argument));
    void *const pointer_result = traced_pointer(&argument);
    traced_void(argument);
    std::printf(
        "TARGET pid=%ld entry=%p arg=%d free=%d mixed=%.2f method=%d "
        "long_method=%d const=%d "
        "static=%lu namespaced=%d int64=%lld float32=%.2f pointer=%d void=%d\n",
        static_cast<long>(::getpid()),
        reinterpret_cast<void *>(
            reinterpret_cast<std::uintptr_t>(&traced_work)),
        argument, free_result, mixed_result, method_result, long_method_result,
        const_result, static_result, namespaced_result, int64_result,
        float32_result, pointer_result == &argument ? 1 : 0,
        static_cast<int>(last_void_argument));
    std::fflush(stdout);

    ++argument;
    timespec delay{0, 200'000'000};
    while (::nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
  }
}

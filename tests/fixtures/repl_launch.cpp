#include <tracer/launch.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sys/syscall.h>
#include <unistd.h>

__attribute__((noinline)) int startup_call(int input) {
  asm volatile("nop; nop; nop; nop; nop; nop; nop; nop" ::: "memory");
  return input + 10;
}

namespace {
unsigned call_count = 0;

unsigned long long scalar_call(int i32, unsigned int u32, long long i64,
                               unsigned long long u64, bool flag,
                               float f32, double f64) {
  ++call_count;
  return i32 == std::numeric_limits<int>::min() &&
                 u32 == std::numeric_limits<unsigned int>::max() &&
                 i64 == std::numeric_limits<long long>::min() &&
                 flag && f32 == 1.25f && f64 == -2.5 ? u64 : 0;
}

long long held_call() {
  ++call_count;
  while (::access("release-call", F_OK) != 0)
    ::usleep(10'000);
  return -9000000000LL;
}

bool startup_thread() { return ::syscall(SYS_gettid) == ::getpid(); }
float float_result(float value) { return value; }
double double_result(double value) { return value; }
unsigned count_calls() { return call_count; }
void throw_error() { throw 17; }
void do_nothing() {}
} // namespace

int main(int argc, char **argv) {
  if (argc > 1 && std::strcmp(argv[1], "--early") == 0)
    return 7;
  if (argc > 1 && std::strcmp(argv[1], "--no-ready") == 0)
    for (;;)
      ::usleep(10'000);
  using tracer::launch::named_call;
  const bool no_calls = argc > 1 && std::strcmp(argv[1], "--no-calls") == 0;
  const bool resumed = no_calls ? tracer::launch::wait_for_resume() :
      tracer::launch::wait_for_resume(
          named_call("app::startup", startup_call),
          named_call("app::scalar", scalar_call),
          named_call("app::held", held_call),
          named_call("app::startup-thread", startup_thread),
          named_call("app::float", float_result),
          named_call("app::double", double_result),
          named_call("app::count", count_calls),
          named_call("app::throw", throw_error),
          named_call("app::void", do_nothing),
          named_call("app::duplicate", do_nothing),
          named_call("app::duplicate", do_nothing));
  if (!resumed)
    return 125;

  const int result = startup_call(7);
  char cwd[4096]{};
  char input[128]{};
  if (!::getcwd(cwd, sizeof(cwd)) || !std::fgets(input, sizeof(input), stdin))
    return 8;
  const char *value = std::getenv("TRACER_LAUNCH_TEST");
  std::printf("result=%d\ncwd=%s\nargs=%s,%s\nenv=%s\nstdin=%s",
              result, cwd, argc > 1 ? argv[1] : "", argc > 2 ? argv[2] : "",
              value ? value : "", input);
  std::fflush(stdout);
  std::fputs("target stderr\n", stderr);
  if (argc > 1 && std::strcmp(argv[1], "--exit-after-start") == 0)
    return 0;
  for (;;)
    ::usleep(10'000);
}

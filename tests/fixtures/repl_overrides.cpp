#include <tracer/launch.hpp>
#include <tracer/override_target.hpp>

#include <cerrno>
#include <cstdio>
#include <unistd.h>

TRACER_SCALAR_OVERRIDE_TARGET double calculate(int depth, int bias, long long signed_token,
                                   unsigned long long token, bool flag,
                                   int guard, int tail, double amount) {
  asm volatile("nop; nop; nop; nop; nop; nop; nop; nop" ::: "memory");
  const int entry_errno = errno;
  if (depth < 0) {
    errno = EDOM;
    throw 17;
  }
  double result;
  if (depth != 0) {
    result = calculate(depth - 1, bias, signed_token, token, flag, guard, tail, amount) + 1;
  } else {
    result = bias + guard + tail + amount + entry_errno;
    if (signed_token != -9000000000LL || token != 18446744073709551615ULL || !flag)
      result += 10000;
  }
  errno = EINTR;
  return result;
}

TRACER_SCALAR_OVERRIDE_TARGET float float_sample(float value) {
  asm volatile("nop; nop; nop; nop; nop; nop; nop; nop" ::: "memory");
  return value + 1.0f;
}

TRACER_SCALAR_OVERRIDE_TARGET long long integer_sample(long long value, bool flag) {
  asm volatile("nop; nop; nop; nop; nop; nop; nop; nop" ::: "memory");
  return flag ? value : 0;
}

[[gnu::noinline, gnu::used, gnu::no_caller_saved_registers]] int unmarked(int a) { return a; }
[[gnu::noinline, gnu::used]] int pointer_sample(int *value) { return *value; }
[[gnu::noinline, gnu::used]] void void_sample(int) {}
[[gnu::noinline, gnu::used, gnu::ms_abi]] int other_abi(int a, double b) { return a + b; }

namespace {
int last_errno = 0;

double scenario(int depth) {
  errno = EAGAIN;
  try {
    const auto result = calculate(depth, 2, -9000000000LL, 18446744073709551615ULL,
                                  true, 5, 4, 0.5);
    last_errno = errno;
    return result;
  } catch (int error) {
    last_errno = errno;
    return -error;
  }
}

int read_errno() { return last_errno; }

void print_scenario(const char *label) {
  const auto result = scenario(1);
  std::printf("%s result=%.2f errno=%d\n", label, result, last_errno);
  std::fflush(stdout);
}
} // namespace

int main() {
  using tracer::launch::named_call;
  if (!tracer::launch::wait_for_resume(named_call("scenario", scenario),
                                       named_call("read_errno", read_errno),
                                       named_call("float_sample", float_sample),
                                       named_call("integer_sample", integer_sample)))
    return 125;
  print_scenario("resumed");
  while (::access("after-detach", F_OK) != 0)
    ::usleep(10'000);
  print_scenario("detached");
  for (;;)
    ::usleep(10'000);
}

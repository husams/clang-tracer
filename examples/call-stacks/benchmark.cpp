#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <ctime>

#include <fcntl.h>
#include <unistd.h>

#include "calculator.hpp"

// Compile with -fno-inline and -fno-optimize-sibling-calls so the existing
// library's stack_add has genuine application callers in every mode.
int benchmark_iteration(StackCalculator &calculator, int value) {
  const int result = library_dispatch(calculator, value, 3);
  return result;
}

std::uint64_t nanoseconds(clockid_t clock) {
  timespec now{};
  if (::clock_gettime(clock, &now) != 0) {
    return 0;
  }
  return std::uint64_t(now.tv_sec) * 1000000000ULL + now.tv_nsec;
}

int main() {
  // Match the existing attach examples: wait in nanosleep between batches,
  // outside all timed intervals, rather than blocking in a pipe read.
  if (::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0)
    return 2;
  std::printf("READY %ld\n", static_cast<long>(::getpid()));
  std::fflush(stdout);
  std::uint64_t count = 0;
  for (;;) {
    if (std::scanf("%" SCNu64, &count) != 1) {
      std::clearerr(stdin);
      ::usleep(1000);
      continue;
    }
    if (!count)
      break;
    if (count > 20000000) {
      return 2;
    }
    StackCalculator calculator;
    std::uint64_t checksum = 0;
    const auto cpu_begin = nanoseconds(CLOCK_THREAD_CPUTIME_ID);
    const auto wall_begin = nanoseconds(CLOCK_MONOTONIC_RAW);
    for (std::uint64_t i = 0; i < count; ++i) {
      checksum += benchmark_iteration(calculator, static_cast<int>(i % 32) + 1);
    }
    const auto wall_end = nanoseconds(CLOCK_MONOTONIC_RAW);
    const auto cpu_end = nanoseconds(CLOCK_THREAD_CPUTIME_ID);
    const auto remainder = count % 32;
    const auto input_sum = (count / 32) * 528 + remainder * (remainder + 1) / 2;
    const auto expected = 3 * input_sum + 7 * count + count * (count + 1) / 2;
    const bool valid = checksum == expected && calculator.calls == int(count);
    std::printf("RESULT,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%d\n",
                count, wall_end - wall_begin, cpu_end - cpu_begin, checksum,
                valid);
    std::fflush(stdout);
    if (!valid) {
      return 1;
    }
  }
}

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <ctime>
#include <thread>
#include <vector>
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

// Only the patchable endpoint uses optnone. The driver and worker loops use -O2.
__attribute__((noinline, optnone)) int bench_leaf(int value) {
  volatile int saved = value;
  volatile int result = saved * 3 + 7;
  return result;
}
std::uint64_t clock_ns(clockid_t id) {
  timespec value{};
  clock_gettime(id, &value);
  return std::uint64_t(value.tv_sec) * 1000000000ULL + value.tv_nsec;
}
thread_local std::vector<std::uint64_t> samples;
std::atomic<bool> go{false};
int sample_stride = 0;
int pace_us = 0;
__attribute__((noinline)) long long bench_root(int count, int workers) {
  std::vector<std::thread> threads;
  std::vector<long long> sums(workers);
  auto work = [&](int worker) {
    while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
    long long sum = 0;
    for (int i = worker; i < count; i += workers) {
      if (workers == 1 && sample_stride && i % sample_stride == 0) {
        auto begin = clock_ns(CLOCK_MONOTONIC_RAW);
        sum += bench_leaf(i % 32 + 1);
        samples.push_back(clock_ns(CLOCK_MONOTONIC_RAW) - begin);
      } else sum += bench_leaf(i % 32 + 1);
      if (pace_us) usleep(pace_us);
    }
    sums[worker] = sum;
  };
  if (workers == 1) {
    go.store(true, std::memory_order_release);
    work(0);
  } else {
    go.store(false, std::memory_order_release);
    for (int i = 0; i < workers; ++i) threads.emplace_back(work, i);
    go.store(true, std::memory_order_release);
    for (auto &thread : threads) thread.join();
  }
  long long total = 0;
  for (auto sum : sums) total += sum;
  return total;
}
int main() {
  fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
  std::printf("READY %ld\n", long(getpid())); std::fflush(stdout);
  int count = 0, workers = 1, stride = 0, pace = 0;
  for (;;) {
    if (std::scanf("%d %d %d %d", &count, &workers, &stride, &pace) != 4) {
      std::clearerr(stdin); usleep(1000); continue;
    }
    if (!count) return 0;
    if (count < 0 || count > 100000000 || workers < 1 || workers > 8 || stride < 0 || pace < 0) return 2;
    sample_stride = stride;
    pace_us = pace;
    samples.clear(); samples.reserve(stride ? count / stride + 1 : 0);
    rusage before{}, after{}; getrusage(RUSAGE_SELF, &before);
    const auto cpu_begin = clock_ns(CLOCK_PROCESS_CPUTIME_ID);
    const auto wall_begin = clock_ns(CLOCK_MONOTONIC_RAW);
    const auto checksum = bench_root(count, workers);
    const auto wall = clock_ns(CLOCK_MONOTONIC_RAW) - wall_begin;
    const auto cpu = clock_ns(CLOCK_PROCESS_CPUTIME_ID) - cpu_begin;
    getrusage(RUSAGE_SELF, &after);
    const long long remainder = count % 32;
    const auto expected = 3 * ((count / 32) * 528LL + remainder * (remainder + 1) / 2) + 7LL * count;
    std::printf("RESULT,%d,%d,%" PRIu64 ",%" PRIu64 ",%lld,%d,%ld,%ld,%ld,%ld\n", count, workers, wall, cpu, checksum, checksum == expected, after.ru_maxrss, after.ru_minflt-before.ru_minflt, after.ru_nvcsw-before.ru_nvcsw, after.ru_nivcsw-before.ru_nivcsw);
    if (!samples.empty()) {
      std::printf("SAMPLES");
      for (auto value : samples) std::printf(",%" PRIu64, value);
      std::printf("\n");
    }
    std::fflush(stdout);
    if (checksum != expected) return 1;
  }
}

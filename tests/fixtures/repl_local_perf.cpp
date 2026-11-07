#include <tracer/launch.hpp>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>

#include <sys/syscall.h>
#include <unistd.h>

extern "C" char tracer_perf_hot_begin[], tracer_perf_hot_end[];

[[gnu::noinline]] void hot_loop() {
  asm volatile(".globl tracer_perf_hot_begin\ntracer_perf_hot_begin:" ::: "memory");
  volatile unsigned value = 0;
  for (unsigned i = 0; i < 100000; ++i)
    value = value * 3 + i;
  asm volatile(".globl tracer_perf_hot_end\ntracer_perf_hot_end:" ::: "memory");
}

int main(int argc, char **argv) {
  if (!tracer::launch::wait_for_resume())
    return 125;
  const bool syscalls = argc > 1 && std::strcmp(argv[1], "syscalls") == 0;
  std::atomic<bool> stop{false};
  std::thread worker([&] {
    std::printf("worker tid=%ld hot=%p-%p\n", ::syscall(SYS_gettid),
                static_cast<void *>(tracer_perf_hot_begin),
                static_cast<void *>(tracer_perf_hot_end));
    std::fflush(stdout);
    while (!stop.load()) {
      if (!syscalls) {
        hot_loop();
      } else if (::access("burst", F_OK) == 0) {
        for (unsigned i = 0; i < 4096; ++i)
          ::syscall(SYS_getppid);
      } else {
        ::usleep(5000);
      }
    }
  });
  while (::access("exit-target", F_OK) != 0)
    ::usleep(10'000);
  stop = true;
  worker.join();
}

#include "person.hpp"
#include "batch.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#ifndef PR_SET_PTRACER_ANY
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif

namespace {
using Clock = std::chrono::steady_clock;

std::uint64_t thread_cpu_ns() {
  timespec usage{};
  if (::clock_gettime(CLOCK_THREAD_CPUTIME_ID, &usage) != 0)
    std::abort();
  return static_cast<std::uint64_t>(usage.tv_sec) * 1000000000 + usage.tv_nsec;
}

struct Worker {
  std::atomic<long> tid{0};
  std::atomic<std::size_t> completed{0};
  std::atomic<bool> finished{false};
  Clock::time_point start, end;
  std::uint64_t cpu = 0;
  std::thread thread;
};
}

int main(int argc, char **argv) {
  char *end = nullptr;
  const auto count = argc == 2 ? std::strtoul(argv[1], &end, 10) : 0;
  if (!count || count % 2 || *end || ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0 ||
      ::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0)
    return 2;
  pthread_barrier_t start;
  if (::pthread_barrier_init(&start, nullptr, 3) != 0)
    return 3;
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  std::vector<demo::Person> people;
  people.reserve(count);
  std::array<Worker, 2> workers;
  const auto per_worker = count / workers.size();
  for (std::size_t i = 0; i < workers.size(); ++i) {
    auto &worker = workers[i];
    worker.thread = std::thread([&, i] {
      auto &self = workers[i];
      self.tid.store(::syscall(SYS_gettid), std::memory_order_release);
      ::pthread_barrier_wait(&start);
      demo::WriteBatch batch{people.data() + i * per_worker, per_worker, &self.completed};
      const auto cpu = thread_cpu_ns();
      self.start = Clock::now();
      batch.run();
      self.end = Clock::now();
      self.cpu = thread_cpu_ns() - cpu;
      self.finished.store(true, std::memory_order_release);
    });
  }
  while (!workers[0].tid.load(std::memory_order_acquire) ||
         !workers[1].tid.load(std::memory_order_acquire))
    ::usleep(1000);
  std::printf("READY tids=%ld,%ld\n", workers[0].tid.load(), workers[1].tid.load());

  bool started = false, joined = false;
  for (;;) {
    if (started && !joined && workers[0].finished.load(std::memory_order_acquire) &&
        workers[1].finished.load(std::memory_order_acquire)) {
      for (auto &worker : workers)
        worker.thread.join();
      joined = true;
      const auto begin = std::min(workers[0].start, workers[1].start);
      const auto end_time = std::max(workers[0].end, workers[1].end);
      const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - begin);
      std::array<std::size_t, 2> correct{};
      for (std::size_t i = 0; i < people.size(); ++i)
        correct[i / per_worker] += people[i].age == 1;
      std::printf("CPU ns=%llu,%llu\n", static_cast<unsigned long long>(workers[0].cpu),
                  static_cast<unsigned long long>(workers[1].cpu));
      std::printf("DONE ns=%lld completed=%zu,%zu correct=%zu,%zu\n",
                  static_cast<long long>(elapsed.count()), workers[0].completed.load(),
                  workers[1].completed.load(), correct[0], correct[1]);
      if (correct[0] != per_worker || correct[1] != per_worker)
        return 4;
    }

    char command = 0;
    const auto read_count = ::read(STDIN_FILENO, &command, 1);
    if (read_count <= 0) {
      if (!read_count || (errno != EAGAIN && errno != EINTR))
        return joined ? 0 : 5;
      ::usleep(1000);
    } else if (command == 's' && people.empty()) {
      for (std::size_t i = 0; i < count; ++i)
        people.emplace_back(42);
      std::printf("SETUP base=%p stride=%zu offset=%zu\n",
                  static_cast<void *>(people.data()), sizeof(demo::Person),
                  offsetof(demo::Person, age));
    } else if (command == 'g' && people.size() == count && !started) {
      started = true;
      ::pthread_barrier_wait(&start);
    } else if (command == 'p') {
      std::printf("PROGRESS completed=%zu,%zu\n",
                  workers[0].completed.load(), workers[1].completed.load());
    } else if (command == 'q' && joined) {
      ::pthread_barrier_destroy(&start);
      std::puts("EXIT");
      return 0;
    }
  }
}

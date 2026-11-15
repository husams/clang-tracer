#include "person.hpp"

#include <array>
#include <atomic>
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string_view>
#include <thread>

#include <fcntl.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#ifndef PR_SET_PTRACER_ANY
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif

namespace {
enum class Operation { store, add, cas };
struct Worker {
  std::atomic<long> tid{0};
  std::atomic<unsigned long> completed{0};
  std::atomic<bool> finished{false};
  long long result = 0;
  std::thread thread;
};
}

int main(int argc, char **argv) {
  const bool paced = argc == 4 && std::string_view(argv[3]) == "paced";
  if (argc != 3 && !paced)
    return 2;
  const std::string_view name = argv[1];
  Operation operation;
  if (name == "store")
    operation = Operation::store;
  else if (name == "add")
    operation = Operation::add;
  else if (name == "cas")
    operation = Operation::cas;
  else
    return 2;
  char *end = nullptr;
  const auto iterations = std::strtoul(argv[2], &end, 10);
  if (!iterations || *end || iterations > (INT_MAX - 42L) / 2 ||
      (operation != Operation::add && iterations != 1) ||
      ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0 ||
      ::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0)
    return 2;
  pthread_barrier_t start;
  if (::pthread_barrier_init(&start, nullptr, 3) != 0)
    return 3;
  std::setvbuf(stdout, nullptr, _IOLBF, 0);

  std::optional<demo::Person> person;
  std::array<Worker, 2> workers;
  for (std::size_t i = 0; i < workers.size(); ++i) {
    workers[i].thread = std::thread([&, i] {
      auto &self = workers[i];
      self.tid.store(::syscall(SYS_gettid), std::memory_order_release);
      ::pthread_barrier_wait(&start);
      for (unsigned long n = 0; n < iterations; ++n) {
        if (operation == Operation::store)
          demo::store_age(*person, static_cast<int>(i) + 1);
        else if (operation == Operation::add)
          self.result += demo::add_age(*person);
        else
          self.result = demo::change_age(*person, i == 0 ? 7 : 9);
        self.completed.store(n + 1, std::memory_order_relaxed);
        if (paced)
          ::usleep(100);
      }
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
      const int value = person->age.load(std::memory_order_relaxed);
      const auto first = workers[0].result, second = workers[1].result;
      std::printf("DONE value=%d completed=%lu,%lu results=%lld,%lld\n", value,
                  workers[0].completed.load(), workers[1].completed.load(), first, second);
      const auto writes = 2LL * iterations;
      if ((operation == Operation::store && value != 1 && value != 2) ||
          (operation == Operation::add &&
           (value != 42 + writes || first + second != writes * (84 + writes - 1) / 2)) ||
          (operation == Operation::cas &&
           (first + second != 1 || value != (first ? 7 : 9))))
        return 4;
    }

    char command = 0;
    const auto read_count = ::read(STDIN_FILENO, &command, 1);
    if (read_count <= 0) {
      if (!read_count || (errno != EAGAIN && errno != EINTR))
        return joined ? 0 : 5;
      ::usleep(1000);
    } else if (command == 's' && !person) {
      person.emplace(42);
      std::printf("SETUP address=%p age=%p\n", static_cast<void *>(&*person),
                  static_cast<void *>(&person->age));
    } else if (command == 'g' && person && !started) {
      started = true;
      ::pthread_barrier_wait(&start);
    } else if (command == 'p') {
      std::printf("PROGRESS completed=%lu,%lu\n",
                  workers[0].completed.load(), workers[1].completed.load());
    } else if (command == 'q' && joined) {
      ::pthread_barrier_destroy(&start);
      std::puts("EXIT");
      return 0;
    }
  }
}

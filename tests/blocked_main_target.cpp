#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <array>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <thread>
#include <time.h>

#include <linux/futex.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

extern "C" int blocked_worker_probe(int value);

namespace {

alignas(4) std::uint32_t futex_word = 0;
std::atomic<bool> running{true};
std::atomic<pid_t> worker_tid{0};

void futex_block_forever() {
  while (running.load(std::memory_order_relaxed)) {
    (void)::syscall(SYS_futex, &futex_word, FUTEX_WAIT_PRIVATE, 0, nullptr,
                    nullptr, 0);
  }
}

[[noreturn]] void block_leader(const char *mode) {
  if (std::string_view(mode) == "--leader-epoll") {
    const int epoll_fd = ::epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0) {
      std::abort();
    }
    epoll_event event{};
    for (;;) {
      (void)::epoll_wait(epoll_fd, &event, 1, -1);
    }
  }
  if (std::string_view(mode) == "--leader-read") {
    int descriptors[2]{};
    if (::pipe(descriptors) != 0) {
      std::abort();
    }
    char byte = 0;
    for (;;) {
      (void)::read(descriptors[0], &byte, 1);
    }
  }
  for (;;) {
    futex_block_forever();
  }
}

void *run_probe_worker(void *argument) {
  const bool hot_worker = *static_cast<const bool *>(argument);
  worker_tid.store(static_cast<pid_t>(::syscall(SYS_gettid)),
                   std::memory_order_release);
  int value = 1;
  while (running.load(std::memory_order_relaxed)) {
    const int argument_value = value;
    if (blocked_worker_probe(argument_value) != argument_value + 41) {
      std::abort();
    }
    value = value == 1000 ? 1 : value + 1;
    if (!hot_worker) {
      timespec delay{0, 100'000'000};
      (void)::syscall(SYS_nanosleep, &delay, nullptr);
    } else {
      // Keep the worker in application code between probe invocations so
      // attach/detach can observe a stable userspace CFI chain.
      for (unsigned int spin = 0; spin < 10'000; ++spin) {
        asm volatile("pause" ::: "memory");
      }
    }
  }
  return nullptr;
}

} // namespace

extern "C" __attribute__((visibility("default"))) std::atomic<std::uint64_t>
    blocked_worker_calls{0};

extern "C" __attribute__((visibility("default"), noinline)) int
blocked_worker_probe(int value) {
  blocked_worker_calls.fetch_add(1, std::memory_order_relaxed);
  return value + 41;
}

int main(int argc, char **argv) {
  if (argc != 3 || (std::string_view(argv[1]) != "--hot-worker" &&
                    std::string_view(argv[1]) != "--sleep-worker" &&
                    std::string_view(argv[1]) != "--all-blocked") ||
      (std::string_view(argv[2]) != "--leader-futex" &&
       std::string_view(argv[2]) != "--leader-epoll" &&
       std::string_view(argv[2]) != "--leader-read")) {
    return 2;
  }
  if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) != 0) {
    return 4;
  }
  const std::string_view mode(argv[1]);
  const bool active_worker = mode != "--all-blocked";
  bool hot_worker = mode == "--hot-worker";
  std::thread sleeping_worker;
  pthread_t hot_worker_thread{};
  std::array<std::thread, 2> blocked;
  if (active_worker) {
    if (hot_worker) {
      if (::pthread_create(&hot_worker_thread, nullptr, run_probe_worker,
                           &hot_worker) != 0) {
        return 3;
      }
    } else {
      sleeping_worker =
          std::thread([&] { run_probe_worker(&hot_worker); });
    }
  }
  for (auto &thread : blocked) {
    thread = std::thread(futex_block_forever);
  }
  while (active_worker && worker_tid.load(std::memory_order_acquire) == 0) {
    std::this_thread::yield();
  }
  std::printf("READY pid=%ld worker=%ld entry=%p calls=%p mode=%s\n",
              static_cast<long>(::getpid()),
              static_cast<long>(worker_tid.load(std::memory_order_acquire)),
              reinterpret_cast<void *>(&blocked_worker_probe),
              static_cast<void *>(&blocked_worker_calls),
              hot_worker ? "hot-worker"
                         : (active_worker ? "sleep-worker" : "all-blocked"));
  std::fflush(stdout);
  block_leader(argv[2]);
  return 0;
}

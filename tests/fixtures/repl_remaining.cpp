#include "tracer/override_target.hpp"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <setjmp.h>
#include <string>
#include <string_view>
#include <thread>

#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
volatile unsigned char writable_byte = 0x13;
std::atomic<int> original_calls{0};
sigjmp_buf recovery;
struct Failure { int value; };
std::atomic<bool> run_worker{true};
std::atomic<bool> capture_requested{false};
std::atomic<long> worker_tid{0};
std::atomic<int> completed{0};

void fault_handler(int, siginfo_t *, void *) {
  siglongjmp(recovery, 1);
}

__attribute__((noinline)) int indirect_destination(int value) {
  return value + 100;
}

__attribute__((noinline)) int direct_destination(int value) {
  return value + 200;
}

int (*volatile dynamic_destination)(int) = indirect_destination;
} // namespace

TRACER_SCALAR_OVERRIDE_TARGET int remaining_observed(int value) {
  asm volatile(".rept 16\n\tnop\n\t.endr" ::: "memory");
  ++original_calls;
  errno = ERANGE;
  if (value == 9)
    throw Failure{91};
  return 40 + value;
}

extern "C" __attribute__((noinline)) int remaining_discovery_entry(int value) {
  const int direct = direct_destination(value);
  return direct + dynamic_destination(value);
}

int main(int argc, char **argv) {
  if (argc != 3)
    return 1;
  const bool child = std::string_view(argv[1]) == "child";
  const std::string control = argv[2], child_control = control + ".child";
  struct sigaction action {};
  action.sa_sigaction = fault_handler;
  action.sa_flags = SA_SIGINFO;
  if (::sigaction(SIGSEGV, &action, nullptr) != 0)
    return 2;
  void *protected_page = ::mmap(nullptr, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (protected_page == MAP_FAILED)
    return 3;

  std::thread worker([] {
    worker_tid.store(::syscall(SYS_gettid));
    while (run_worker.load()) {
      if (capture_requested.exchange(false)) {
        const auto discovered = remaining_discovery_entry(2);
        const auto observed = remaining_observed(2);
        std::printf("capture result=%d observed=%d serial=%d\n",
                    discovered, observed, ++completed);
        std::fflush(stdout);
      }
      ::usleep(5000);
    }
  });
  while (!worker_tid.load())
    ::usleep(1000);
  std::printf("%s pid=%d tid=%ld data=%p entry=%p direct=%p indirect=%p\n",
              child ? "child-ready" : "ready", ::getpid(), worker_tid.load(),
              const_cast<unsigned char *>(&writable_byte),
              reinterpret_cast<void *>(&remaining_discovery_entry),
              reinterpret_cast<void *>(&direct_destination),
              reinterpret_cast<void *>(&indirect_destination));
  std::fflush(stdout);

  for (;;) {
    char command = 0;
    if (std::ifstream input(control); input >> command) {
      ::unlink(control.c_str());
      switch (command) {
      case 'v': {
        const int before = original_calls.load();
        errno = 0;
        const int result = remaining_observed(2);
        std::printf("value=%d calls=%d errno=%d\n", result, original_calls - before, errno);
        break;
      }
      case 'e': {
        const int before = original_calls.load();
        try {
          (void)remaining_observed(9);
        } catch (const Failure &failure) {
          std::printf("caught=%d calls=%d errno=%d\n", failure.value, original_calls - before, errno);
        }
        break;
      }
      case 'w':
        std::printf("writable=%u\n", writable_byte);
        break;
      case 'i':
        capture_requested.store(true);
        break;
      case 'n':
        if (sigsetjmp(recovery, 1) == 0)
          asm volatile("movb $1,(%0)" :: "r"(protected_page) : "memory");
        else
          std::puts("fault-recovered");
        break;
      case 'a': {
        void *first = std::malloc(12347);
        void *second = std::calloc(7, 163);
        first = std::realloc(first, 23459);
        asm volatile("" :: "r"(first), "r"(second) : "memory");
        std::free(first);
        std::free(second);
        std::puts("allocations-done");
        break;
      }
      case 'f': {
        const char *path = child_control.c_str();
        const auto pid = ::fork();
        if (pid == 0) {
          ::execl(argv[0], argv[0], "child", path, static_cast<char *>(nullptr));
          ::_exit(127);
        }
        std::printf("child-started=%d\n", pid);
        break;
      }
      case 'q':
        run_worker.store(false);
        worker.join();
        return 0;
      }
      std::fflush(stdout);
    }
    for (int status = 0, exited; (exited = ::waitpid(-1, &status, WNOHANG)) > 0;) {
      std::printf("child-exited=%d status=%d\n", exited, status);
      std::fflush(stdout);
    }
    ::usleep(5000);
  }
}

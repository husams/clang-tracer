#include <atomic>
#include <cerrno>
#include <cstdio>
#include <thread>

#include <fcntl.h>
#include <gnu/libc-version.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

extern "C" unsigned long pilot_work(unsigned, int, int, int);
extern "C" char pilot_work_end[];

namespace {
std::atomic<int> pending{0};
std::atomic<int> worker_tid{0};
}

// These entries are ordinary application functions before attachment.
[[gnu::noinline]] int pilot_control(int command, int tid, unsigned long begin,
                                    unsigned long end, int limit) {
  asm volatile(".rept 16\n\tnop\n\t.endr" ::: "memory");
  return command + (tid > 0 && begin < end && limit > 0);
}

[[gnu::noinline]] int pilot_observed(int rounds) {
  asm volatile(".rept 16\n\tnop\n\t.endr" ::: "memory");
  return static_cast<int>(pilot_work(rounds, getpid(), ::syscall(SYS_gettid), 0)) + 10;
}

int main() {
  const auto parent = getppid();
  if (parent <= 1 || prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 || getppid() != parent)
    return 1;
  // The controller and its descendants include the independently launched
  // native REPL server. Keep this fixture's authorization explicit.
#ifdef PR_SET_PTRACER
  if (prctl(PR_SET_PTRACER, static_cast<unsigned long>(parent), 0UL, 0UL, 0UL) != 0 &&
      errno != EINVAL)
    return 1;
#endif
  if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0)
    return 1;
  std::thread worker([] {
    worker_tid = ::syscall(SYS_gettid);
    for (;;) {
      const auto rounds = pending.exchange(0);
      if (rounds < 0)
        return;
      if (rounds) {
        const auto value = pilot_observed(rounds);
        std::printf("worker returned %d\n", value);
        std::fflush(stdout);
      }
      ::usleep(5000);
    }
  });
  while (!worker_tid.load())
    ::usleep(1000);
  const auto begin = reinterpret_cast<unsigned long>(&pilot_work);
  const auto end = reinterpret_cast<unsigned long>(pilot_work_end);
  std::printf("ready tid=%d begin=0x%lx end=0x%lx probe=%p glibc=%s\n",
              worker_tid.load(), begin, end, reinterpret_cast<void *>(&pilot_observed),
              gnu_get_libc_version());
  std::fflush(stdout);
  for (;;) {
    char command;
    const auto count = ::read(STDIN_FILENO, &command, 1);
    if (count == 0 || (count == 1 && command == 'q'))
      break;
    if (count == 1) {
      if (command == 'w' || command == 'v') {
        pending.store(command == 'w' ? 3 : 2);
      } else if (command == 'b' || command == 'c' || command == 'e') {
        const auto value = pilot_control(command, worker_tid.load(), begin, end,
                                         command == 'c' ? 7 : 256);
        std::printf("control returned %d\n", value);
        std::fflush(stdout);
      }
    }
    ::usleep(5000);
  }
  pending.store(-1);
  worker.join();
}

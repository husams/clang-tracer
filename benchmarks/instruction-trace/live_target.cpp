#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <thread>

#include <fcntl.h>
#include <gnu/libc-version.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

extern "C" unsigned long pilot_work(unsigned, int, int, int);
extern "C" int pilot_fault(const void *);
extern "C" char pilot_live_work_end[];
extern "C" unsigned long pilot_memory(unsigned long *, unsigned long);
extern "C" char pilot_memory_end[];

namespace {
std::atomic<int> pending{0};
std::atomic<int> worker_tid{0};
void *fault_page = nullptr;
alignas(16) unsigned long watched[2]{10, 0};
std::size_t page_size = 0;
thread_local volatile sig_atomic_t received_signals = 0;
thread_local volatile sig_atomic_t recovered_faults = 0;

void on_signal(int signal, siginfo_t *info, void *) {
  if (info->si_code != SI_TKILL || info->si_pid != getpid())
    _exit(128 + signal);
  ++received_signals;
}

void on_fault(int signal, siginfo_t *info, void *) {
  const int saved_errno = errno;
  // Retry the actual faulting instruction by restoring access to this page.
  if (info->si_code != SEGV_ACCERR || info->si_addr != fault_page ||
      ::syscall(SYS_mprotect, fault_page, page_size, PROT_READ) != 0)
    _exit(128 + signal);
  ++recovered_faults;
  errno = saved_errno;
}

bool install_handler(int signal, void (*handler)(int, siginfo_t *, void *)) {
  struct sigaction action {};
  action.sa_sigaction = handler;
  action.sa_flags = SA_SIGINFO;
  return sigemptyset(&action.sa_mask) == 0 && sigaction(signal, &action, nullptr) == 0;
}
}

// These entries are ordinary application functions before attachment.
[[gnu::noinline]] int pilot_control(int command, int tid, unsigned long begin,
                                    unsigned long end, int limit) {
  asm volatile(".rept 16\n\tnop\n\t.endr" ::: "memory");
  return command + (tid > 0 && begin < end && limit > 0);
}

[[gnu::noinline]] int pilot_observed(int rounds) {
  asm volatile(".rept 16\n\tnop\n\t.endr" ::: "memory");
  if (rounds == 0)
    return pilot_fault(fault_page) + 10;
  if (rounds == -4) {
    watched[0] = 10;
    return static_cast<int>(pilot_memory(watched, 7)) + 10;
  }
  const auto count = static_cast<unsigned>(rounds < 0 ? -rounds : rounds);
  return static_cast<int>(pilot_work(count, getpid(), ::syscall(SYS_gettid), rounds < 0)) + 10;
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
  const auto size = sysconf(_SC_PAGESIZE);
  if (size <= 0)
    return 1;
  page_size = static_cast<std::size_t>(size);
  fault_page = mmap(nullptr, page_size, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (fault_page == MAP_FAILED || !install_handler(SIGUSR1, on_signal) ||
      !install_handler(SIGSEGV, on_fault))
    return 1;
  *static_cast<int *>(fault_page) = 29;
  std::thread worker([] {
    worker_tid = ::syscall(SYS_gettid);
    for (;;) {
      const auto command = pending.exchange(0);
      if (command < 0)
        return;
      if (command) {
        const auto rounds = command == 'm' ? -4 : command == 'f' ? 0 :
                            command == 's' ? -3 : command == 'v' ? 2 : 3;
        if (command == 'f' && mprotect(fault_page, page_size, PROT_NONE) != 0)
          _exit(2);
        const auto signals_before = received_signals;
        const auto faults_before = recovered_faults;
        if (command == 'm')
          errno = E2BIG;
        const auto value = pilot_observed(rounds);
        if (command == 'm' && errno != E2BIG)
          _exit(3);
        std::printf("worker returned %d signals=%d faults=%d\n", value,
                    received_signals - signals_before, recovered_faults - faults_before);
        std::fflush(stdout);
      }
      ::usleep(5000);
    }
  });
  while (!worker_tid.load())
    ::usleep(1000);
  const auto begin = reinterpret_cast<unsigned long>(&pilot_work);
  const auto end = reinterpret_cast<unsigned long>(pilot_live_work_end);
  std::printf("ready tid=%d begin=0x%lx end=0x%lx probe=%p glibc=%s\n",
              worker_tid.load(), begin, end, reinterpret_cast<void *>(&pilot_observed),
              gnu_get_libc_version());
  std::printf("memory begin=%p end=%p data=%p\n", reinterpret_cast<void *>(&pilot_memory),
              reinterpret_cast<void *>(pilot_memory_end), static_cast<void *>(watched));
  std::fflush(stdout);
  for (;;) {
    char command;
    const auto count = ::read(STDIN_FILENO, &command, 1);
    if (count == 0 || (count == 1 && command == 'q'))
      break;
    if (count == 1) {
      if (command == 'w' || command == 'v' || command == 's' || command == 'f' || command == 'm') {
        pending.store(command);
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
  return munmap(fault_page, page_size) == 0 ? 0 : 1;
}

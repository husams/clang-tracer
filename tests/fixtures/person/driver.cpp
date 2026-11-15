#include "person.hpp"

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#ifndef PR_SET_PTRACER_ANY
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif

namespace {
std::atomic<std::uintptr_t> fault_pc{0};
std::atomic<std::uintptr_t> protected_address{0};
static_assert(decltype(fault_pc)::is_always_lock_free);
volatile sig_atomic_t faults = 0;
volatile sig_atomic_t skip_fault = 0;

void write_fault(int sig, siginfo_t *info, void *context) {
  const auto address = protected_address.load(std::memory_order_relaxed);
  if (sig != SIGSEGV || info->si_code != SEGV_ACCERR || faults != 0 ||
      reinterpret_cast<std::uintptr_t>(info->si_addr) != address)
    ::_exit(9);
  faults = 1;
  auto &pc = static_cast<ucontext_t *>(context)->uc_mcontext.gregs[REG_RIP];
  fault_pc.store(pc, std::memory_order_relaxed);
  if (skip_fault)
    pc += 6; // The verified MOV32mi encoding in demo::update.
  if (::syscall(SYS_mprotect, address, sizeof(demo::Person), PROT_READ | PROT_WRITE))
    ::_exit(10);
}

char next_command() {
  for (;;) {
    char command = 0;
    const auto count = ::read(STDIN_FILENO, &command, 1);
    if (count == 1 && command != '\n')
      return command;
    if (!count || (count < 0 && errno != EAGAIN && errno != EINTR))
      return 'q';
    ::usleep(10000);
  }
}
}

int main(int argc, char **argv) {
  if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0 ||
      ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0)
    return 2;
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  const bool write_only = argc == 2 && std::strcmp(argv[1], "write-only") == 0;
  const bool skip = argc == 2 && std::strcmp(argv[1], "fault-skip") == 0;
  const bool handled = skip || (argc == 2 && std::strcmp(argv[1], "fault-retry") == 0);
  const bool fatal = argc == 2 && std::strcmp(argv[1], "fault-default") == 0;
  const bool ignored = argc == 2 && std::strcmp(argv[1], "fault-ignored") == 0;
  const bool divide = argc == 2 && std::strcmp(argv[1], "fault-divide") == 0;
  const bool fault = handled || fatal || ignored;
  alignas(demo::Person) unsigned char local[sizeof(demo::Person)];
  void *storage = local;
  if (write_only || fault) {
    storage = ::mmap(nullptr, sizeof(demo::Person), write_only ? PROT_WRITE : PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (storage == MAP_FAILED)
      return 6;
  }
  if (fatal || ignored || divide) {
    struct rlimit limit{};
    if (::setrlimit(RLIMIT_CORE, &limit) || (ignored && ::signal(SIGSEGV, SIG_IGN) == SIG_ERR))
      return 11;
  }
  if (handled) {
    protected_address.store(reinterpret_cast<std::uintptr_t>(storage), std::memory_order_relaxed);
    skip_fault = skip;
    struct sigaction action{};
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = write_fault;
    if (::sigaction(SIGSEGV, &action, nullptr))
      return 11;
  }
  std::printf("READY address=%p\n", storage);
  if (next_command() != 'g')
    return 3;

  auto &person = *::new (storage) demo::Person{42};
  if (fault && ::mprotect(storage, sizeof(person), PROT_READ))
    return 12;
  if (divide) {
    demo::remainder(person, 0);
    return 15;
  }
  errno = EDOM;
  demo::update(person);
  if (errno != EDOM)
    return 14;
  if (fault) {
    std::printf("FAULT pc=%p count=%d first_age=%d\n",
                reinterpret_cast<void *>(fault_pc.load()), static_cast<int>(faults), person.age);
    if (faults != 1 || person.age != (skip ? 42 : 1))
      return 13;
  }
  demo::update(person);
  std::puts("STORES_DONE");
  if (next_command() != 'r' ||
      (write_only && ::mprotect(storage, sizeof(person), PROT_READ | PROT_WRITE) != 0))
    return 7;
  std::printf("age=%d address=%p\n", person.age, static_cast<void *>(&person));
  if (person.age != 1 || next_command() != 'v')
    return 4;
  person.age = 42;
  demo::update(person);
  std::printf("after_detach_age=%d\n", person.age);
  if (person.age != 1 || next_command() != 'q')
    return 5;
  person.~Person();
  if ((write_only || fault) && ::munmap(storage, sizeof(person)) != 0)
    return 8;
  std::puts("EXIT");
}

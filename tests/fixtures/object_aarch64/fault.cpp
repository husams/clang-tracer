#include <csignal>
#include <cstdio>
#include <cstring>
#include <new>
#include <ucontext.h>

#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(TRACE_ARM_TEST_RENDEZVOUS)
#include "rendezvous.hpp"
#endif

namespace arm_test {
struct Record {
  int value;
  Record();
  ~Record();
};

__attribute__((noinline, noipa)) Record::Record() : value(17) {}
__attribute__((noinline, noipa)) Record::~Record() { asm volatile("" ::: "memory"); }

extern "C" __attribute__((noinline, noipa)) void arm_write_value(Record &record, int value) {
  record.value = value;
}
}

namespace {
void *page;
std::size_t page_size;
bool skip;
bool park;
volatile sig_atomic_t delivered;
unsigned long long fault_pc;

void handle_fault(int sig, siginfo_t *info, void *context) {
  if (sig != SIGSEGV || info->si_code != SEGV_ACCERR || info->si_addr != page || delivered ||
      ::syscall(SYS_mprotect, page, page_size, PROT_READ | PROT_WRITE))
    ::_exit(81);
  auto &pc = static_cast<ucontext_t *>(context)->uc_mcontext.pc;
  fault_pc = pc;
  if (park) {
    constexpr char message[] = "BODY_FAULT\n";
    char release;
    if (::write(STDOUT_FILENO, message, sizeof(message) - 1) != sizeof(message) - 1 ||
        ::read(STDIN_FILENO, &release, 1) != 1)
      ::_exit(82);
  }
  if (skip)
    pc += 4;
  delivered = 1;
}
}

int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  const bool paced = !std::strcmp(argv[1], "paced");
  skip = !std::strcmp(argv[1], "skip");
  park = !std::strcmp(argv[1], "park");
  struct sigaction action{};
  action.sa_flags = SA_SIGINFO;
  action.sa_sigaction = handle_fault;
  ::sigemptyset(&action.sa_mask);
  page_size = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
  page = ::mmap(nullptr, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (page == MAP_FAILED || ::sigaction(SIGSEGV, &action, nullptr))
    return 3;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
#if defined(TRACE_ARM_TEST_RENDEZVOUS)
  arm_rendezvous();
#else
  while (::access("write.now", F_OK))
    ::usleep(1000);
#endif
  auto *record = new (page) arm_test::Record;
  if (paced) {
    std::puts("STARTED");
    for (int i = 1; i <= 1000; ++i) {
      arm_test::arm_write_value(*record, 17 + i);
      ::usleep(1000);
    }
    std::printf("DONE count=1000 value=%d\n", record->value);
  } else {
    if (::mprotect(page, page_size, PROT_READ))
      return 4;
    arm_test::arm_write_value(*record, 19);
    std::printf("FAULT count=%d pc=%#llx value=%d\n", int(delivered), fault_pc, record->value);
    arm_test::arm_write_value(*record, 23);
    std::printf("DONE value=%d\n", record->value);
  }
  while (::access("stop.now", F_OK))
    ::usleep(1000);
  const auto result = record->value == (paced ? 1017 : 23) ? 0 : 5;
  record->~Record();
  ::munmap(page, page_size);
  return result;
}

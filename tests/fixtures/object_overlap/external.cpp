#include <cstdio>
#include <new>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__aarch64__)
#include "../object_aarch64/rendezvous.hpp"
#endif

namespace overlap_test {
struct Record {
  int value;
  Record();
  ~Record();
};

__attribute__((noinline, noclone)) Record::Record() : value(17) {
  asm volatile("nop\n nop\n nop\n nop\n nop" ::: "memory");
}
__attribute__((noinline, noclone)) Record::~Record() {
  std::puts("RETIRED");
  asm volatile("nop\n nop\n nop\n nop\n nop" ::: "memory");
}

extern "C" __attribute__((noinline, noclone)) void overlap_write(Record &record, int value) {
  record.value = value;
  asm volatile("nop\n nop\n nop\n nop\n nop" ::: "memory");
}
}

int main() {
  auto *storage = ::mmap(nullptr, sizeof(overlap_test::Record), PROT_READ | PROT_WRITE,
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (storage == MAP_FAILED)
    return 1;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
#if defined(__aarch64__)
  arm_rendezvous();
#else
  while (::access("write.now", F_OK))
    ::usleep(1000);
#endif
  auto *record = new (storage) overlap_test::Record;
  overlap_test::overlap_write(*record, 1000);
  const auto child = ::fork();
  if (!child) {
    *static_cast<volatile int *>(&record->value) = 59;
    ::_exit(0);
  }
  int status;
  if (child < 0 || ::waitpid(child, &status, 0) != child || !WIFEXITED(status) ||
      WEXITSTATUS(status) || record->value != 59)
    return 2;
  overlap_test::overlap_write(*record, 1001);
  std::printf("DONE value=%d\n", record->value);
  while (::access("stop.now", F_OK))
    ::usleep(1000);
  const auto result = record->value != 1001;
  record->~Record();
  ::munmap(storage, sizeof(overlap_test::Record));
  return result;
}

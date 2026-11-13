#include "rendezvous.hpp"

#include <atomic>
#include <cstdio>
#include <fcntl.h>
#include <new>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/prctl.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#ifndef PR_SET_PTRACER_ANY
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif

namespace shared_test {
struct Counter {
#if defined(TRACE_TEST_SHARED_SCALAR)
  int value;
#else
  std::atomic<int> value;
#endif
  Counter();
  ~Counter();
};

__attribute__((noinline, noipa)) Counter::Counter() : value(0) {}
__attribute__((noinline, noipa)) Counter::~Counter() { asm volatile("" ::: "memory"); }

extern "C" __attribute__((noinline, noipa)) int increment_shared(Counter &counter) {
#if defined(TRACE_TEST_SHARED_SCALAR)
  const auto old = counter.value;
  counter.value = old + 1;
  return old;
#else
  return counter.value.fetch_add(1, std::memory_order_acq_rel);
#endif
}
}

int main(int argc, char **argv) {
  if (argc != 2 || ::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) != 0)
    return 1;
  const auto page = ::sysconf(_SC_PAGESIZE);
  const int file = ::open(argv[1], O_RDWR);
  if (file < 0 || page <= 0)
    return 2;
  void *shared = ::mmap(nullptr, page, PROT_READ | PROT_WRITE, MAP_SHARED, file, page);
  void *private_copy = ::mmap(nullptr, page, PROT_READ | PROT_WRITE, MAP_PRIVATE, file, page);
  if (shared == MAP_FAILED || private_copy == MAP_FAILED)
    return 3;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
#if defined(__aarch64__)
  arm_rendezvous();
#else
  while (::access("write.now", F_OK))
    ::usleep(1000);
#endif
  auto *private_counter = new (private_copy) shared_test::Counter;
  auto *counter = new (static_cast<char *>(shared) + 128) shared_test::Counter;
  std::printf("SETUP address=%p offset=%ld\n", static_cast<void *>(counter), page + 128);
  while (::access("../run.now", F_OK))
    ::usleep(1000);
  if (shared_test::increment_shared(*private_counter) != 0)
    return 4;
  // The same address and inode no longer refer to shared storage. A cached
  // positive lookup would incorrectly record this write before the 64 below.
  if (::mmap(shared, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, file, page) != shared)
    return 7;
  if (shared_test::increment_shared(*counter) < 0)
    return 8;
  if (::mmap(shared, page, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, file, page) != shared)
    return 9;
  unsigned checksum = 0;
  for (unsigned i = 0; i != 64; ++i) {
#if defined(TRACE_TEST_SHARED_SCALAR)
    if (::flock(file, LOCK_EX))
      return 5;
#endif
    checksum += shared_test::increment_shared(*counter);
#if defined(TRACE_TEST_SHARED_SCALAR)
    if (::flock(file, LOCK_UN))
      return 6;
#endif
  }
  std::printf("DONE sum=%u\n", checksum);
  while (::access("../stop.now", F_OK))
    ::usleep(1000);
  private_counter->~Counter();
  ::munmap(private_copy, page);
  ::munmap(shared, page);
  ::close(file);
}

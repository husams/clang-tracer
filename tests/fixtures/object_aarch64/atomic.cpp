#include "rendezvous.hpp"

#include <atomic>
#include <cstdio>
#include <thread>

namespace arm_test {
struct AtomicRecord {
  std::atomic<int> value;
  AtomicRecord();
  ~AtomicRecord();
};

__attribute__((noinline, noipa)) AtomicRecord::AtomicRecord() : value(17) {}
__attribute__((noinline, noipa)) AtomicRecord::~AtomicRecord() { asm volatile("" ::: "memory"); }

extern "C" __attribute__((noinline, noipa)) int arm_atomic_write(AtomicRecord &record, int kind) {
  if (kind == 0) {
    record.value.store(30, std::memory_order_release);
    return 0;
  }
  if (kind == 1)
    return record.value.fetch_add(3, std::memory_order_acq_rel);
  int expected = 36;
  return record.value.compare_exchange_strong(expected, 70, std::memory_order_acq_rel);
}
}

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
  arm_rendezvous();
  arm_test::AtomicRecord record;
  arm_test::arm_atomic_write(record, 0);
  int first = 0, second = 0;
  std::atomic<bool> go{false};
  std::thread a([&] {
    while (!go.load(std::memory_order_acquire)) {}
    first = arm_test::arm_atomic_write(record, 1);
  });
  std::thread b([&] {
    while (!go.load(std::memory_order_acquire)) {}
    second = arm_test::arm_atomic_write(record, 1);
  });
  go.store(true, std::memory_order_release);
  a.join();
  b.join();
  const auto won = arm_test::arm_atomic_write(record, 2);
  const auto lost = arm_test::arm_atomic_write(record, 2);
  const auto final = record.value.load(std::memory_order_relaxed);
  std::printf("DONE olds=%d,%d cas=%d,%d value=%d\n", first, second, won, lost, final);
  while (::access("stop.now", F_OK))
    ::usleep(1000);
  return first + second != 63 || won != 1 || lost != 0 || final != 70;
}

#include "person.hpp"
#include "batch.hpp"

#include <atomic>
#include <cstddef>

extern "C" __attribute__((noinline))
void person_write_many(demo::Person *people, std::size_t count,
                       std::atomic<std::size_t> *completed) {
  for (std::size_t i = 0; i < count; ++i) {
    people[i].age = 1; // STORE_LINE
    completed->store(i + 1, std::memory_order_relaxed);
  }
}

void demo::WriteBatch::run() {
  // Preserve a normal entry and call boundary even in the optimized fixture.
  volatile std::size_t iterations = count;
  person_write_many(people, iterations, static_cast<std::atomic<std::size_t> *>(completed));
  std::atomic_signal_fence(std::memory_order_acq_rel);
}

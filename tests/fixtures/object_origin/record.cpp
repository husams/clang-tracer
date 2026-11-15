#include "record.hpp"

namespace origin_test {
Tracked::Tracked(int initial) : value(initial) {}
Tracked::~Tracked() { asm volatile("" ::: "memory"); }

[[gnu::always_inline]] inline void inner(Aggregate &record) {
  record.value = 91; // INLINE_STORE
}

[[gnu::noinline]] void write_inline(Aggregate &record) {
  inner(record);
}
} // namespace origin_test

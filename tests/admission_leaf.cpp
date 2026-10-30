#include <cstdint>

// Compiled separately with frame pointers omitted, including when callers use
// frame pointers. This checks ABI-preserved register recovery across that
// boundary.
extern "C" __attribute__((noinline)) void
admission_busy_leaf(std::uint64_t *calls) {
  std::uint64_t iteration = 1;
  for (;;) {
    volatile std::uint64_t arithmetic =
        iteration * 0x9e3779b97f4a7c15ULL + 0x632be59bd9b4e019ULL;
    __atomic_add_fetch(calls, 1ULL, __ATOMIC_RELAXED);
    iteration += arithmetic & 1U;
    ++iteration;
  }
}

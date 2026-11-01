#include "shared_stack.hpp"

#include <cerrno>
#include <cstdint>
#include <link.h>

namespace {

__attribute__((noinline)) void shared_stack_busy_leaf(SharedStackState *state) {
  std::uint64_t iteration = 1;
  for (;;) {
    volatile std::uint64_t integer_value =
        iteration * 0x9e3779b97f4a7c15ULL + 0x632be59bd9b4e019ULL;
    volatile std::uint64_t integer_check =
        (integer_value - 0x632be59bd9b4e019ULL) -
        iteration * 0x9e3779b97f4a7c15ULL;
    volatile double floating_value =
        static_cast<double>(iteration & 0xffffU) * 0.125 + 0.375;
    volatile double floating_check = floating_value * 8.0;
    if (integer_check != 0 ||
        floating_check != static_cast<double>(iteration & 0xffffU) + 3.0 ||
        *state->errno_address != EDOM) {
      __atomic_add_fetch(state->failures, 1ULL, __ATOMIC_RELAXED);
    }
    __atomic_add_fetch(state->calls, 1ULL, __ATOMIC_RELAXED);
    ++iteration;
  }
}

} // namespace

__attribute__((noinline)) void
SharedStackB::run(SharedStackState *state) const {
  shared_stack_busy_leaf(state);
  volatile int keep_frame = 1;
  (void)keep_frame;
}

extern "C" __attribute__((visibility("default"), noinline)) int
shared_stack_runtime_callback(dl_phdr_info *, std::size_t, void *context) {
  auto *state = static_cast<SharedStackState *>(context);
  std::uint64_t iteration = 1;
  for (;;) {
    volatile std::uint64_t arithmetic =
        iteration * 0x9e3779b97f4a7c15ULL + 0x632be59bd9b4e019ULL;
    __atomic_add_fetch(state->calls, 1ULL, __ATOMIC_RELAXED);
    iteration += arithmetic & 1U;
    ++iteration;
  }
}

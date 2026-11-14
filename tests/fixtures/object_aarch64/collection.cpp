#include "rendezvous.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace arm_test {
struct Leg { std::uint32_t id; int quantity; };
struct Portfolio {
  std::vector<Leg> legs;
  void rebalance();
};

extern "C" __attribute__((noinline, noipa)) void arm_reserve(std::vector<Leg> &legs, std::size_t capacity) {
  legs.reserve(capacity);
}

#if defined(TRACE_TEST_MULTIPLE_MUTATIONS)
std::uintptr_t middle_address;
#endif
#if defined(TRACE_TEST_DISTINCT_MUTATIONS)
extern "C" __attribute__((noinline, noipa)) void arm_resize(std::vector<Leg> &legs, std::size_t size) {
  legs.resize(size);
}
#endif

#if defined(TRACE_TEST_INLINE_MUTATION)
#if defined(TRACE_TEST_DISJOINT_MUTATION)
volatile bool alternate_reserve = false;

extern "C" __attribute__((noinline, noipa))
void arm_reserve_alternate(std::vector<Leg> &legs) {
  legs.reserve(3);
}
#endif

#if defined(TRACE_TEST_MULTIPLE_INLINE_EXITS)
extern "C" [[gnu::always_inline]] inline bool inline_reserve(std::vector<Leg> &legs) {
  asm volatile("nop\n nop\n nop\n nop\n nop" : : "r"(&legs) : "memory");
  if (alternate_reserve) {
    arm_reserve_alternate(legs);
    asm volatile("nop\n nop\n nop\n nop\n nop" : : "r"(&legs) : "memory");
    return true;
  }
  arm_reserve(legs, 2);
  asm volatile("nop\n nop\n nop\n nop\n nop" : : "r"(&legs) : "memory");
  return false;
}
#else
extern "C" [[gnu::always_inline]] inline void inline_reserve(std::vector<Leg> &legs) {
  asm volatile("nop\n nop\n nop\n nop\n nop" : : "r"(&legs) : "memory");
#if defined(TRACE_TEST_DISJOINT_MUTATION)
  if (alternate_reserve) {
    arm_reserve_alternate(legs);
    asm volatile("nop\n nop\n nop\n nop\n nop" : : "r"(&legs) : "memory");
  } else
#endif
  arm_reserve(legs, 2);
  asm volatile("nop\n nop\n nop\n nop\n nop" : : "r"(&legs) : "memory");
}
#endif
#endif

__attribute__((noinline, noipa)) void Portfolio::rebalance() {
#if defined(TRACE_TEST_INLINE_MUTATION)
#if defined(__aarch64__)
  register auto *owner asm("x19") = this;
#else
  register auto *owner asm("rbx") = this;
#endif
  asm volatile("nop\n nop\n nop\n nop\n nop" : "+r"(owner) : : "memory");
#if defined(TRACE_TEST_MULTIPLE_INLINE_EXITS)
  if (inline_reserve(owner->legs))
    asm volatile("nop\n nop\n nop\n nop\n nop\n nop" : : "r"(owner) : "memory");
  else
    asm volatile("nop\n nop\n nop\n nop\n nop" : : "r"(owner) : "memory");
#else
  inline_reserve(owner->legs);
  asm volatile("nop\n nop\n nop\n nop\n nop" : : "r"(owner) : "memory");
#endif
  owner->legs.front().quantity = 75;
#else
  arm_reserve(legs, 2);
  legs.front().quantity = 75;
#endif
#if defined(TRACE_TEST_INLINE_MUTATION)
  asm volatile("nop\n nop\n nop\n nop\n nop" ::: "memory");
#endif
#if defined(TRACE_TEST_MULTIPLE_MUTATIONS)
  middle_address = reinterpret_cast<std::uintptr_t>(&legs.front().quantity);
#if defined(TRACE_TEST_DISTINCT_MUTATIONS)
  arm_resize(legs, 4);
#else
  arm_reserve(legs, 4);
#endif
  legs.front().quantity = 80;
#endif
}
}

#if defined(TRACE_TEST_DISJOINT_MUTATION)
int main(int argc, char **) {
  arm_test::alternate_reserve = argc > 1;
#else
int main() {
#endif
  arm_test::Portfolio portfolio{{{12, 100}}};
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
#if defined(__aarch64__)
  arm_rendezvous();
#else
  while (::access("write.now", F_OK))
    ::usleep(1000);
#endif
  const auto previous = reinterpret_cast<std::uintptr_t>(&portfolio.legs.front().quantity);
  portfolio.rebalance();
  const auto current = &portfolio.legs.front().quantity;
#if defined(TRACE_TEST_MULTIPLE_MUTATIONS)
  std::printf("DONE previous=%#llx middle=%#llx current=%p value=%d\n",
              static_cast<unsigned long long>(previous),
              static_cast<unsigned long long>(arm_test::middle_address),
              static_cast<void *>(current), *current);
  constexpr int expected = 80;
#else
  std::printf("DONE previous=%#llx current=%p value=%d\n",
              static_cast<unsigned long long>(previous), static_cast<void *>(current), *current);
  constexpr int expected = 75;
#endif
  while (::access("stop.now", F_OK))
    ::usleep(1000);
  return *current != expected || previous == reinterpret_cast<std::uintptr_t>(current);
}

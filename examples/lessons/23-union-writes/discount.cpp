#include "discount.hpp"

namespace shop {
// Writes the member that is already in use.
__attribute__((noinline)) void Discount::raise() {
  state.value.amount = state.value.amount * 2 + 50;
}

// Changes which member is in use, then writes the new member.
__attribute__((noinline)) void Discount::to_rate() {
  state.kind = Kind::rate;
  state.value.rate = 0.25;
}

__attribute__((noinline)) void Discount::reset(long long amount) {
  state.kind = Kind::amount;
  state.value.amount = amount;
}
} // namespace shop

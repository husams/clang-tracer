#include "cart.hpp"

namespace shop {
__attribute__((noinline)) void grow(std::vector<Line> &lines) {
  lines.reserve(lines.capacity() * 2);
}

// Writes the second line twice, with a reallocation in between.
__attribute__((noinline)) void Cart::restock() {
  lines.at(1).quantity += 5;
  grow(lines);
  lines.at(1).quantity += 5;
}
} // namespace shop

#include "cart.hpp"

#include <algorithm>

namespace shop {
__attribute__((noinline)) void move_to_front(Lines &lines) {
  std::rotate(lines.begin(), lines.end() - 1, lines.end());
}

__attribute__((noinline)) void drop_front(Lines &lines) {
  lines.erase(lines.begin());
}

__attribute__((noinline)) void add_line(Lines &lines) {
  lines.push_back({12, 1});
}

// Moves line 12, removes it, adds it again, then writes its quantity.
__attribute__((noinline)) void Cart::rebuild() {
  move_to_front(lines);
  drop_front(lines);
  add_line(lines);
  lines.back().quantity = 5;
}
} // namespace shop

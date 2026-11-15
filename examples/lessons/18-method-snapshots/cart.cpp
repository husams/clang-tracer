#include "cart.hpp"

namespace shop {
// One call changes three members: a quantity, a discount and the total.
void Cart::apply_offer() {
  for (auto &line : lines)
    if (line.id == 12)
      line.quantity += 1;
  discounts["bundle"] += 5;
  total = 0;
  for (const auto &line : lines)
    total += line.quantity * 10;
  total -= discounts["bundle"];
}
} // namespace shop

#pragma once

namespace demo {
struct Order {
  int quantity;
  explicit Order(int initial);
  ~Order() noexcept;
  void set_quantity(int next);
};

void apply_fill(Order &order, int amount);
} // namespace demo

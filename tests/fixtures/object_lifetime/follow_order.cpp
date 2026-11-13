#include "follow_order.hpp"

namespace demo {
Order::Order(int initial) : quantity(initial) {}
Order::~Order() noexcept = default;
void Order::set_quantity(int next) { quantity = next; }
void apply_fill(Order &order, int amount) { order.quantity -= amount; }
} // namespace demo

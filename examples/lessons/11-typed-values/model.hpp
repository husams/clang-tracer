#pragma once

namespace app {

// A public record with a default constructor, so the REPL can map it
// automatically with `%cpp generate` or `%cpp import`.
struct Order {
  int id = 0;
  int quantity = 0;
  double unit_price = 0.0;
};

double submit(const Order& order);

}  // namespace app

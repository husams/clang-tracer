#pragma once

namespace shop {
enum class Kind : unsigned { amount = 1, rate = 2 };

// A discount is either a fixed amount in cents or a rate. `kind` says which
// member of `value` is in use.
struct Discount {
  struct State {
    Kind kind;
    union Value {
      long long amount;
      double rate;
    } value;
  } state;

  void raise();   // doubles the amount and adds 50 cents
  void to_rate(); // switches the discount to a rate of 0.25
  void reset(long long amount);
};
} // namespace shop

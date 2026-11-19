#pragma once

namespace shop {
class Account {
public:
  explicit Account(int opening);
  ~Account();

  // Adds `amount` to the balance and returns the new balance.
  int deposit(int amount);

  int balance = 0;
  int deposits = 0;
};
} // namespace shop

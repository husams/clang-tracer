#include "account.hpp"

namespace shop {
Account::Account(int opening) : balance(opening) {}

Account::~Account() { balance = -1; }

int Account::deposit(int amount) {
  balance += amount;
  ++deposits;
  return balance;
}
} // namespace shop

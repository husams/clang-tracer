#include "account.hpp"

namespace bank {
Account::Account(int id, int opening) : id(id), balance(opening) {}
Account::~Account() = default;

void deposit(Account &account, int amount) {
  account.balance += amount;
}
} // namespace bank

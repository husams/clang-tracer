#pragma once

namespace bank {
struct Account {
  int id;
  int balance;
  Account(int id, int opening);
  ~Account();
};

void deposit(Account &account, int amount);
} // namespace bank

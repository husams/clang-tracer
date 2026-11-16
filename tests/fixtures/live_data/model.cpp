#include "model.hpp"

#include <array>
#include <cstdlib>
#include <stdexcept>
#include <thread>

namespace example {
Service::~Service() = default;

int Service::kind() const {
  return 1;
}

View::~View() = default;

int View::view() const {
  return 2;
}

Cache::~Cache() = default;
Account::~Account() = default;

void Cache::update_if_present(int product, int cents) {
  if (product != 42) {
    throw std::runtime_error("unexpected product");
  }
  std::lock_guard lock(mutex_);
  price_cents_ = cents;
  ++calls_;
}

int Cache::price() const {
  std::lock_guard lock(mutex_);
  return price_cents_;
}

int Cache::calls() const {
  std::lock_guard lock(mutex_);
  return calls_;
}

void Account::set_daily_limit_cents(int cents) {
  std::lock_guard lock(mutex_);
  limit_cents_ = cents;
  ++calls_;
}

int Account::limit() const {
  std::lock_guard lock(mutex_);
  return limit_cents_;
}

int Account::calls() const {
  std::lock_guard lock(mutex_);
  return calls_;
}

std::shared_ptr<Service> g_primary_cache;
std::shared_ptr<Account> g_active_account;
std::mutex owner_mutex;
std::shared_ptr<Service> secondary_cache;
std::array<std::shared_ptr<Account>, 2> accounts;
std::weak_ptr<Account> old_owner;
std::mutex borrow_mutex;
Account borrowed{7};

std::vector<std::shared_ptr<Service>> snapshot_caches() {
  std::lock_guard lock(owner_mutex);
  return {g_primary_cache, secondary_cache, g_primary_cache};
}

std::vector<std::shared_ptr<Account>> snapshot_accounts() {
  std::lock_guard lock(owner_mutex);
  return {g_active_account, accounts[0], accounts[1]};
}

std::weak_ptr<Account> expired_account() {
  return old_owner;
}

void release_primary_cache(const std::shared_ptr<Cache>& owner) {
  std::thread releaser([&] {
    std::lock_guard lock(owner_mutex);
    if (g_primary_cache.owner_before(owner) || owner.owner_before(g_primary_cache)) {
      std::abort();
    }
    g_primary_cache.reset();
  });
  releaser.join();
}

void checked_borrow_account(void (*callback)(Account&, const std::unique_lock<std::mutex>&, void*),
                            void* context) {
  const std::unique_lock lock(borrow_mutex);
  callback(borrowed, lock, context);
}
} // namespace example

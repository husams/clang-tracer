#pragma once

#include <array>
#include <memory>
#include <mutex>
#include <vector>

namespace example {
struct Service {
  virtual ~Service();
  virtual int kind() const;
};

struct View {
  virtual ~View();
  virtual int view() const;
};

struct Cache final : Service, View {
  ~Cache() override;
  void update_if_present(int product, int cents);
  int price() const;
  int calls() const;

private:
  mutable std::mutex mutex_;
  int price_cents_ = 100;
  int calls_ = 0;
};

struct Account final : Service {
  explicit Account(int id)
      : id(id) {}

  ~Account() override;
  void set_daily_limit_cents(int cents);
  int limit() const;
  int calls() const;

  const int id;

private:
  mutable std::mutex mutex_;
  int limit_cents_ = 200;
  int calls_ = 0;
};

struct Plain {
  int value = 0;
};

std::vector<std::shared_ptr<Service>> snapshot_caches();
std::vector<std::shared_ptr<Account>> snapshot_accounts();
std::weak_ptr<Account> expired_account();
void release_primary_cache(const std::shared_ptr<Cache>&);
void checked_borrow_account(void (*)(Account&, const std::unique_lock<std::mutex>&, void*), void*);
extern std::shared_ptr<Service> g_primary_cache;
extern std::shared_ptr<Account> g_active_account;
extern std::mutex owner_mutex;
extern std::shared_ptr<Service> secondary_cache;
extern std::array<std::shared_ptr<Account>, 2> accounts;
extern std::weak_ptr<Account> old_owner;
} // namespace example

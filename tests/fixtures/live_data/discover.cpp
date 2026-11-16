#include <clang_tracer/live.hpp>

#include "model.hpp"

void discover(clang_tracer::ObjectSink& objects) {
  for (const auto& service : example::snapshot_caches()) {
    if (auto cache = std::dynamic_pointer_cast<example::Cache>(service)) {
      objects.add(cache, "CacheManager::snapshot_caches");
    }
  }
  for (const auto& account : example::snapshot_accounts()) {
    objects.add(account, "AccountStore::snapshot_accounts");
  }
  objects.add(example::expired_account(), "expired_account");
  objects.skip<example::Account>("unverified receiver");
}

void scan_owners(clang_tracer::ObjectSink& objects) {
  std::shared_ptr<example::Service> cache;
  std::shared_ptr<example::Account> account;
  {
    const std::lock_guard lock(example::owner_mutex);
    cache = example::g_primary_cache;
    account = example::g_active_account;
  }
  if (auto concrete = std::dynamic_pointer_cast<example::Cache>(cache)) {
    objects.add(concrete, "g_primary_cache");
  }
  objects.add(account, "g_active_account");
}

void borrow_account(clang_tracer::ObjectSink& objects) {
  example::checked_borrow_account(
      [](example::Account& account, const std::unique_lock<std::mutex>& lock, void* data) {
        static_cast<clang_tracer::ObjectSink*>(data)->borrow(account, lock, "borrow_mutex");
      },
      &objects);
}

void unlocked_account(clang_tracer::ObjectSink& objects) {
  auto account = example::snapshot_accounts().front();
  const std::unique_lock lock(example::owner_mutex, std::defer_lock);
  objects.borrow(*account, lock, "unlocked");
}

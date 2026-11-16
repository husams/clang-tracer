#include <stdexcept>

#include "model.hpp"

void update(std::shared_ptr<example::Cache> cache, std::shared_ptr<example::Account> account) {
  if (static_cast<bool>(cache) == static_cast<bool>(account)) {
    throw std::runtime_error("expected one shared argument");
  }
  if (cache) {
    cache->update_if_present(42, 999);
  } else {
    account->set_daily_limit_cents(5000);
  }
}

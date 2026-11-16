#include <stdexcept>

#include "model.hpp"

void update(example::Cache* cache, example::Account* account) {
  if (static_cast<bool>(cache) == static_cast<bool>(account)) {
    throw std::runtime_error("expected one raw argument");
  }
  if (cache) {
    cache->update_if_present(42, 999);
  } else {
    account->set_daily_limit_cents(5000);
  }
}

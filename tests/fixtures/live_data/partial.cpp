#include <stdexcept>

#include "model.hpp"

void update(example::Cache& cache) {
  cache.update_if_present(42, 999);
}

void update(example::Account& account) {
  if (account.id == 2) {
    throw std::runtime_error("fixture callback failure");
  }
  account.set_daily_limit_cents(5000);
}

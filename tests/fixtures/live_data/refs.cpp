#include "model.hpp"

void update(example::Cache& cache) {
  cache.update_if_present(42, 999);
}

void update(example::Account& account) {
  account.set_daily_limit_cents(5000);
}

#include "model.hpp"

void update(std::shared_ptr<example::Cache> cache) {
  example::release_primary_cache(cache);
  cache->update_if_present(42, 999);
}

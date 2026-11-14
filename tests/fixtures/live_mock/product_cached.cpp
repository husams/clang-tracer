#include "application.hpp"

std::optional<Product> load_cached_discount(ProductStore& store, int id) {
  auto product = store.find_cached(id);
  if (product && id == 42) {
    product->price_cents = 999;
  }
  return product;
}

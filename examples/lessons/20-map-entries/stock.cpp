#include "stock.hpp"

namespace shop {
__attribute__((noinline)) void rehash(Items &items) {
  items.rehash(items.bucket_count() * 3);
}

// Writes the price of item 12 twice, with a rehash in between.
__attribute__((noinline)) void Stock::reprice() {
  items.at(12).pricing.unit += 0.5;
  rehash(items);
  items.at(12).pricing.unit += 0.25;
}
} // namespace shop

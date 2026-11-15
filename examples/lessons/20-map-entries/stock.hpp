#pragma once

#include <unordered_map>

namespace shop {
struct Item {
  struct Pricing {
    double unit;
  } pricing;
};

using Items = std::unordered_map<int, Item>;

struct Stock {
  Items items;

  void reprice();
};

// Triples the bucket count. The items stay where they are, but the table
// that leads to them is rebuilt.
void rehash(Items &items);
} // namespace shop

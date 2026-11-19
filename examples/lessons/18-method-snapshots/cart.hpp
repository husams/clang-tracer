#pragma once

#include <map>
#include <string>
#include <vector>

namespace shop {
struct Line {
  unsigned id;
  int quantity;
};

struct Cart {
  std::vector<Line> lines;
  std::map<std::string, int> discounts;
  int total = 0;

  void apply_offer();
};
} // namespace shop

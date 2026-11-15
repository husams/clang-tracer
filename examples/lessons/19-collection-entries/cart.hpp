#pragma once

#include <vector>

namespace shop {
struct Line {
  unsigned id;
  int quantity;
};

struct Cart {
  std::vector<Line> lines;

  void restock();
};

// Doubles the capacity, which moves every line to new storage.
void grow(std::vector<Line> &lines);
} // namespace shop

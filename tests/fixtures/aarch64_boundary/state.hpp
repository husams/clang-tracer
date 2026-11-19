#pragma once
#include <vector>

namespace boundary_test {
struct Record {
  int value;
  std::vector<int> values;
  void first();
  void second();
};
} // namespace boundary_test

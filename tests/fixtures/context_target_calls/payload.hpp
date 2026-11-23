#pragma once
#include <string>
#include <vector>

#include "items.hpp"

namespace target_calls_probe {
struct ItemsScope {};
struct Payload {
  std::vector<std::string> items;
  std::vector<std::string> labels;
  std::vector<int> numbers;
};
// populate() names Item only through a pointer. Naming the implementation by
// value makes the prepared adapter declare both classes with their virtual
// methods.
struct Admit {
  target_calls::ItemImpl item;
};
} // namespace target_calls_probe

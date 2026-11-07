#pragma once

#include "account.hpp"

namespace prepared_demo {
struct ReadingScope {};
struct Reading {
  std::string label;
  std::vector<int> parts;
  Adjustment adjustment;
};
} // namespace prepared_demo

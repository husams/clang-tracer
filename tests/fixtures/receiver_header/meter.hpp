#pragma once

#include <cstdint>

namespace receiver_header {
class Meter {
public:
  std::int64_t total_cents = 0;
  std::uint32_t charges = 0;
  double rate = 0.25;
  bool frozen = false;
  std::int64_t charge(std::int64_t cents);
  std::int64_t total() const { return total_cents; }

private:
  int hidden = 99;
};
} // namespace receiver_header

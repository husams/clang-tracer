#pragma once

#include <cstdint>

namespace receiver_header {
class Meter {
public:
  std::int64_t total_cents = 0;
  std::uint32_t charges = 0;
  double rate = 0.25;
  bool frozen = false;
  static std::int64_t static_total;
  std::int64_t charge(std::int64_t cents);
  std::int64_t charge() const { return total_cents; }
  std::int64_t total() const { return total_cents; }
  std::int64_t bump(std::int64_t cents) const;

private:
  int hidden = 99;
};

// Receiver implementation code is not callback code, even outside the class.
inline std::int64_t Meter::bump(std::int64_t cents) const {
  return const_cast<Meter *>(this)->charge(cents) + 1;
}
} // namespace receiver_header

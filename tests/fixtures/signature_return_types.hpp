#pragma once

#include <cstdint>

namespace signature_fixture {

struct SmallReturn {
  std::int32_t first;
  std::int32_t second;
};

struct MixedReturn {
  double ratio;
  std::int32_t count;
};

struct LargeReturn {
  std::uint64_t first;
  std::uint64_t second;
  std::uint64_t third;
};

struct NontrivialReturn {
  explicit NontrivialReturn(int input);
  NontrivialReturn(const NontrivialReturn &other);
  NontrivialReturn(NontrivialReturn &&other) noexcept;
  ~NontrivialReturn();

  int value;
  static int copies() noexcept;
  static int moves() noexcept;
  static int destructions() noexcept;
};

SmallReturn small_return(std::int32_t input);
MixedReturn mixed_return(std::int32_t input);
LargeReturn large_return(std::uint64_t input);
NontrivialReturn nontrivial_return(int input);
int nontrivial_copy_count();
int nontrivial_move_count();
int nontrivial_destruction_count();

} // namespace signature_fixture

#pragma once

#include "state.hpp"

namespace fixture {
struct CacheV2 {
  std::size_t max_entries = 1024;
  std::string name;
  int hook_calls = 0;
  std::vector<int> entries;
};

inline CacheV2 make_v2(const Seed& seed) {
  CacheV2 state;
  state.name = seed.name;
  state.entries.assign(seed.count, 42);
  return state;
}
} // namespace fixture

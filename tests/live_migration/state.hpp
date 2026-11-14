#pragma once

#include <string>
#include <vector>

#include "tracer/live_migration.hpp"

namespace fixture {
namespace managed = tracer::live::managed;

struct Seed {
  std::string name;
  std::size_t count;
};

struct CacheV1 {
  std::string name;
  std::vector<int> entries;
};

struct CacheV2 {
  std::size_t max_entries = 1024;
  std::string name;
  int hook_calls = 0;
  std::vector<int> entries;
};

struct TaggedV1 {
  enum class Kind { count, fraction } kind;

  union {
    long count;
    double fraction;
  };
};

struct TaggedV2 {
  unsigned revision = 2;
  TaggedV1::Kind kind;

  union {
    double fraction;
    long count;
  };
};

inline CacheV1 make_v1(const Seed& seed) {
  return {seed.name, std::vector<int>(seed.count, 42)};
}

inline CacheV2 make_v2(const Seed& seed) {
  CacheV2 state;
  state.name = seed.name;
  state.entries.assign(seed.count, 42);
  return state;
}
} // namespace fixture

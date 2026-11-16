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

} // namespace fixture

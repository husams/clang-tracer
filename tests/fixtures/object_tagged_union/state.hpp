#pragma once

#include <cstdint>

namespace tagged_test {
enum class Kind : std::uint32_t { integer = 1, real = 2, unregistered = 3 };

union Payload {
  std::int32_t integer;
  float real;
  std::uint32_t raw;
};

struct State {
  Kind kind;
  Payload payload;
  explicit State(Kind kind);
  ~State();
};

void write_bits(State &state, std::uint32_t bits);
}

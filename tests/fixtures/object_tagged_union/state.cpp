#include "state.hpp"

#include <cstring>

namespace tagged_test {
State::State(Kind tag) : kind(tag), payload{0} {
  switch (kind) {
  case Kind::integer:
    payload.integer = -2;
    break;
  case Kind::real:
    payload.real = 1.5F;
    break;
  case Kind::unregistered:
    payload.raw = 0x12345678;
    break;
  }
}

State::~State() { asm volatile("" ::: "memory"); }

void write_bits(State &state, std::uint32_t bits) {
  std::memcpy(&state.payload, &bits, sizeof(bits));
}
}

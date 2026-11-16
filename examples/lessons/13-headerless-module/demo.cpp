#include "demo.hpp"

namespace demo {
__attribute__((noinline)) int score(const Packet& packet) {
  return packet.value * 10 + packet.flags;
}
} // namespace demo

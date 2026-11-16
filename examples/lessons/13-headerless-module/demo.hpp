#pragma once

namespace demo {
struct Packet {
  int value;
  int flags;
};
int score(const Packet& packet);
} // namespace demo

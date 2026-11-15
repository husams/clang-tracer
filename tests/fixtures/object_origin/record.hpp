#pragma once

namespace origin_test {
struct Aggregate {
  int value;
};
struct Tracked {
  explicit Tracked(int value);
  ~Tracked();
  int value;
};
void write_inline(Aggregate &);
} // namespace origin_test
extern "C" void opaque_write(origin_test::Tracked &);

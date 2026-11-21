#include "repl_virtual.hpp"

#define NOINLINE __attribute__((noinline))

namespace virt {
NOINLINE Shape::Shape() = default;
NOINLINE Shape::~Shape() = default;
NOINLINE int Shape::area() const { return 1; }
NOINLINE int Shape::plain() const { return 7; }
NOINLINE Square::Square(int side) : side_(side) {}
NOINLINE Square::~Square() = default;
NOINLINE int Square::sides() const { return 4; }
NOINLINE int Square::area() const { return side_ * side_; }
NOINLINE Plain::Plain() = default;
NOINLINE Plain::~Plain() = default;
NOINLINE int Plain::sides() const { return 3; }
NOINLINE Tagged::Tagged() = default;
NOINLINE Tagged::~Tagged() = default;
NOINLINE int Tagged::tag() const { return 100; }
NOINLINE Mixed::Mixed() = default;
NOINLINE Mixed::~Mixed() = default;
NOINLINE int Mixed::sides() const { return 5; }
NOINLINE int Mixed::tag() const { return 200; }
NOINLINE Counter::Counter() = default;
NOINLINE int Counter::add(int amount) {
  value_ += amount;
  return value_;
}

// Per pass: Square::sides 4, Square::area 9, Plain::sides 3, Shape::area 1, Shape::plain 7,
// Mixed::sides 5, Mixed::tag 200 (through Tagged), Counter::add(1) then add(2).
NOINLINE int run_once(int pass) {
  Square square(3);
  Plain plain;
  Mixed mixed;
  Counter counter;
  Shape &a = square;
  Shape &b = plain;
  Shape &c = mixed;
  Tagged &tagged = mixed;
  int sum = a.sides() + a.area() + b.sides() + b.area() + square.plain() + c.sides() + tagged.tag();
  sum += counter.add(1) + counter.add(2);
  return sum + pass * 0;
}
} // namespace virt

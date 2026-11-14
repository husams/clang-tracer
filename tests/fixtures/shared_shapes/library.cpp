#include "shapes.hpp"

#include <cstdlib>
#include <new>

namespace {
alignas(demo::Circle) unsigned char circle_slot[sizeof(demo::Circle)];
bool occupied = false;
}

namespace demo {
__attribute__((noinline, patchable_function_entry(5)))
Circle::Circle(int initial) : Shape("icon"), radius(initial) {}

__attribute__((noinline, patchable_function_entry(5)))
Circle::~Circle() = default;

void *Circle::operator new(std::size_t size) {
  if (occupied || size != sizeof(Circle))
    throw std::bad_alloc();
  occupied = true;
  return circle_slot;
}

void Circle::operator delete(void *pointer) noexcept {
  if (pointer != circle_slot) {
    ::operator delete(pointer);
    return;
  }
  if (!occupied)
    std::abort();
  occupied = false;
}

__attribute__((noinline)) int Circle::resize(int value) {
  const int limited = value > 100 ? 100 : value;
  radius = limited; // RADIUS_WRITE
  return limited;
}

int Circle::size() const { return radius; }

class Square final : public Shape {
  int side = 20;
public:
  Square() : Shape("square") {}
  int resize(int value) override { return side = value; }
  int size() const override { return side; }
};
} // namespace demo

extern "C" demo::Shape *make_circle(int value) { return new demo::Circle(value); }
extern "C" demo::Shape *make_control_circle() { return ::new demo::Circle(10); }
extern "C" bool circle_slot_available() { return !occupied; }
extern "C" demo::Shape *make_square() { return new demo::Square; }
extern "C" const int *radius_address(demo::Shape *shape) {
  return static_cast<demo::Circle *>(shape)->field();
}
extern "C" int direct_circle_resize(demo::Shape *shape, int value) {
  return static_cast<demo::Circle *>(shape)->demo::Circle::resize(value);
}

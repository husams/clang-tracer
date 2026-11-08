#include "types.hpp"

namespace prepared_concrete {

Prefix::operator bool() const { return prefix != 0; }

int Prefix::unused() const { return prefix; }

int Handler::route(int value) const { return value + factor + base + shared; }

int Handler::scale(int value) {
  // Keep enough straight-line instructions in the entry window for patching.
  const volatile int result = value * 3;
  return result;
}

int Handler::Nested::adjust(int value) { return value + 9; }

int through_base(const Interface *object, int value) {
  return object->route(value);
}

template<> double Handler::convert<double>(double value) const {
  return value + factor + 0.5;
}

template<> double twice<double>(double value) {
  const volatile double original = value;
  return original + 0.5;
}

} // namespace prepared_concrete

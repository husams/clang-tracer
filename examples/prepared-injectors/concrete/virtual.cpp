#include "types.hpp"

namespace prepared_concrete {

Prefix::operator bool() const { return prefix != 0; }

int Prefix::unused() const { return prefix; }

int Handler::route(int value) const { return value + factor + base + shared; }

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

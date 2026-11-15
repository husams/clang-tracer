#include "counter.hpp"

namespace lesson {

Counter::Counter(int initial) : value(initial) {}

Counter::~Counter() { value = -1; }

int Counter::add(int amount) { return value += amount; }

}  // namespace lesson

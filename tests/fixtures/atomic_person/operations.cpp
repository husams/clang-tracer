#include "person.hpp"

namespace demo {
Person::Person(int initial) : age(initial) {}
Person::~Person() = default;

void store_age(Person &person, int value) {
  person.age.store(value, std::memory_order_relaxed);
}

int add_age(Person &person) {
  return person.age.fetch_add(1, std::memory_order_relaxed);
}

bool change_age(Person &person, int desired) {
  int expected = 42;
  return person.age.compare_exchange_strong(expected, desired, std::memory_order_relaxed);
}
}

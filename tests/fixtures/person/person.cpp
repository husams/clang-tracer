#include "person.hpp"

namespace demo {
Person::Person(int initial) : age(initial) {}
Person::~Person() = default;

void update(Person &person) {
  person.age = 1; // STORE_LINE
}
}

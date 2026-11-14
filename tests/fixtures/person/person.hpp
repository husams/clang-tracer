#pragma once

namespace demo {
struct Person {
  int age;
  explicit Person(int initial);
  ~Person();
};

void update(Person &person);
void remainder(Person &person, int divisor);
}

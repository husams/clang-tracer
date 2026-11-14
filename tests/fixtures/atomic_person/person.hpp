#pragma once

#include <atomic>

namespace demo {
struct Person {
  std::atomic<int> age;
  explicit Person(int initial);
  ~Person();
};

void store_age(Person &person, int value);
int add_age(Person &person);
bool change_age(Person &person, int desired);
}

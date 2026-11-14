#pragma once

#include <cstddef>

namespace demo {
struct Person;

struct WriteBatch {
  Person *people;
  std::size_t count;
  void *completed;

  void run();
};
}

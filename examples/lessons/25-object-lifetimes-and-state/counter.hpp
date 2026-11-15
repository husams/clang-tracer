#pragma once

namespace lesson {

struct Counter {
  int value;

  explicit Counter(int initial);
  ~Counter();
  int add(int amount);
};

}  // namespace lesson

#pragma once

namespace lesson {

struct Counter {
  int value;
  explicit Counter(int initial);
  ~Counter();
  int add(int amount);
};

struct Base {
  int base;
  explicit Base(int initial);
  virtual ~Base();
  virtual int score(int amount) const;
};

struct Derived final : Base {
  int bonus;
  Derived(int initial, int extra);
  ~Derived() override;
  int score(int amount) const override;
};

int through_base(const Base& object, int amount);

}  // namespace lesson

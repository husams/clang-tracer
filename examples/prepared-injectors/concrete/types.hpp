#pragma once

namespace prepared_concrete {

struct Prefix {
  virtual explicit operator bool() const;
  virtual int unused() const;
  virtual ~Prefix() = default;
  int prefix = 1;
};

struct Interface {
  virtual int route(int value) const = 0;
  virtual ~Interface() = default;
  int base = 3;
};

struct VirtualState { int shared = 9; };

struct Handler final : Prefix, Interface, virtual VirtualState {
  int factor = 7;
  int route(int value) const override;
  template<class T> T convert(T value) const { return value + factor; }
};

template<> double Handler::convert<double>(double value) const;

template<class T> T twice(T value) {
  const T original = value;
  return value + original;
}
template<> double twice<double>(double value);

template<class T, int Bias> struct Box {
  T stored;
  T apply(T value) const { return value + stored + Bias; }
};

int through_base(const Interface *object, int value);

} // namespace prepared_concrete

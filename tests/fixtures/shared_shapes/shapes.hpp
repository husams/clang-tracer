#pragma once

#include <memory>
#include <vector>

namespace demo {
class Shape {
  const char *name_;
public:
  explicit Shape(const char *name) : name_(name) {}
  virtual ~Shape() = default;
  virtual int resize(int value) = 0;
  virtual int size() const = 0;
  const char *name() const { return name_; }
};

class Circle final : public Shape {
  int radius;
public:
  explicit Circle(int initial);
  ~Circle() override;
  static void *operator new(std::size_t size);
  static void operator delete(void *pointer) noexcept;
  int resize(int value) override;
  int size() const override;
  const int *field() const { return &radius; }
};

class Image {
  std::vector<std::unique_ptr<Shape>> _shapes;
public:
  Image() : _shapes(1) {}
  Shape *add(Shape *shape);
  void remove();
  Shape &get();
};
} // namespace demo

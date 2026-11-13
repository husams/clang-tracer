#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <unistd.h>

#if defined(TRACE_ARM_TEST_RENDEZVOUS)
#include "rendezvous.hpp"
#endif

#if defined(TRACE_TEST_OWNER_SELECTORS)
#define OWNER_ADD insert
#define OWNER_GET at
#define OWNER_REMOVE erase
#define OWNER_SNAPSHOT describe
#else
#define OWNER_ADD add
#define OWNER_GET get
#define OWNER_REMOVE remove
#define OWNER_SNAPSHOT set_description
#endif

namespace ownership_test {
class Shape {
  std::string description_;

public:
  virtual ~Shape();
  void OWNER_SNAPSHOT(const std::string &description);
};

#if defined(TRACE_TEST_SECONDARY_BASE)
class Primary {
  int marker_ = 19;

public:
  virtual ~Primary() = default;
};
class Circle final : public Primary, public Shape {
#else
class Circle final : public Shape {
#endif
  int radius_;

public:
  explicit Circle(int radius);
  ~Circle() override;
  void set_radius(int radius);
  int radius() const { return radius_; }
};

class Image {
  std::vector<std::unique_ptr<Shape>> shapes_;

public:
  void OWNER_ADD(std::unique_ptr<Shape> shape);
  Shape *OWNER_GET();
  bool OWNER_REMOVE();
  std::unique_ptr<Shape> take();
};

Shape::~Shape() = default;
void Shape::OWNER_SNAPSHOT(const std::string &description) { description_ = description; }
Circle::Circle(int radius) : radius_(radius) {}
Circle::~Circle() = default;
void Circle::set_radius(int radius) { radius_ = radius; }

void Image::OWNER_ADD(std::unique_ptr<Shape> shape) { shapes_.push_back(std::move(shape)); }
Shape *Image::OWNER_GET() { return shapes_.front().get(); }
bool Image::OWNER_REMOVE() {
  if (shapes_.empty())
    return false;
  shapes_.erase(shapes_.begin());
  return true;
}

std::unique_ptr<Shape> Image::take() {
  auto result = std::move(shapes_.front());
  shapes_.erase(shapes_.begin());
  return result;
}
}

int main() {
  using namespace ownership_test;
  Image first, second;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
#if defined(TRACE_ARM_TEST_RENDEZVOUS)
  arm_rendezvous();
#endif
  while (::access("transfer.now", F_OK))
    ::usleep(1000);
  first.OWNER_ADD(std::make_unique<Circle>(40));
  auto *before = static_cast<Circle *>(first.OWNER_GET());
  before->set_radius(41);
  second.OWNER_ADD(first.take());
  auto *after = static_cast<Circle *>(second.OWNER_GET());
  after->set_radius(42);
#if defined(TRACE_TEST_SECONDARY_BASE)
  if (static_cast<void *>(static_cast<Shape *>(after)) == static_cast<void *>(after))
    return 3;
#endif
  std::printf("OWNERS first=%p second=%p circle=%p\n", static_cast<void *>(&first),
              static_cast<void *>(&second), static_cast<void *>(after));
  if (before != after || after->radius() != 42 || !second.OWNER_REMOVE() || first.OWNER_REMOVE())
    return 2;
  while (::access("stop.now", F_OK))
    ::usleep(1000);
  return 0;
}

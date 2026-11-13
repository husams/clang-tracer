#include <csignal>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <unistd.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#ifndef PR_SET_PTRACER_ANY
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define BOUNDARY __attribute__((noipa))
#else
#define BOUNDARY __attribute__((noinline))
#endif

namespace demo {
class Shape {
  std::string _name;
  std::string _description;
public:
  Shape(std::string name, std::string description);
  virtual ~Shape();
  virtual int size() const = 0;
  const std::string &name() const { return _name; }
  BOUNDARY void set_description(const std::string &description);
};

class Circle final : public Shape {
  int radius;
public:
  BOUNDARY Circle(std::string name, std::string description, int value);
  BOUNDARY ~Circle() override;
  BOUNDARY int size() const override;
  void set_radius(int value) { radius = value; } // WRITER_LINE
  static void *operator new(std::size_t);
  static void operator delete(void *) noexcept;
};

class Image {
  std::vector<std::unique_ptr<Shape>> _shapes;
public:
  BOUNDARY void add(std::unique_ptr<Shape> shape);
  BOUNDARY Shape &get(const std::string &name);
  BOUNDARY bool remove(const std::string &name);
};
} // namespace demo

namespace {
constexpr std::size_t page = 4096;
alignas(page) unsigned char circle_slot[page];
bool occupied = false;
volatile std::sig_atomic_t faults = 0;

void make_writable(int, siginfo_t *info, void *) {
  if (info->si_code != SEGV_ACCERR ||
      info->si_addr < static_cast<void *>(circle_slot) ||
      info->si_addr >= static_cast<void *>(circle_slot + page) ||
      ::mprotect(circle_slot, page, PROT_READ | PROT_WRITE) != 0)
    _exit(91);
  faults = 1;
}
} // namespace

namespace demo {
Shape::Shape(std::string name, std::string description)
    : _name(std::move(name)), _description(std::move(description)) {}
Shape::~Shape() = default;
void Shape::set_description(const std::string &description) {
  _description = description;
}

Circle::Circle(std::string name, std::string description, int value)
    : Shape(std::move(name), std::move(description)), radius(value) {}
Circle::~Circle() {
  std::printf("destroy %p radius=%d\n", static_cast<void *>(this), radius);
}
int Circle::size() const { return radius; }
void *Circle::operator new(std::size_t size) {
  if (occupied || size != sizeof(Circle))
    throw std::bad_alloc();
  occupied = true;
  return circle_slot;
}
void Circle::operator delete(void *address) noexcept {
  if (!occupied || address != circle_slot)
    std::abort();
  occupied = false;
}

void Image::add(std::unique_ptr<Shape> shape) {
  _shapes.push_back(std::move(shape));
}
Shape &Image::get(const std::string &name) {
  for (const auto &shape : _shapes)
    if (shape->name() == name)
      return *shape;
  throw std::out_of_range(name);
}
bool Image::remove(const std::string &name) {
  for (auto it = _shapes.begin(); it != _shapes.end(); ++it) {
    if ((*it)->name() == name) {
      _shapes.erase(it);
      return true;
    }
  }
  return false;
}
} // namespace demo

extern "C" BOUNDARY int borrowed_write(demo::Image &image, const std::string &name,
                                       int value, bool update_description, bool fault) {
  auto &shape = image.get(name);
  if (update_description)
    shape.set_description("selected");
  auto &circle = dynamic_cast<demo::Circle &>(shape);
  if (fault && ::mprotect(circle_slot, page, PROT_READ) != 0)
    std::abort();
  circle.set_radius(value);
  return circle.size();
}

void exercise(bool fault) {
  demo::Image image;
  const std::string name = "icon";
  image.add(std::make_unique<demo::Circle>(name, "draft", 10));
  if (borrowed_write(image, name, 15, true, fault) != 15 || !image.remove(name))
    std::abort();
  image.add(std::make_unique<demo::Circle>(name, "replacement", 20));
  if (borrowed_write(image, name, 25, false, false) != 25 || !image.remove(name))
    std::abort();
}

int main() {
  if (::sysconf(_SC_PAGESIZE) != page || ::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0)
    return 90;
  struct sigaction action {};
  action.sa_sigaction = make_writable;
  action.sa_flags = SA_SIGINFO;
  if (::sigaction(SIGSEGV, &action, nullptr) != 0)
    return 91;
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  if (::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0)
    return 92;
  std::puts("READY");
  for (;;) {
    char command;
    const auto count = ::read(STDIN_FILENO, &command, 1);
    if (!count || (count == 1 && command == 'q'))
      break;
    if (count < 0) {
      if (errno != EAGAIN && errno != EINTR)
        return 93;
      ::usleep(10000);
      continue;
    }
    if (command == 't') {
      std::thread peer([] { std::puts("PEER"); });
      peer.join();
      std::puts("THREAD DONE");
      continue;
    }
    if (command != 'g' && command != 'f')
      continue;
    faults = 0;
    exercise(command == 'f');
    std::printf("DONE fault=%d\n", static_cast<int>(faults));
  }
}

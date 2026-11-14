#include "shapes.hpp"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <optional>

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <unistd.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#ifndef PR_SET_PTRACER_ANY
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif

__attribute__((noinline, patchable_function_entry(5)))
demo::Shape *demo::Image::add(Shape *shape) {
  if (_shapes.front())
    return nullptr;
  std::unique_ptr<Shape> incoming(shape);
  incoming.swap(_shapes.front());
  return shape;
}

__attribute__((noinline)) demo::Shape &demo::Image::get() { return *_shapes.front(); }
__attribute__((noinline)) void demo::Image::remove() { _shapes.front().reset(); }

extern "C" __attribute__((noinline)) int borrowed_resize(demo::Image &image, int value) {
  auto &shape = image.get();
  return shape.resize(value);
}

namespace {
std::uintptr_t virtual_target(const demo::Shape *shape, unsigned slot) {
  // Inspect the Itanium vtable bytes through the object representation.
  std::uintptr_t table, target;
  std::memcpy(&table, static_cast<const void *>(shape), sizeof(table));
  std::memcpy(&target, reinterpret_cast<const void *>(table + slot * sizeof(target)),
              sizeof(target));
  return target;
}

char command() {
  for (;;) {
    char input = 0;
    const auto n = ::read(STDIN_FILENO, &input, 1);
    if (n > 0 && input != '\n')
      return input;
    if (!n || (n < 0 && errno != EAGAIN && errno != EINTR))
      return 'q';
    ::usleep(1000);
  }
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2 || ::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) ||
      ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK))
    return 2;
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  auto *handle = ::dlopen(argv[1], RTLD_LOCAL | RTLD_NOW);
  if (!handle)
    return 3;
  const auto make_circle = reinterpret_cast<demo::Shape *(*)(int)>(::dlsym(handle, "make_circle"));
  const auto make_square = reinterpret_cast<demo::Shape *(*)()>(::dlsym(handle, "make_square"));
  const auto make_control = reinterpret_cast<demo::Shape *(*)()>(::dlsym(handle, "make_control_circle"));
  const auto slot_available = reinterpret_cast<bool (*)()>(::dlsym(handle, "circle_slot_available"));
  const auto radius = reinterpret_cast<const int *(*)(demo::Shape *)>(::dlsym(handle, "radius_address"));
  const auto direct = reinterpret_cast<int (*)(demo::Shape *, int)>(::dlsym(handle, "direct_circle_resize"));
  if (!make_circle || !make_square || !make_control || !slot_available || !radius || !direct)
    return 4;
  std::optional<demo::Image> image(std::in_place), alternate(std::in_place);
  auto *square = make_square();
  std::unique_ptr<demo::Shape> direct_circle(make_control());
  alternate->add(square);
  demo::Shape *circle = nullptr;
  std::uintptr_t first_address = 0;
  unsigned cycle = 0;
  std::printf("READY image=%p circle_target=%#lx alternate_target=%#lx\n",
      static_cast<void *>(&*image),
      static_cast<unsigned long>(virtual_target(direct_circle.get(), 2)),
      static_cast<unsigned long>(virtual_target(square, 2)));
  for (;;) {
    const auto c = command();
    if (c == 'o') {
      if (borrowed_resize(*alternate, 21) != 21 || square->size() != 21)
        return 5;
      std::puts("OTHER result=21");
    } else if (c == 's' && !circle && cycle < 2) {
      const int initial = ++cycle * 10;
      circle = make_circle(initial);
      auto *complete = dynamic_cast<void *>(circle);
      const auto address = reinterpret_cast<std::uintptr_t>(complete);
      if (cycle == 1)
        first_address = address;
      if (complete != static_cast<void *>(circle) || address != first_address ||
          std::strcmp(circle->name(), "icon") ||
          image->add(circle) != circle || circle->size() != initial)
        return 6;
      std::printf("ADDED cycle=%u circle=%p field=%p name=%s destroy=%#lx\n", cycle,
          complete, static_cast<const void *>(radius(circle)), circle->name(),
          static_cast<unsigned long>(virtual_target(circle, 1)));
    } else if (c == 'g' && circle) {
      const int value = cycle * 10 + 5;
      const auto start = std::chrono::steady_clock::now();
      const auto returned = borrowed_resize(*image, value);
      const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now() - start).count();
      if (returned != value || circle->size() != value)
        return 7;
      std::printf("WROTE cycle=%u returned=%d radius=%d ns=%lld\n", cycle, returned,
                  circle->size(), static_cast<long long>(elapsed));
    } else if (c == 'x' && circle) {
      image->remove();
      circle = nullptr;
      if (!slot_available())
        return 8;
      std::printf("REMOVED cycle=%u slot_free=yes\n", cycle);
    } else if (c == 'r' && !circle) {
      auto *after = make_circle(10);
      if (reinterpret_cast<std::uintptr_t>(dynamic_cast<void *>(after)) != first_address ||
          image->add(after) != after || borrowed_resize(*image, 15) != 15 || after->size() != 15)
        return 9;
      image->remove();
      if (!slot_available())
        return 10;
      std::puts("AFTER_DETACH radius=15 slot_free=yes");
    } else if ((c == 'c' || c == 'u') && handle) {
      if (c == 'u') {
        image.reset();
        alternate.reset();
        direct_circle.reset();
      }
      if (::dlclose(handle))
        return 11;
      handle = nullptr;
      std::puts("CLOSED");
    } else if (c == 'd') {
      if (direct(direct_circle.get(), 17) != 17 || direct_circle->size() != 17)
        return 12;
      std::puts("DIRECT result=17");
    } else if (c == 'q') {
      image.reset();
      alternate.reset();
      direct_circle.reset();
      if (handle)
        ::dlclose(handle);
      std::puts("EXIT");
      return 0;
    }
  }
}

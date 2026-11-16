#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/prctl.h>
#include <unistd.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif

namespace app {
struct Foo { int value = 0; };
struct Types { struct Foo { int value = 0; }; };
struct Plain {
  int state_ = 0;
  __attribute__((noinline)) void set(int value) { state_ = value; }
};
template <class T, int N> struct Item {
  int state_ = 0;
  T argument{};
  __attribute__((noinline)) void set(int value) { state_ = value + N; }
};
}

// No outer namespace: the only scope separators are in the argument.
template <class T, int N> struct Item {
  int state_ = 0;
  T argument{};
  __attribute__((noinline)) void set(int value) { state_ = value + N; }
};

int main() {
  if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) || ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK))
    return 1;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  app::Plain plain;
  app::Item<int, 3> simple;
  app::Item<app::Foo, 3> qualified;
  app::Item<app::Item<app::Foo, 3>, 4> nested;
  app::Item<app::Types::Foo, 3> member;
  Item<app::Foo, 3> global;
  std::puts("READY");
  int value = 20;
  for (;;) {
    char command;
    const auto size = ::read(STDIN_FILENO, &command, 1);
    if (size == 0)
      return 0;
    if (size < 0) {
      if (errno != EAGAIN && errno != EINTR)
        return 3;
      ::usleep(1000);
      continue;
    }
    if (command != 'g')
      continue;
    plain.set(value);
    simple.set(value);
    qualified.set(value);
    nested.set(value);
    member.set(value);
    global.set(value);
    if (plain.state_ != value || simple.state_ != value + 3 ||
        qualified.state_ != value + 3 || nested.state_ != value + 4 ||
        member.state_ != value + 3 || global.state_ != value + 3)
      return 2;
    std::puts("WROTE");
    value += 19;
  }
}

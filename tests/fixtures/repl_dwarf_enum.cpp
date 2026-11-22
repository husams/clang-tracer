// Enumerations and member pointers in DWARF-resolved signatures (issue #136).
//
// One pass per "g" byte on stdin, so a test counts events exactly. Every traced function is
// noinline and its value is checked, so a call the tracer corrupted prints a mismatch and ends
// the program with status 1. The loop polls a non-blocking stdin so the tracer can attach while
// the program waits.
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/prctl.h>
#include <system_error>
#include <unistd.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#ifndef PR_SET_PTRACER_ANY
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif

#define NOINLINE __attribute__((noinline))

namespace enums {

// Supported: the underlying type is int, long long, unsigned long or bool, which have an exact
// DWARF trace value type (int32, int64, uint64, bool).
enum Sign { Minus = -1, Plus = 1 };
enum class Flag : bool { No, Yes };
enum class Wide : long long { Low = -5, High = 1LL << 40 };
enum class Big : unsigned long { Small = 1, Huge = 1UL << 63 };

// Unsupported, with a diagnostic that names the enumeration: both compilers give it unsigned int
// and the detour has no exact trace value type for that, and unsigned char is narrower than any.
enum Color { Red, Green };
enum class Narrow : unsigned char { Idle, Busy = 200 };

struct Order {
  enum class State { New, Paid };
  int total = 17;
  NOINLINE int plain(int x) { return x + 1; }
  NOINLINE int set_state(State s) { return static_cast<int>(s); }
  NOINLINE State state_of(int v) { return v ? State::Paid : State::New; }
  NOINLINE int by_ref(const State& s) { return static_cast<int>(s) + 10; }
  // Unsupported: a pointer to a data member.
  NOINLINE int read(int Order::*field) { return this->*field; }
};

NOINLINE Sign flip(Sign s) { return s == Minus ? Plus : Minus; }
NOINLINE Wide widen(Wide w) { return w == Wide::Low ? Wide::High : Wide::Low; }
NOINLINE int big(Big b) { return b == Big::Huge ? 2 : 1; }
NOINLINE int flag(Flag f) { return f == Flag::Yes ? 1 : 0; }
// A standard-library enumeration: its header is chosen automatically.
NOINLINE int code(std::errc e) { return static_cast<int>(e); }
NOINLINE Color paint(Color c) { return c == Red ? Green : Red; }
NOINLINE Narrow narrow(Narrow n) { return n == Narrow::Idle ? Narrow::Busy : Narrow::Idle; }
// Unsupported: a pointer to a member function.
NOINLINE int call(Order* order, int (Order::*function)(int)) { return (order->*function)(3); }

}  // namespace enums

namespace {

void check(bool condition, const char* what, int pass) {
  if (condition)
    return;
  std::printf("MISMATCH %s pass %d\n", what, pass);
  std::fflush(stdout);
  std::exit(1);
}

// One exact pass. The number of calls to each traced function is fixed: the test counts them.
void run_once(int pass) {
  using namespace enums;
  Order order;
  check(order.plain(pass) == pass + 1, "plain", pass);
  check(order.set_state(Order::State::Paid) == 1, "set_state", pass);
  check(order.state_of(1) == Order::State::Paid, "state_of(1)", pass);
  check(order.state_of(0) == Order::State::New, "state_of(0)", pass);
  const Order::State paid = Order::State::Paid;
  check(order.by_ref(paid) == 11, "by_ref", pass);
  check(flip(Minus) == Plus, "flip(Minus)", pass);
  check(flip(Plus) == Minus, "flip(Plus)", pass);
  check(widen(Wide::Low) == Wide::High, "widen(Low)", pass);
  check(widen(Wide::High) == Wide::Low, "widen(High)", pass);
  check(big(Big::Huge) == 2, "big(Huge)", pass);
  check(big(Big::Small) == 1, "big(Small)", pass);
  check(flag(Flag::Yes) == 1, "flag", pass);
  check(code(std::errc::invalid_argument) == 22, "code", pass);
  check(paint(Red) == Green, "paint", pass);
  check(narrow(Narrow::Idle) == Narrow::Busy, "narrow", pass);
  check(order.read(&Order::total) == 17, "read", pass);
  check(call(&order, &Order::plain) == 4, "call", pass);
}

}  // namespace

int main() {
  if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0 ||
      ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0)
    return 2;
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  std::puts("READY");
  int pass = 0;
  for (;;) {
    char command = 0;
    const auto count = ::read(STDIN_FILENO, &command, 1);
    if (count == 0 || command == 'q')
      break;
    if (count < 0) {
      if (errno != EAGAIN && errno != EINTR)
        return 4;
      ::usleep(10000);
      continue;
    }
    if (command == 'g') {
      run_once(pass);
      std::printf("PASS %d ok\n", pass);
      ++pass;
    }
  }
  std::puts("EXIT");
  return 0;
}

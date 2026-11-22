// Receiver layouts that headerless DWARF callbacks rebuild (issue #146).
//
// Every class has a `get() const` called once per pass on an object with fixed member values, so a
// test counts events exactly and reads exact member values. One pass per "g" byte on stdin. Each
// traced method is noinline and its result is checked, so a call the tracer corrupted prints a
// mismatch and ends the program with status 1. The loop polls a non-blocking stdin so the tracer
// can attach while the program waits.
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/prctl.h>
#include <unistd.h>

#include "repl_dwarf_layout.hpp"

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#ifndef PR_SET_PTRACER_ANY
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif

#define NOINLINE __attribute__((noinline))

namespace layout {

NOINLINE int Plain::get() const { return a + b; }
NOINLINE int Derived::get() const { return base_value + own; }
NOINLINE int Level2::get() const { return base_value + own + level; }
NOINLINE int Multi::get() const { return base_value + second_value + multi_own; }
NOINLINE int Bits::get() const { return static_cast<int>(a + b) + plain; }
NOINLINE int BitGap::get() const {
  return static_cast<int>(a + b) + s + (flag ? 1 : 0) + static_cast<int>(wide);
}
NOINLINE int Holder::get() const { return in.x + y; }
NOINLINE int UHolder::get() const { return u.i + z; }
NOINLINE int Arr::get() const { return a[0] + a[3]; }
NOINLINE int AnonMem::get() const { return a + x + y + u; }
NOINLINE int Deep::get() const { return items[1].x + pair.hi + m[1][2]; }
NOINLINE int Outer::get() const { return in.v + tail; }
NOINLINE int Aligned::get() const { return a; }
NOINLINE int MemberAligned::get() const { return c + wide; }
NOINLINE int Poly::vget() const { return a; }
NOINLINE int Poly::get() const { return a + vget(); }
NOINLINE int VD::get() const { return d + vb; }
NOINLINE int Packed::get() const { return c + i; }
NOINLINE int WithEnum::get() const { return c + i; }
NOINLINE int WithPtr::get() const { return i; }

// Classes of the new shapes passed and returned by value: the target checks every argument and
// result, so a corrupted call ends the program.
NOINLINE int take_holder(Holder h) { return h.in.x * 100 + h.y; }
NOINLINE int take_arr(Arr a) { return a.a[0] + a.a[1] * 10 + a.a[2] * 100 + a.a[3] * 1000; }
NOINLINE int take_derived(Derived d) { return d.base_value * 100 + d.own; }
NOINLINE int take_uholder(UHolder u) { return u.u.i * 100 + u.z; }
NOINLINE int take_bits(Bits b) { return static_cast<int>(b.a) * 1000 + static_cast<int>(b.b) * 100 + b.plain; }
// Returned in registers, and returned in memory.
NOINLINE Holder make_holder(int v) {
  Holder h;
  h.in.x = v;
  h.y = v + 1;
  return h;
}
NOINLINE Deep make_deep() {
  Deep d;
  d.m[1][2] = 42;
  return d;
}
// A union passed and returned by value, in a register.
NOINLINE int take_u(U u) { return u.i * 3 + 1; }
NOINLINE U make_u(int v) {
  U u;
  u.i = v;
  return u;
}
// The same class behind a reference.
NOINLINE int ref_holder(const Holder& h) { return h.in.x * 100 + h.y; }

}  // namespace layout

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
  using namespace layout;
  check(Plain().get() == 3, "Plain", pass);
  check(Derived().get() == 33, "Derived", pass);
  check(Level2().get() == 88, "Level2", pass);
  check(Multi().get() == 88, "Multi", pass);
  check(Bits().get() == 8, "Bits", pass);
  check(BitGap().get() == 3 + (-7) + 1 + 77, "BitGap", pass);
  check(Holder().get() == 3, "Holder", pass);
  check(UHolder().get() == 4, "UHolder", pass);
  check(Arr().get() == 5, "Arr", pass);
  check(AnonMem().get() == 10, "AnonMem", pass);
  check(Deep().get() == 6 + 8 + 5, "Deep", pass);
  check(Outer().get() == 19, "Outer", pass);
  check(Aligned().get() == 1, "Aligned", pass);
  check(MemberAligned().get() == 67, "MemberAligned", pass);
  check(Poly().get() == 2, "Poly", pass);
  check(VD().get() == 3, "VD", pass);
  check(Packed().get() == 3, "Packed", pass);
  check(WithEnum().get() == 4, "WithEnum", pass);
  check(WithPtr().get() == 3, "WithPtr", pass);

  Holder held;
  held.in.x = 3;
  held.y = 4;
  check(take_holder(held) == 304, "take_holder", pass);
  check(take_arr(Arr()) == 4321, "take_arr", pass);
  check(take_derived(Derived()) == 1122, "take_derived", pass);
  UHolder united;
  united.u.i = 9;
  check(take_uholder(united) == 904, "take_uholder", pass);
  check(take_bits(Bits()) == 1205, "take_bits", pass);
  const Holder made = make_holder(5);
  check(made.in.x == 5 && made.y == 6, "make_holder", pass);
  const Deep deep = make_deep();
  check(deep.m[1][2] == 42 && deep.items[1].x == 6 && deep.pair.hi == 8 && deep.m[0][1] == 1,
        "make_deep", pass);
  U one;
  one.i = 7;
  check(take_u(one) == 22, "take_u", pass);
  check(make_u(8).i == 8, "make_u", pass);
  check(ref_holder(held) == 304, "ref_holder", pass);
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

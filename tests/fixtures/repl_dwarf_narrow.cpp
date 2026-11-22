// Narrow and unsigned 32-bit integers in DWARF-resolved signatures (issue #145).
//
// One pass per "g" byte on stdin, so a test counts events exactly. Every traced function is
// noinline and its result is checked, so a call the tracer corrupted prints a mismatch and ends
// the program with status 1. The loop polls a non-blocking stdin so the tracer can attach while
// the program waits.
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/prctl.h>
#include <unistd.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#ifndef PR_SET_PTRACER_ANY
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif

#define NOINLINE __attribute__((noinline))

namespace narrow {

using Byte = unsigned char;

// Each function maps one distinguished argument to a different distinguished result, so the test
// sees a width or signedness error in either. The values are chosen at the edge of the type.
NOINLINE char t_char(char v) { return v == -5 ? char(-7) : char(0); }
NOINLINE signed char t_schar(signed char v) {
  return v == -128 ? static_cast<signed char>(127) : static_cast<signed char>(0);
}
NOINLINE unsigned char t_uchar(unsigned char v) { return v == 250 ? 5 : 0; }
NOINLINE short t_short(short v) { return v == -3 ? short(-32768) : short(0); }
NOINLINE unsigned short t_ushort(unsigned short v) { return v == 65535 ? 1 : 0; }
NOINLINE unsigned int t_uint(unsigned int v) { return v == 4000000000u ? 3000000000u : 0u; }
NOINLINE char16_t t_c16(char16_t v) { return v == 0xFFFE ? char16_t(0x8001) : char16_t(0); }
NOINLINE char32_t t_c32(char32_t v) { return v == 0x10FFFF ? char32_t(0xFFFFFFFEu) : char32_t(0); }
NOINLINE wchar_t t_wchar(wchar_t v) { return v == -9 ? wchar_t(-70000) : wchar_t(0); }

// Controls that worked before the change.
NOINLINE int t_int(int v) { return v == -1 ? 2 : 0; }
NOINLINE long t_long(long v) { return v == -(1L << 40) ? (1L << 41) : 0; }
NOINLINE unsigned long t_ulong(unsigned long v) { return v == (1UL << 63) ? 5UL : 0UL; }
NOINLINE bool t_bool(bool v) { return !v; }
NOINLINE double t_double(double v) { return v == 1.5 ? 2.5 : 0.0; }

// A by-value parameter with a top-level const, and a typedef of a narrow type.
NOINLINE int t_cshort(const short v) { return v == -3 ? 33 : 0; }
NOINLINE Byte t_byte(Byte v) { return v == 250 ? Byte(6) : Byte(0); }

// Six narrow arguments of different widths in one call: a value that lands in the wrong slot
// changes the result.
NOINLINE int t_mix(signed char a, unsigned char b, short c, unsigned short d, unsigned int e,
                   char16_t f) {
  return a + b + c + d + static_cast<int>(e >> 16) + f;
}

// Unsupported: no exact trace value type for these.
NOINLINE int t_i128(__int128 v) { return static_cast<int>(v); }
NOINLINE long double t_ldouble(long double v) { return v + 1; }
NOINLINE int t_ldarg(long double v) { return v == 1.5L ? 4 : 0; }

struct Meter {
  int total = 0;
  NOINLINE short mix(unsigned char a, short b) { return static_cast<short>(a - 300 + b); }
  NOINLINE unsigned short widen(unsigned short v) { return v == 65535 ? 2 : 0; }
};

}  // namespace narrow

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
  using namespace narrow;
  check(t_char(-5) == -7, "t_char", pass);
  check(t_schar(-128) == 127, "t_schar", pass);
  check(t_uchar(250) == 5, "t_uchar", pass);
  check(t_short(-3) == -32768, "t_short", pass);
  check(t_ushort(65535) == 1, "t_ushort", pass);
  check(t_uint(4000000000u) == 3000000000u, "t_uint", pass);
  check(t_c16(0xFFFE) == 0x8001, "t_c16", pass);
  check(t_c32(0x10FFFF) == 0xFFFFFFFEu, "t_c32", pass);
  check(t_wchar(-9) == -70000, "t_wchar", pass);
  check(t_int(-1) == 2, "t_int", pass);
  check(t_long(-(1L << 40)) == (1L << 41), "t_long", pass);
  check(t_ulong(1UL << 63) == 5UL, "t_ulong", pass);
  check(t_bool(false), "t_bool", pass);
  check(t_double(1.5) == 2.5, "t_double", pass);
  check(t_cshort(-3) == 33, "t_cshort", pass);
  check(t_byte(250) == 6, "t_byte", pass);
  check(t_mix(-1, 200, -300, 60000, 3000000000u, 0xABCD) == -1 + 200 - 300 + 60000 + 45776 + 0xABCD,
        "t_mix", pass);
  check(t_i128(7) == 7, "t_i128", pass);
  check(t_ldouble(1.5L) == 2.5L, "t_ldouble", pass);
  check(t_ldarg(1.5L) == 4, "t_ldarg", pass);
  Meter meter;
  check(meter.mix(250, -3) == -53, "Meter::mix", pass);
  check(meter.widen(65535) == 2, "Meter::widen", pass);

  // A caller is free to leave the upper half of the register of a 32-bit argument undefined. The
  // function is called as if it took an unsigned long, with garbage above the argument.
  using Wide = unsigned long (*)(unsigned long);
  Wide volatile wide = reinterpret_cast<Wide>(&t_uint);
  const unsigned long dirty = wide(0xDEADBEEF00000000ul | 4000000000u);
  check(static_cast<unsigned>(dirty) == 3000000000u, "t_uint with garbage in the upper half", pass);
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

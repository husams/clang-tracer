// Variadic callables with known results. Observing the fixed arguments must leave every result
// unchanged, whatever the unknown tail holds: integer registers, vector registers (and the
// %al count that announces them) and stack arguments.
//
// One "g" byte on stdin runs one pass over every callable and prints "PASS <n> bad=<count>". The
// loop polls a non-blocking stdin so the tracer can attach while the program waits.
#include <cerrno>
#include <cstdarg>
#include <cstdio>
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

namespace variadic {
NOINLINE int vlog(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int sum = 0;
  for (const char *p = fmt; *p; ++p)
    sum += va_arg(ap, int);
  va_end(ap);
  return sum;
}
struct Logger {
  NOINLINE int log(const char *fmt, ...);
};
NOINLINE int Logger::log(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int sum = 0;
  for (const char *p = fmt; *p; ++p)
    sum += va_arg(ap, int);
  va_end(ap);
  return sum;
}
NOINLINE int fixed(const char *fmt, int n) { return *fmt + n; }
NOINLINE int sum_ints(int count, ...) {
  va_list ap;
  va_start(ap, count);
  int sum = 0;
  for (int i = 0; i < count; ++i)
    sum += va_arg(ap, int);
  va_end(ap);
  return sum;
}
// Twelve integer arguments after one fixed pointer: five stay in registers, seven are on the stack.
NOINLINE int spill(const char *tag, ...) {
  va_list ap;
  va_start(ap, tag);
  int sum = tag[0];
  for (int i = 0; i < 12; ++i)
    sum += va_arg(ap, int) * (i + 1);
  va_end(ap);
  return sum;
}
// Ten doubles: eight in vector registers (so %al is 8), two on the stack.
NOINLINE double dsum(int count, ...) {
  va_list ap;
  va_start(ap, count);
  double sum = 0;
  for (int i = 0; i < count; ++i)
    sum += va_arg(ap, double);
  va_end(ap);
  return sum;
}
NOINLINE double mixed(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  double sum = 0;
  for (const char *p = fmt; *p; ++p) {
    if (*p == 'i')
      sum += va_arg(ap, int);
    else if (*p == 'd')
      sum += va_arg(ap, double);
    else
      sum += va_arg(ap, long);
  }
  va_end(ap);
  return sum;
}
NOINLINE int fmtlog(const char *fmt, ...) {
  char buffer[256];
  va_list ap;
  va_start(ap, fmt);
  const int length = std::vsnprintf(buffer, sizeof buffer, fmt, ap);
  va_end(ap);
  return length + buffer[0];
}
} // namespace variadic

extern "C" NOINLINE int c_vlog(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int sum = 0;
  for (const char *p = fmt; *p; ++p)
    sum += va_arg(ap, int);
  va_end(ap);
  return sum;
}

namespace {
int bad = 0;
template <class T> void check(const char *what, T got, T want) {
  if (got != want) {
    ++bad;
    std::printf("MISMATCH %s got=%g want=%g\n", what, static_cast<double>(got),
                static_cast<double>(want));
  }
}
} // namespace

int main() {
  if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0 || ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0)
    return 2;
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  std::puts("READY");
  for (int pass = 0;;) {
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
    if (command != 'g')
      continue;
    variadic::Logger logger;
    check("vlog", variadic::vlog("ab", 1, 2), 3);
    check("Logger::log", logger.log("xyz", 5, 6, 7), 18);
    check("c_vlog", c_vlog("yy", 20, 22), 42);
    check("fixed", variadic::fixed("z", 3), 'z' + 3);
    check("sum_ints", variadic::sum_ints(3, 1, 2, 3), 6);
    check("sum_ints12", variadic::sum_ints(12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12), 78);
    check("spill", variadic::spill("A", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12), 'A' + 650);
    check("dsum", variadic::dsum(10, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5, 10.5), 60.0);
    check("mixed", variadic::mixed("idlidlidl", 1, 2.5, 3L, 4, 5.5, 6L, 7, 8.5, 9L), 46.5);
    check("fmtlog", variadic::fmtlog("%d %s %.2f %d %d %d %d %d %d", 1, "two", 3.25, 4, 5, 6, 7, 8, 9),
          22 + '1');
    std::printf("PASS %d bad=%d\n", pass++, bad);
  }
}

// One pass of every function per "g" byte on stdin, so a test counts events exactly. The loop
// polls a non-blocking stdin so the tracer can attach while the program waits.
#include "anon_pointer.hpp"

#include <cerrno>
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

int main() {
  if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0 ||
      ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0)
    return 2;
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  std::printf("ADDRESS a=%p b=%p\n", static_cast<const void *>(anonp::unit_a_address()),
              static_cast<const void *>(anonp::unit_b_address()));
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
      const int sum = anonp::run_unit_a(pass) + anonp::run_unit_b(pass) + anonp::run_local(pass) +
                      anonl::run_library_one(pass) + anonl::run_library_two(pass);
      std::printf("PASS %d %d\n", pass, sum);
      ++pass;
    }
  }
  std::puts("EXIT");
  return 0;
}

// One pass of repl_capture_install.hpp per "g" byte on stdin, so a test counts events exactly.
// Each line reports how often every function has run. The loop polls a non-blocking stdin so the
// tracer can attach while the program waits.
#include "repl_capture_install.hpp"

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
      capi::run_once();
      std::printf("PASS %d", pass);
#define CAPI_PRINT(name, type) std::printf(" " #name "=%d", capi::calls.name);
      CAPI_CASES(CAPI_PRINT)
#undef CAPI_PRINT
      std::printf(" receiver=%d", capi::calls.receiver);
      std::printf("\n");
      ++pass;
    }
  }
  std::puts("EXIT");
  return 0;
}

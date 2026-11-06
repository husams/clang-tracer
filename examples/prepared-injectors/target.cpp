#include "calculator.hpp"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <ctime>

#include <sys/prctl.h>
#include <unistd.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#ifndef PR_SET_PTRACER_ANY
#define PR_SET_PTRACER_ANY (~0UL)
#endif

namespace {
volatile std::sig_atomic_t running = 1;
void stop(int) { running = 0; }
} // namespace

int main() {
  std::signal(SIGINT, stop);
  std::signal(SIGTERM, stop);
  // Allow the separately launched tracer to attach to this demo only. No
  // system-wide ptrace setting needs to be changed.
  if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) != 0) {
    std::perror("allow tracing of the example target");
    return 1;
  }
  std::printf("TARGET pid=%ld bias=%d\n", static_cast<long>(::getpid()),
              PREPARED_DEMO_BIAS);
  std::fflush(stdout);
  int value = 1;
  while (running) {
    std::printf("CALL value=%d result=%d\n", value,
                prepared_demo::compute(value));
    std::fflush(stdout);
    value = value == 5 ? 1 : value + 1;
    timespec delay{0, 250'000'000};
    while (running && ::nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
  }
  std::puts("TARGET stopped");
}

#include "item.hpp"

#include <csignal>
#include <cstdio>
#include <ctime>
#include <sys/prctl.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#ifndef PR_SET_PTRACER_ANY
#define PR_SET_PTRACER_ANY (~0UL)
#endif

namespace {
volatile std::sig_atomic_t running = 1;
void stop(int) { running = 0; }
}

int main() {
  std::signal(SIGTERM, stop);
  if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) != 0) {
    std::perror("allow tracing of the example target");
    return 1;
  }
  HFW::HfwManager manager;
  auto *base = manager.createItem();
  auto *child = dynamic_cast<HFW::Child *>(base);
  if (!child || static_cast<void *>(base) == static_cast<void *>(child))
    return 1;
  while (running) {
    (void)manager.createItem();
    const timespec delay{0, 250'000'000};
    ::nanosleep(&delay, nullptr);
  }
}

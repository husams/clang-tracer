#include "item.hpp"

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
  if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) != 0) {
    std::perror("allow tracing of the example target");
    return 1;
  }
  std::printf("TARGET pid=%ld\n", static_cast<long>(::getpid()));
  std::fflush(stdout);
  prepared_pointee::HfwManager manager{11, "items manager"};
  int id = 1;
  while (running) {
    auto *item = manager.createItem(id);
    std::printf("CALL id=%d result=%s\n", id,
                item == nullptr ? "null" : item->label.c_str());
    std::fflush(stdout);
    id = id == 0 ? 1 : 0;
    timespec delay{0, 250'000'000};
    while (running && ::nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
  }
  std::puts("TARGET stopped");
}

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

  HFW::HfwManager manager;
  int pointer_kind = 1;
  int reference_kind = 0;
  std::printf("TARGET pid=%ld\n", static_cast<long>(::getpid()));
  std::fflush(stdout);
  while (running) {
    auto *pointer = manager.createItem(pointer_kind);
    auto &reference = manager.referenceItem(reference_kind);
    std::printf("CALL pointer=%s reference=%d\n",
                pointer == nullptr ? "null" : "item", reference.id);
    std::fflush(stdout);

    pointer_kind = pointer_kind == 5 ? 0 : pointer_kind + 1;
    reference_kind = (reference_kind + 1) % 5;
    timespec delay{0, 250'000'000};
    while (running && ::nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
  }
  return 0;
}

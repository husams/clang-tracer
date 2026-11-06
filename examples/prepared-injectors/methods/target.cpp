#include "account.hpp"

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
  prepared_demo::Account account;
  const std::string label = "deposit";
  const auto factor = std::make_shared<int>(2);
  const std::weak_ptr<int> observer = factor;
  while (running) {
    const auto receipt =
        account.apply(label, {2, 5}, {4, 30}, std::make_unique<int>(5),
                      factor, observer,
                      prepared_demo::Choice(std::in_place_index<2>, "argument choice"));
    std::printf("CALL label=%s parts=%zu total=%d\n", receipt.label.c_str(),
                receipt.parts.size(), receipt.total);
    std::fflush(stdout);
    timespec delay{0, 250'000'000};
    while (running && ::nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
  }
}

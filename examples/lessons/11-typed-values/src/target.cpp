#include <cstdio>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>

#include "model.hpp"

namespace app {

__attribute__((noinline)) double submit(const Order& order) {
  return order.quantity * order.unit_price;
}

}  // namespace app

int main() {
#ifdef PR_SET_PTRACER
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) == -1) {
    perror("PR_SET_PTRACER");
    return 1;
  }
#endif
  std::printf("PID %ld | press Enter to call app::submit, Ctrl+C to exit\n",
              static_cast<long>(getpid()));
  std::fflush(stdout);

  app::Order order{7, 3, 19.5};
  pollfd input{STDIN_FILENO, POLLIN, 0};
  while (true) {
    // Check stdin without blocking in a read while the tracer attaches.
    if (poll(&input, 1, 0) > 0) {
      char byte;
      if (read(STDIN_FILENO, &byte, 1) != 1) break;
      if (byte == '\n') {
        std::printf("submit(order %d) = %.2f\n", order.id, app::submit(order));
        std::fflush(stdout);
        ++order.id;
      }
    }
    usleep(20'000);
  }
}

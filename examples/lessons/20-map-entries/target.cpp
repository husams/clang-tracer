// Reprices one stock item each time you press Enter.
#include "stock.hpp"

#include <cstdio>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>

int main() {
#ifdef PR_SET_PTRACER
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) == -1) {
    perror("PR_SET_PTRACER");
    return 1;
  }
#endif
  std::printf("PID %ld | press Enter to reprice item 12, Ctrl+C to exit\n",
              static_cast<long>(getpid()));
  std::fflush(stdout);

  shop::Stock stock;
  stock.items = {{11, {{4.0}}}, {12, {{9.5}}}, {13, {{2.25}}}};
  pollfd input{STDIN_FILENO, POLLIN, 0};
  while (true) {
    // Check stdin without blocking in a read while the tracer attaches.
    if (poll(&input, 1, 0) > 0) {
      char byte;
      if (read(STDIN_FILENO, &byte, 1) != 1) break;
      if (byte == '\n') {
        stock.reprice();
        std::printf("repriced, item 12 costs %.2f\n", stock.items.at(12).pricing.unit);
        std::fflush(stdout);
      }
    }
    usleep(20'000);
  }
}

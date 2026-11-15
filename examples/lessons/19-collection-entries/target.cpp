// Restocks one cart each time you press Enter.
#include "cart.hpp"

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
  std::printf("PID %ld | press Enter to restock the cart, Ctrl+C to exit\n",
              static_cast<long>(getpid()));
  std::fflush(stdout);

  shop::Cart cart;
  cart.lines = {{11, 2}, {12, 1}};
  pollfd input{STDIN_FILENO, POLLIN, 0};
  while (true) {
    // Check stdin without blocking in a read while the tracer attaches.
    if (poll(&input, 1, 0) > 0) {
      char byte;
      if (read(STDIN_FILENO, &byte, 1) != 1) break;
      if (byte == '\n') {
        cart.restock();
        std::printf("restocked, line 12 has quantity %d\n", cart.lines.at(1).quantity);
        std::fflush(stdout);
      }
    }
    usleep(20'000);
  }
}

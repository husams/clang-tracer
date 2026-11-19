// Raises a discount, turns it into a rate, and restores it each time you
// press Enter.
#include "discount.hpp"

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
  std::printf("PID %ld | press Enter to change the discount, Ctrl+C to exit\n",
              static_cast<long>(getpid()));
  std::fflush(stdout);

  shop::Discount discount{{shop::Kind::amount, {100}}};
  pollfd input{STDIN_FILENO, POLLIN, 0};
  while (true) {
    // Check stdin without blocking in a read while the tracer attaches.
    if (poll(&input, 1, 0) > 0) {
      char byte;
      if (read(STDIN_FILENO, &byte, 1) != 1) break;
      if (byte == '\n') {
        discount.raise();
        const long long amount = discount.state.value.amount;
        discount.to_rate();
        std::printf("amount raised to %lld, then switched to rate %.2f\n", amount,
                    discount.state.value.rate);
        std::fflush(stdout);
        discount.reset(amount); // back to an amount for the next press
      }
    }
    usleep(20'000);
  }
}

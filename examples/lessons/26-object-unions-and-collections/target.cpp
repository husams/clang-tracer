#include "cart.hpp"
#include "discount.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>

int main(int argc, char** argv) {
  if (argc != 2 || (std::strcmp(argv[1], "union") && std::strcmp(argv[1], "cart"))) {
    std::fprintf(stderr, "Usage: %s union|cart\n", argv[0]);
    return 2;
  }
  const bool trace_union = std::strcmp(argv[1], "union") == 0;

#ifdef PR_SET_PTRACER
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) == -1) {
    std::perror("PR_SET_PTRACER");
    return 1;
  }
#endif

  std::printf("PID %ld | press Enter for one %s update, Ctrl+C to exit\n",
              static_cast<long>(getpid()), argv[1]);
  std::fflush(stdout);

  shop::Discount discount{{shop::Kind::amount, {100}}};
  shop::Cart cart;
  pollfd input{STDIN_FILENO, POLLIN, 0};
  while (true) {
    // Keep the main thread outside blocking reads during native attachment.
    const int ready = poll(&input, 1, 0);
    if (ready < 0 && errno != EINTR) {
      std::perror("poll");
      return 1;
    }
    if (ready > 0) {
      if (input.revents & POLLIN) {
        char byte;
        if (read(STDIN_FILENO, &byte, 1) != 1)
          return 0;
        if (byte == '\n') {
          if (trace_union) {
            discount.raise();
            const long long amount = discount.state.value.amount;
            discount.to_rate();
            std::printf("amount raised to %lld, then switched to rate %.2f\n",
                        amount, discount.state.value.rate);
            discount.reset(100);
          } else {
            cart.lines = {{11, 2}, {12, 3}};
            cart.rebuild();
            std::printf("rebuilt, %zu lines, line 12 has quantity %d\n",
                        cart.lines.size(), cart.lines.back().quantity);
          }
          std::fflush(stdout);
        }
      } else if (input.revents & (POLLHUP | POLLERR | POLLNVAL)) {
        return 0;
      }
    }
    usleep(20'000);
  }
}

#include <cstdio>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>

#include "types.hpp"

namespace shop {
__attribute__((noinline)) double price(const Order& order, const std::string& coupon,
                                       const std::vector<int>& fees,
                                       const std::map<std::string, int>& stock) {
  double total = order.quantity * order.unit_price;
  for (int fee : fees) total += fee;
  auto found = stock.find(coupon);
  return found == stock.end() ? total : total - found->second;
}
} // namespace shop

int main() {
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) == -1) {
    perror("PR_SET_PTRACER");
    return 1;
  }
  std::printf("PID %ld | press Enter to price an order, Ctrl+C to exit\n",
              static_cast<long>(getpid()));
  std::fflush(stdout);

  shop::Order order{7, 3, 19.5};
  std::vector<int> fees{2, 5};
  std::map<std::string, int> stock{{"SAVE5", 5}, {"SAVE10", 10}};
  pollfd input{STDIN_FILENO, POLLIN, 0};
  while (true) {
    if (poll(&input, 1, 0) > 0) {
      char byte;
      if (read(STDIN_FILENO, &byte, 1) != 1) break;
      if (byte == '\n') {
        std::printf("price = %.2f\n", shop::price(order, "SAVE5", fees, stock));
        std::fflush(stdout);
      }
    }
    usleep(20'000);
  }
}

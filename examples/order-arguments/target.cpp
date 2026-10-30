#include <cerrno>
#include <csignal>
#include <cstdio>
#include <ctime>

#include <unistd.h>

#include "order.hpp"

// The tracer obtains these argument types solely from the binary's DWARF.
double total(Order& order, double fee, const Coupon& coupon, Order sample, int count) {
  volatile double saved_fee = fee;
  const double result = order.quantity * order.unit_price + saved_fee - coupon.amount +
                        sample.quantity * sample.unit_price;
  order.quantity += count;
  sample.quantity += 10; // A local by-value copy: the caller's sample stays unchanged.
  return result;
}

double pointer_total(Order* order, int extra) {
  volatile int saved_extra = extra;
  if (!order) {
    return 0;
  }
  const double result = order->quantity * order->unit_price + saved_extra;
  order->quantity += extra;
  return result;
}

namespace {
volatile std::sig_atomic_t running = 1;

void stop(int) {
  running = 0;
}
} // namespace

int main() {
  std::signal(SIGINT, stop);
  std::signal(SIGTERM, stop);
  std::printf("TARGET pid=%ld\n", static_cast<long>(::getpid()));
  std::fflush(stdout);
  int calls = 0;
  int failures = 0;
  while (running) {
    Order order{3, 12.50};
    const Coupon coupon{2.0, 1};
    Order sample{2, 3.50};
    const double result = total(order, 1.25, coupon, sample, 2);
    const int after_total = order.quantity;
    const double pointer_result = pointer_total(&order, 3);
    if (result != 43.75 || after_total != 5 || order.quantity != 8 || sample.quantity != 2 ||
        pointer_result != 65.5) {
      ++failures;
    }
    std::printf("CALL calls=%d order_before=3 order_after_total=%d "
                "order_after_pointer=%d sample=2 fee=1.25 coupon=2 count=2 "
                "total=%.2f pointer=%.2f failures=%d\n",
                ++calls, after_total, order.quantity, result, pointer_result, failures);
    std::fflush(stdout);
    timespec delay{0, 200'000'000};
    while (running && ::nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
  }
}

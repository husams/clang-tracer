#include "follow_order.hpp"

#include <cstdio>

#include <unistd.h>

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
  while (::access("go.now", F_OK) != 0)
    ::usleep(1000);
  {
    demo::Order order(10);
    order.set_quantity(9);
    demo::apply_fill(order, 2);
  }
  {
    demo::Order order(5);
    demo::apply_fill(order, 0);
  }
  std::puts("DONE");
  while (::access("stop.now", F_OK) != 0)
    ::usleep(1000);
}

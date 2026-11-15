// A small checkout service: every request runs the same four steps.
// Ordinary application code with no dependency on the tracer.
#include <cstdio>
#include <poll.h>
#include <sys/prctl.h>
#include <thread>
#include <unistd.h>

// The pauses make the two requests of a round overlap.
__attribute__((noinline)) int authenticate(int user) {
  usleep(30'000);
  return user % 2; // 1 = accepted
}

__attribute__((noinline)) int price_item(int sku, int quantity) {
  usleep(30'000);
  return sku / 10 * quantity;
}

__attribute__((noinline)) int charge_card(int user, int amount) {
  usleep(30'000);
  return amount > 0 ? 9000 + user : -1; // receipt number
}

// One request. Everything it calls belongs to this request.
__attribute__((noinline)) int handle_checkout(int request, int user) {
  if (!authenticate(user))
    return -1;
  int total = price_item(120, 2) + price_item(350, 1);
  if (request % 2 == 0)
    total += price_item(990, 3);
  return charge_card(user, total);
}

int main() {
#ifdef PR_SET_PTRACER
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) == -1) {
    perror("PR_SET_PTRACER");
    return 1;
  }
#endif
  std::printf("PID %ld | press Enter to serve two overlapping requests, Ctrl+C to exit\n",
              static_cast<long>(getpid()));
  std::fflush(stdout);

  int request = 0;
  pollfd input{STDIN_FILENO, POLLIN, 0};
  while (true) {
    // Check stdin without blocking in a read while the tracer attaches.
    if (poll(&input, 1, 0) > 0) {
      char byte;
      if (read(STDIN_FILENO, &byte, 1) != 1) break;
      if (byte == '\n') {
        const int first = ++request, second = ++request;
        int receipts[2] = {0, 0};
        // Two clients at once: their calls interleave in time.
        std::thread a([&] { receipts[0] = handle_checkout(first, 41); });
        usleep(15'000);
        std::thread b([&] { receipts[1] = handle_checkout(second, 77); });
        a.join();
        b.join();
        std::printf("request %d -> receipt %d | request %d -> receipt %d\n",
                    first, receipts[0], second, receipts[1]);
        std::fflush(stdout);
      }
    }
    usleep(20'000);
  }
}

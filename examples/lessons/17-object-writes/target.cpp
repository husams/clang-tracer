// Opens an account, deposits twice and closes it each time you press Enter.
#include "account.hpp"

#include <cstdio>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>

__attribute__((noinline)) int round_trip(int id) {
  bank::Account account(id, 100);
  bank::deposit(account, 25);
  bank::deposit(account, 40);
  return account.balance;
}

int main() {
#ifdef PR_SET_PTRACER
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) == -1) {
    perror("PR_SET_PTRACER");
    return 1;
  }
#endif
  std::printf("PID %ld | press Enter to open an account and deposit twice, Ctrl+C to exit\n",
              static_cast<long>(getpid()));
  std::fflush(stdout);

  int id = 0;
  pollfd input{STDIN_FILENO, POLLIN, 0};
  while (true) {
    // Check stdin without blocking in a read while the tracer attaches.
    if (poll(&input, 1, 0) > 0) {
      char byte;
      if (read(STDIN_FILENO, &byte, 1) != 1) break;
      if (byte == '\n') {
        ++id;
        std::printf("account %d closed with balance %d\n", id, round_trip(id));
        std::fflush(stdout);
      }
    }
    usleep(20'000);
  }
}

// Opens an account, makes two deposits and closes it each time you press Enter.
#include "account.hpp"

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
  std::printf("PID %ld | press Enter to open an account and make two deposits, Ctrl+C to exit\n",
              static_cast<long>(getpid()));
  std::fflush(stdout);

  int opened = 0;
  pollfd input{STDIN_FILENO, POLLIN, 0};
  while (true) {
    // Check stdin without blocking in a read while the tracer attaches.
    if (poll(&input, 1, 0) > 0) {
      char byte;
      if (read(STDIN_FILENO, &byte, 1) != 1) break;
      if (byte == '\n') {
        auto *account = new shop::Account(100 * ++opened);
        account->deposit(25);
        int balance = account->deposit(50);
        delete account;
        std::printf("account %d closed, balance was %d\n", opened, balance);
        std::fflush(stdout);
      }
    }
    usleep(20'000);
  }
}

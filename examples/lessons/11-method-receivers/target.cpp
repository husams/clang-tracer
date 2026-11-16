#include <cstdio>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>

class Account {
public:
  int balance = 100;
  mutable int operations = 0;

  __attribute__((noinline)) int deposit(int amount) {
    ++operations;
    balance += amount;
    return balance;
  }

private:
  int audit_code = 99; // private fields stay out of the probe's reach
};

int main() {
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) == -1) {
    perror("PR_SET_PTRACER");
    return 1;
  }
  std::printf("PID %ld | press Enter to deposit 25, Ctrl+C to exit\n",
              static_cast<long>(getpid()));
  std::fflush(stdout);

  Account account;
  pollfd input{STDIN_FILENO, POLLIN, 0};
  while (true) {
    if (poll(&input, 1, 0) > 0) {
      char byte;
      if (read(STDIN_FILENO, &byte, 1) != 1) break;
      if (byte == '\n') {
        std::printf("balance = %d\n", account.deposit(25));
        std::fflush(stdout);
      }
    }
    usleep(20'000);
  }
}

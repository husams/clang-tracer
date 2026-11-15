#include "counter.hpp"

#include <cerrno>
#include <cstdio>
#include <poll.h>
#include <unistd.h>

int main() {
  std::setvbuf(stdin, nullptr, _IONBF, 0);
  std::printf("READY pid=%ld\n", static_cast<long>(getpid()));
  std::puts("Press Enter to add 2, enter 0 to add 0, or enter q to quit.");
  std::fflush(stdout);

  pollfd terminal{STDIN_FILENO, POLLIN, 0};
  while (true) {
    // Give the tracer attachment opportunities while the terminal is idle.
    const int ready = poll(&terminal, 1, 0);
    if (ready < 0 && errno != EINTR) {
      std::perror("poll");
      return 1;
    }
    if (ready <= 0) {
      usleep(20'000);
      continue;
    }
    char input[16];
    if (!(terminal.revents & POLLIN) || !std::fgets(input, sizeof(input), stdin) || input[0] == 'q')
      break;
    {
      lesson::Counter counter(10);
      const int result = counter.add(input[0] == '0' ? 0 : 2);
      std::printf("object=%p value=%d\n", static_cast<void*>(&counter), result);
    }
    std::fflush(stdout);
  }
}

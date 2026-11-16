// Only the library has debug information that the probe will use; the target
// itself needs no header from it for the tracer.
#include <cstdio>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>

#include "demo.hpp"

int main() {
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) == -1) {
    perror("PR_SET_PTRACER");
    return 1;
  }
  std::printf("PID %ld | press Enter to score a packet, Ctrl+C to exit\n",
              static_cast<long>(getpid()));
  std::fflush(stdout);

  demo::Packet packet{4, 2};
  pollfd input{STDIN_FILENO, POLLIN, 0};
  while (true) {
    if (poll(&input, 1, 0) > 0) {
      char byte;
      if (read(STDIN_FILENO, &byte, 1) != 1) break;
      if (byte == '\n') {
        std::printf("score = %d\n", demo::score(packet));
        std::fflush(stdout);
      }
    }
    usleep(20'000);
  }
}

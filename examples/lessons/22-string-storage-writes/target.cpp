// Marks a label and bumps its code each time you press Enter.
#include "label.hpp"

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
  std::printf("PID %ld | press Enter to mark the label, Ctrl+C to exit\n",
              static_cast<long>(getpid()));
  std::fflush(stdout);

  shop::Label label("sale", 7);
  pollfd input{STDIN_FILENO, POLLIN, 0};
  while (true) {
    // Check stdin without blocking in a read while the tracer attaches.
    if (poll(&input, 1, 0) > 0) {
      char byte;
      if (read(STDIN_FILENO, &byte, 1) != 1) break;
      if (byte == '\n') {
        label.mark();
        label.bump();
        std::printf("label is \"%s\" with code %d\n", label.text().c_str(), label.code());
        std::fflush(stdout);
      }
    }
    usleep(20'000);
  }
}

// Creates one item of each version and encodes it each time you press Enter.
#include "item_factory.hpp"

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
  std::printf("PID %ld | press Enter to create three items, Ctrl+C to exit\n",
              static_cast<long>(getpid()));
  std::fflush(stdout);

  ItemFactory factory;
  pollfd input{STDIN_FILENO, POLLIN, 0};
  while (true) {
    // Check stdin without blocking in a read while the tracer attaches.
    if (poll(&input, 1, 0) > 0) {
      char byte;
      if (read(STDIN_FILENO, &byte, 1) != 1) break;
      if (byte == '\n') {
        for (const char* version : {"v1", "v2", "v3"}) {
          Item* item = factory.create(version);
          std::printf("%s -> %s\n", item->description().c_str(),
                      item->encode("text").c_str());
          delete item;
        }
        std::fflush(stdout);
      }
    }
    usleep(20'000);
  }
}

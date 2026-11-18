#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

#include "meter.hpp"
#include "types.hpp"

std::int64_t receiver_header::Meter::charge(std::int64_t cents) {
  if (!frozen) {
    total_cents += cents;
    ++charges;
  }
  return total_cents;
}

receiver_header::Quote receiver_header::quote(const Request &request) {
  return {request.cents + 7};
}

int main() {
  receiver_header::Meter meter;
  std::puts("ready");
  std::fflush(stdout);
  // Keep an idle injection point while waiting for the next test call.
  ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
  for (;;) {
    char command;
    const auto count = ::read(STDIN_FILENO, &command, 1);
    if (count == 0)
      break;
    if (count < 0) {
      if (errno != EAGAIN && errno != EINTR)
        return 1;
      ::usleep(10000);
      continue;
    }
    if (command == 'c') {
      const auto result = meter.charge(40);
      std::printf("charged total=%ld charges=%u result=%ld\n",
                  static_cast<long>(meter.total_cents), meter.charges,
                  static_cast<long>(result));
    } else if (command == 'q') {
      const auto result = receiver_header::quote({40});
      std::printf("quoted cents=%ld\n", result.cents);
    } else {
      continue;
    }
    std::fflush(stdout);
  }
}

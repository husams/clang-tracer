#include "record.hpp"

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

int main() {
  if (::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK))
    return 2;
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  origin_test::Aggregate aggregate{72};
  std::puts("READY");
  for (;;) {
    char command;
    const auto size = ::read(STDIN_FILENO, &command, 1);
    if (size == 0 || (size == 1 && command == 'q'))
      return 0;
    if (size < 0) {
      if (errno != EAGAIN && errno != EINTR)
        return 3;
      ::usleep(1000);
      continue;
    }
    if (command == 'i') {
      aggregate.value = 72;
      origin_test::write_inline(aggregate);
      std::printf("inline=%d\n", aggregate.value);
    } else if (command == 'o') {
      origin_test::Tracked tracked(73);
      opaque_write(tracked);
      std::printf("opaque=%d\n", tracked.value);
    }
  }
}

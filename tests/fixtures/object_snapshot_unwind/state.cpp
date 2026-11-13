#include "state.hpp"

#include <cstdio>
#include <unistd.h>

__attribute__((noinline)) void snapshot_unwind::State::fail() {
  value_ = 11;
  throw 42;
}

int main(int argc, char **argv) {
  snapshot_unwind::State state;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc > 1) {
    std::printf("REEXEC pid=%ld\n", static_cast<long>(::getpid()));
    while (::access("stop.now", F_OK))
      ::usleep(1000);
    return 0;
  }
  std::puts("READY");
  while (::access("throw.now", F_OK))
    ::usleep(1000);
  try {
    state.fail();
    return 2;
  } catch (int value) {
    std::printf("CAUGHT exception=%d field=%d\n", value, state.value());
  }
  while (::access("stop.now", F_OK)) {
    if (!::access("exec.now", F_OK)) {
      ::execl(argv[0], argv[0], "replaced", static_cast<char *>(nullptr));
      return 3;
    }
    ::usleep(1000);
  }
  return 0;
}

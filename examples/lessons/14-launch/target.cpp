#include <tracer/launch.hpp>

#include <cstdio>
#include <unistd.h>

__attribute__((noinline)) int add(int a, int b) { return a + b; }

int main() {
  // The REPL's %launch holds the program here until %resume, so probes can be
  // installed before any traced code runs. On an ordinary run it returns at once.
  if (!tracer::launch::wait_for_resume())
    return 125;

  std::printf("PID %ld | calling add once a second\n", static_cast<long>(getpid()));
  std::fflush(stdout);

  // %launch gives the program /dev/null as standard input, so this target
  // calls add on a timer instead of waiting for Enter.
  while (true) {
    std::printf("add(20, 22) = %d\n", add(20, 22));
    std::fflush(stdout);
    sleep(1);
  }
}

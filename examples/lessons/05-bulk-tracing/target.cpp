#include <cstdio>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>

__attribute__((noinline)) int add(int a, int b) { return a + b; }
__attribute__((noinline)) int multiply(int a, int b) { return a * b; }

int main() {
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) == -1) {
    perror("PR_SET_PTRACER");
    return 1;
  }
  std::printf("PID %ld | press Enter to run one round, Ctrl+C to exit\n",
              static_cast<long>(getpid()));
  std::fflush(stdout);

  pollfd input{STDIN_FILENO, POLLIN, 0};
  while (true) {
    // Poll stdin so the target never blocks in read() while the tracer attaches.
    if (poll(&input, 1, 0) > 0) {
      char byte;
      if (read(STDIN_FILENO, &byte, 1) != 1) break;
      if (byte == '\n') {
        std::printf("add(20, 22) = %d\n", add(20, 22));
        std::printf("multiply(6, 7) = %d\n", multiply(6, 7));
        std::fflush(stdout);
      }
    }
    usleep(20'000);
  }
}

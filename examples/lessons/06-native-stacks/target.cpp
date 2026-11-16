#include <cstdio>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>

__attribute__((noinline)) int leaf(int value) { return value * 2; }
__attribute__((noinline)) int middle(int value) { return leaf(value + 1) + 1; }
__attribute__((noinline)) int outer(int value) { return middle(value) + 1; }

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
        std::printf("outer(20) = %d\n", outer(20));
        std::fflush(stdout);
      }
    }
    usleep(20'000);
  }
}

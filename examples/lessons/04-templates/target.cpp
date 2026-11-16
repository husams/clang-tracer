#include <cstdio>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>

template <class T>
__attribute__((noinline)) T echo(T value) {
  volatile T saved = value;
  return saved;
}

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
        std::printf("echo<int>(42) = %d\n", echo<int>(42));
        std::printf("echo<double>(1.5) = %g\n", echo<double>(1.5));
        std::fflush(stdout);
      }
    }
    usleep(20'000);
  }
}

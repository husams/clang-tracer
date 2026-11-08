#define _POSIX_C_SOURCE 200809L
#include "tracer/launch_gate.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

__attribute__((constructor)) static void tracer_launch_checkpoint(void) {
  const char *value = getenv(TRACER_LAUNCH_FD);
  if (!value)
    return;
  char *end;
  errno = 0;
  const long fd = strtol(value, &end, 10);
  const char *previous = getenv(TRACER_LAUNCH_PRELOAD_RESTORE);
  if (errno || end == value || *end || fd < 3 || fd > INT_MAX || !previous ||
      (*previous ? setenv("LD_PRELOAD", previous, 1) : unsetenv("LD_PRELOAD")) != 0 ||
      unsetenv(TRACER_LAUNCH_PRELOAD_RESTORE) != 0 || unsetenv(TRACER_LAUNCH_FD) != 0 ||
      fcntl((int)fd, F_SETFD, FD_CLOEXEC) == -1)
    _exit(126);
  const char ready = TRACER_LAUNCH_READY;
  ssize_t count;
  do {
    count = send((int)fd, &ready, 1, MSG_NOSIGNAL);
  } while (count < 0 && errno == EINTR);
  if (count != 1)
    _exit(126);
  for (;;) {
    char command = 0;
    count = recv((int)fd, &command, 1, MSG_DONTWAIT | MSG_TRUNC);
    if (count == 1 && command == TRACER_LAUNCH_RESUME) {
      close((int)fd);
      return;
    }
    if (count < 0 && errno == EINTR)
      continue;
    if (count >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
      _exit(126);
    const struct timespec interval = {0, 10 * 1000 * 1000};
    nanosleep(&interval, NULL);
  }
}

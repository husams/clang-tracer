#define _POSIX_C_SOURCE 200809L
#include "tracer/launch_gate.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif

static void checkpoint(int fd, int descendant) {
  const char ready = TRACER_LAUNCH_READY;
  ssize_t count;
  do {
    count = send(fd, &ready, 1, MSG_NOSIGNAL);
  } while (count < 0 && errno == EINTR);
  if (count != 1)
    _exit(126);
  struct timespec start;
  if (descendant && clock_gettime(CLOCK_MONOTONIC, &start) != 0)
    _exit(126);
  for (;;) {
    char command = 0;
    count = recv(fd, &command, 1, MSG_DONTWAIT | MSG_TRUNC);
    if (count == 1 && command == TRACER_LAUNCH_RESUME) {
      close(fd);
      return;
    }
    if (count < 0 && errno == EINTR)
      continue;
    if (count >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
      _exit(126);
    struct timespec now;
    if (descendant && (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec - start.tv_sec >= 60))
      _exit(126);
    const struct timespec interval = {0, 10 * 1000 * 1000};
    nanosleep(&interval, NULL);
  }
}

static void child_checkpoint(const char *path) {
  const char *server = getenv(TRACER_CHILD_TRACER);
  struct sockaddr_un address = {0};
  if (!server || strlen(path) >= sizeof(address.sun_path))
    _exit(126);
  char *end;
  errno = 0;
  const long tracer = strtol(server, &end, 10);
  // -1 means the launching REPL does not know the server PID: accept any
  // tracer, since (unsigned long)-1 is PR_SET_PTRACER_ANY.
  if (errno || end == server || *end || (tracer <= 0 && tracer != -1) || tracer > INT_MAX)
    _exit(126);
  if (prctl(PR_SET_PTRACER, (unsigned long)tracer, 0UL, 0UL, 0UL) != 0 && errno != EINVAL)
    _exit(126);
  address.sun_family = AF_UNIX;
  strcpy(address.sun_path, path);
  const int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
  if (fd < 0 || connect(fd, (const struct sockaddr *)&address, sizeof(address)) != 0)
    _exit(126);
  checkpoint(fd, 1);
}

__attribute__((constructor)) static void tracer_launch_checkpoint(void) {
  const char *value = getenv(TRACER_LAUNCH_FD);
  const char *children = getenv(TRACER_CHILD_GATE);
  if (!value) {
    if (children)
      child_checkpoint(children);
    return;
  }
  char *end;
  errno = 0;
  const long fd = strtol(value, &end, 10);
  const char *previous = getenv(TRACER_LAUNCH_PRELOAD_RESTORE);
  if (errno || end == value || *end || fd < 3 || fd > INT_MAX || !previous ||
      (!children && (*previous ? setenv("LD_PRELOAD", previous, 1) : unsetenv("LD_PRELOAD")) != 0) ||
      unsetenv(TRACER_LAUNCH_PRELOAD_RESTORE) != 0 || unsetenv(TRACER_LAUNCH_FD) != 0 ||
      fcntl((int)fd, F_SETFD, FD_CLOEXEC) == -1)
    _exit(126);
  checkpoint((int)fd, 0);
}

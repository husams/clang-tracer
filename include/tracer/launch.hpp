#pragma once

#if defined(__linux__) && defined(__x86_64__)

#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

namespace tracer::launch {
inline constexpr char gate_environment[] = "CLANG_TRACER_LAUNCH_FD";
inline constexpr char ready = 'R';
inline constexpr char resume = 'G';

// Call at the desired startup boundary, before starting other application
// threads. An ordinary launch without a tracer proceeds immediately.
inline bool wait_for_resume() noexcept {
  const char *value = std::getenv(gate_environment);
  if (!value)
    return true;
  const char *end = value + std::strlen(value);
  int fd = -1;
  const auto parsed = std::from_chars(value, end, fd);
  if (parsed.ec != std::errc{} || parsed.ptr != end || fd < 3)
    return false;
  struct Close {
    int fd;
    ~Close() { ::close(fd); }
  } close{fd};
  if (::fcntl(fd, F_SETFD, FD_CLOEXEC) == -1 || ::unsetenv(gate_environment) != 0)
    return false;
  ssize_t sent;
  do {
    sent = ::send(fd, &ready, 1, MSG_NOSIGNAL);
  } while (sent < 0 && errno == EINTR);
  if (sent != 1)
    return false;
  for (;;) {
    char command = 0;
    const auto count = ::recv(fd, &command, 1, MSG_DONTWAIT | MSG_TRUNC);
    if (count >= 0)
      return count == 1 && command == resume;
    if (errno == EINTR)
      continue;
    if (errno != EAGAIN && errno != EWOULDBLOCK)
      return false;
    // The native injector admits threads blocked in nanosleep.
    const timespec interval{0, 10'000'000};
    ::nanosleep(&interval, nullptr);
  }
}
} // namespace tracer::launch
#endif

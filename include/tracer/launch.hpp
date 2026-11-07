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

#include "launch_call.hpp"

namespace tracer::launch {
inline constexpr char gate_environment[] = "CLANG_TRACER_LAUNCH_FD";
inline constexpr char ready = 'R';
inline constexpr char resume = 'G';

// Call at the desired startup boundary, before starting other application
// threads. An ordinary launch without a tracer proceeds immediately.
template <class... Calls>
bool wait_for_resume(const Calls &... calls) {
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
  constexpr char ready_message = sizeof...(Calls) ? detail::call_ready : ready;
  do {
    sent = ::send(fd, &ready_message, 1, MSG_NOSIGNAL);
  } while (sent < 0 && errno == EINTR);
  if (sent != 1)
    return false;
  for (;;) {
    detail::CallRequest request;
    const auto count = ::recv(fd, &request, sizeof(request), MSG_DONTWAIT | MSG_TRUNC);
    if (count == 1)
      return *reinterpret_cast<const char *>(&request) == resume;
    if (count >= 0) {
      if constexpr (sizeof...(Calls) != 0) {
        if (count == static_cast<ssize_t>(sizeof(request)) &&
            request.version == detail::call_protocol &&
            request.count <= detail::max_call_arguments && request.name[0] &&
            !request.name[sizeof(request.name) - 1]) {
          for (const auto &argument : request.arguments)
            if (argument[sizeof(argument) - 1])
              return false;
          const auto reply = detail::dispatch_call(request, calls...);
          do {
            sent = ::send(fd, &reply, sizeof(reply), MSG_NOSIGNAL);
          } while (sent < 0 && errno == EINTR);
          if (sent == static_cast<ssize_t>(sizeof(reply)))
            continue;
        }
      }
      return false;
    }
    if (errno == EINTR)
      continue;
    if (errno != EAGAIN && errno != EWOULDBLOCK)
      return false;
    // The native injector admits threads blocked in nanosleep.
    const timespec interval{0, 10'000'000};
    ::nanosleep(&interval, nullptr);
  }
}

inline bool wait_for_resume() noexcept { return wait_for_resume<>(); }
} // namespace tracer::launch
#endif

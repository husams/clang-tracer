#pragma once

#include <atomic>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdint>
#include <linux/futex.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace tracer::injector {

// Linux PTRACE_EVENT_STOP is absent from older SDK headers.
inline constexpr unsigned ptrace_event_stop = 128;

// Ptrace stops send SIGCHLD to the tracer process. A shared sequence lets
// each worker wait for its own waitpid results without consuming another
// worker's notification. Only the tracer's executables use this helper.
class PtraceWait {
  inline static std::atomic<std::uint32_t> sequence_{0};
  static_assert(decltype(sequence_)::is_always_lock_free);
  static_assert(sizeof(sequence_) == sizeof(std::uint32_t));

  static void signal_received(int) noexcept {
    const int saved = errno;
    wake();
    errno = saved;
  }

public:
  static bool enable() noexcept {
    static const bool installed = [] {
      struct sigaction current{}, action{};
      if (::sigaction(SIGCHLD, nullptr, &current) != 0 || current.sa_handler != SIG_DFL ||
          (current.sa_flags & (SA_NOCLDSTOP | SA_NOCLDWAIT)))
        return false;
      action.sa_handler = signal_received;
      action.sa_flags = SA_RESTART;
      ::sigemptyset(&action.sa_mask);
      // A filtered kernel may deny futex operations in the tracer.
      if (::syscall(SYS_futex, &sequence_, FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0) < 0)
        return false;
      return ::sigaction(SIGCHLD, &action, nullptr) == 0;
    }();
    if (!installed)
      return false;
    sigset_t mask;
    ::sigemptyset(&mask);
    ::sigaddset(&mask, SIGCHLD);
    return ::pthread_sigmask(SIG_UNBLOCK, &mask, nullptr) == 0;
  }

  static void wake() noexcept {
    sequence_.fetch_add(1, std::memory_order_relaxed);
    (void)::syscall(SYS_futex, &sequence_, FUTEX_WAKE_PRIVATE, INT_MAX, nullptr, nullptr, 0);
  }

  static std::uint32_t generation() noexcept { return sequence_.load(std::memory_order_relaxed); }

  static bool wait(std::uint32_t previous, std::chrono::steady_clock::time_point deadline) noexcept {
    using namespace std::chrono;
    timespec limit{};
    const bool bounded = deadline != steady_clock::time_point::max();
    if (bounded) {
      const auto when = deadline.time_since_epoch();
      const auto seconds_part = duration_cast<seconds>(when);
      limit.tv_sec = seconds_part.count();
      limit.tv_nsec = duration_cast<nanoseconds>(when - seconds_part).count();
    }
    return ::syscall(SYS_futex, &sequence_, FUTEX_WAIT_BITSET_PRIVATE, previous,
                     bounded ? &limit : nullptr, nullptr, FUTEX_BITSET_MATCH_ANY) == 0 ||
           errno == EAGAIN || errno == EINTR || errno == ETIMEDOUT;
  }
};

} // namespace tracer::injector

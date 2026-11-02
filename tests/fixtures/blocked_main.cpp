// Target whose main thread blocks in a non-sleep syscall while workers either
// sleep (and call add()) or block forever on an untimed futex.
//
// Usage: blocked-main-fixture <main-wait> [--blocked-workers]
//   main-wait: futex | epoll | poll | read | accept | pause | sigwait

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <poll.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

constexpr int kWorkerCount = 4;

std::mutex blocked_mutex;
std::condition_variable blocked_condition;
volatile int sink = 0;

[[noreturn]] void fail(const char *what) {
  std::fprintf(stderr, "blocked-main-fixture: %s: %s\n", what,
               std::strerror(errno));
  std::exit(2);
}

// The predicate never becomes true, so every wait is an untimed futex wait.
[[noreturn]] void wait_forever_on_condition() {
  std::unique_lock lock(blocked_mutex);
  for (;;) {
    blocked_condition.wait(lock, [] { return false; });
  }
}

} // namespace

__attribute__((visibility("default"), noinline)) int add(int left, int right) {
  return left + right;
}

namespace {

void active_worker() {
  for (int i = 0;; ++i) {
    sink = sink + add(i, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void blocked_worker() { wait_forever_on_condition(); }

[[noreturn]] void main_epoll() {
  const int epoll_fd = ::epoll_create1(0);
  if (epoll_fd < 0) {
    fail("epoll_create1");
  }
  epoll_event event{};
  for (;;) {
    const int result = ::epoll_wait(epoll_fd, &event, 1, -1);
    if (result < 0 && errno != EINTR) {
      fail("epoll_wait");
    }
  }
}

[[noreturn]] void main_poll() {
  int pipe_fds[2];
  if (::pipe(pipe_fds) != 0) {
    fail("pipe");
  }
  pollfd descriptor{pipe_fds[0], POLLIN, 0};
  for (;;) {
    const int result = ::poll(&descriptor, 1, -1);
    if (result < 0 && errno != EINTR) {
      fail("poll");
    }
  }
}

[[noreturn]] void main_read() {
  int pipe_fds[2];
  if (::pipe(pipe_fds) != 0) {
    fail("pipe");
  }
  char byte = 0;
  for (;;) {
    const ssize_t result = ::read(pipe_fds[0], &byte, 1);
    if (result < 0 && errno != EINTR) {
      fail("read");
    }
  }
}

int make_listener() {
  const int listener = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (listener < 0) {
    fail("socket");
  }
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  const std::string name = "clang-tracer-blocked-main-" +
                           std::to_string(static_cast<long>(::getpid()));
  // A leading NUL byte selects the abstract namespace (no filesystem entry).
  std::memcpy(address.sun_path + 1, name.data(), name.size());
  const socklen_t length = static_cast<socklen_t>(
      offsetof(sockaddr_un, sun_path) + 1 + name.size());
  if (::bind(listener, reinterpret_cast<sockaddr *>(&address), length) != 0) {
    fail("bind");
  }
  if (::listen(listener, 1) != 0) {
    fail("listen");
  }
  return listener;
}

[[noreturn]] void main_accept(int listener) {
  for (;;) {
    const int client = ::accept(listener, nullptr, nullptr);
    if (client >= 0) {
      ::close(client);
    } else if (errno != EINTR) {
      fail("accept");
    }
  }
}

[[noreturn]] void main_pause() {
  for (;;) {
    ::pause();
  }
}

[[noreturn]] void main_sigwait(const sigset_t &set) {
  for (;;) {
    int signal_number = 0;
    const int result = ::sigwait(&set, &signal_number);
    if (result != 0 && result != EINTR) {
      errno = result;
      fail("sigwait");
    }
  }
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) {
    std::fprintf(stderr,
                 "usage: %s futex|epoll|poll|read|accept|pause|sigwait "
                 "[--blocked-workers]\n",
                 argv[0]);
    return 2;
  }
  const std::string wait = argv[1];
  bool blocked_workers = false;
  if (argc == 3) {
    if (std::string(argv[2]) != "--blocked-workers") {
      std::fprintf(stderr, "unknown option: %s\n", argv[2]);
      return 2;
    }
    blocked_workers = true;
  }

  // Block SIGUSR1 before creating workers so that they inherit the mask and
  // sigwait in main is the only consumer.
  sigset_t wait_set;
  sigemptyset(&wait_set);
  if (wait == "sigwait") {
    sigaddset(&wait_set, SIGUSR1);
    if (::pthread_sigmask(SIG_BLOCK, &wait_set, nullptr) != 0) {
      fail("pthread_sigmask");
    }
  }

  int listener = -1;
  if (wait == "accept") {
    listener = make_listener();
  }

  std::vector<std::thread> workers;
  for (int i = 0; i < kWorkerCount; ++i) {
    workers.emplace_back(blocked_workers ? blocked_worker : active_worker);
  }

  std::printf("TARGET pid=%ld\n", static_cast<long>(::getpid()));
  std::fflush(stdout);

  if (wait == "futex") {
    wait_forever_on_condition();
  } else if (wait == "epoll") {
    main_epoll();
  } else if (wait == "poll") {
    main_poll();
  } else if (wait == "read") {
    main_read();
  } else if (wait == "accept") {
    main_accept(listener);
  } else if (wait == "pause") {
    main_pause();
  } else if (wait == "sigwait") {
    main_sigwait(wait_set);
  }
  std::fprintf(stderr, "unknown main-wait: %s\n", wait.c_str());
  return 2;
}

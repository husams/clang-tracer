#include <cerrno>
#include <chrono>
#include <csignal>
#include <sstream>
#include <stdexcept>

#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "check_process.hpp"
#include "controller.hpp"

extern char** environ;

namespace tracer_repl {
namespace {
constexpr std::size_t request_limit = 64U * 1024U * 1024U;

struct File {
  int fd = -1;

  ~File() {
    if (fd >= 0) {
      ::close(fd);
    }
  }
};
} // namespace

struct CppCheckSession::Impl {
  std::string helper;
  std::vector<std::string> arguments;
  int fd = -1;
  pid_t pid = -1;
  std::chrono::steady_clock::time_point deadline;

  ~Impl() {
    if (pid > 0) {
      // posix_spawn creates a private group, also containing speculative forks.
      ::kill(-pid, SIGKILL);
      while (::waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {
      }
    }
    if (fd >= 0) {
      ::close(fd);
    }
  }

  void start() {
    int sockets[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
      throw std::runtime_error("cannot create C++ checker session");
    }
    fd = sockets[0];
    File child{sockets[1]};
    if (fd < STDERR_FILENO + 1) {
      const int promoted = ::fcntl(fd, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
      if (promoted < 0) {
        throw std::runtime_error("cannot configure C++ checker descriptors");
      }
      ::close(fd);
      fd = promoted;
    }
    if (child.fd < STDERR_FILENO + 1) {
      const int promoted = ::fcntl(child.fd, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
      if (promoted < 0) {
        throw std::runtime_error("cannot configure C++ checker descriptors");
      }
      ::close(child.fd);
      child.fd = promoted;
    }
    const int status_flags = ::fcntl(fd, F_GETFL);
    if (::fcntl(fd, F_SETFD, FD_CLOEXEC) < 0 || ::fcntl(child.fd, F_SETFD, FD_CLOEXEC) < 0 ||
        status_flags < 0 || ::fcntl(fd, F_SETFL, status_flags | O_NONBLOCK) < 0) {
      throw std::runtime_error("cannot configure C++ checker session");
    }
#ifdef SO_NOSIGPIPE
    const int enabled = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)) < 0) {
      throw std::runtime_error("cannot configure C++ checker signals");
    }
#endif
    std::vector<std::string> args{helper, "serve", "0"};
    args.insert(args.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv;
    for (auto& arg : args) {
      argv.push_back(arg.data());
    }
    argv.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    if (::posix_spawn_file_actions_init(&actions) != 0) {
      throw std::runtime_error("cannot initialize C++ checker actions");
    }
    posix_spawnattr_t attributes;
    if (::posix_spawnattr_init(&attributes) != 0) {
      ::posix_spawn_file_actions_destroy(&actions);
      throw std::runtime_error("cannot initialize C++ checker attributes");
    }
    const int setup =
        ::posix_spawn_file_actions_adddup2(&actions, child.fd, STDIN_FILENO) |
        ::posix_spawn_file_actions_adddup2(&actions, child.fd, STDOUT_FILENO) |
        ::posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0) |
        ::posix_spawn_file_actions_addclose(&actions, fd) |
        ::posix_spawnattr_setpgroup(&attributes, 0) |
        ::posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    const int result =
        setup ? setup
              : ::posix_spawn(&pid, helper.c_str(), &actions, &attributes, argv.data(), environ);
    ::posix_spawnattr_destroy(&attributes);
    ::posix_spawn_file_actions_destroy(&actions);
    if (result != 0) {
      pid = -1;
      throw std::runtime_error("cannot start C++ checker session");
    }
  }

  void ready(short events) {
    if (interrupted.load(std::memory_order_relaxed)) {
      throw std::runtime_error("C++ check cancelled; previous variables retained");
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      throw std::runtime_error("C++ check timed out; previous variables retained");
    }
    struct pollfd event{fd, events, 0};
    if (::poll(&event, 1, 20) < 0 && errno != EINTR) {
      throw std::runtime_error("cannot poll C++ checker session");
    }
  }

  void write(const std::string& text) {
    std::size_t offset = 0;
    while (offset < text.size()) {
      ready(POLLOUT);
      int flags = 0;
#ifdef MSG_NOSIGNAL
      flags = MSG_NOSIGNAL;
#endif
      const auto n = ::send(fd, text.data() + offset, text.size() - offset, flags);
      if (n < 0 && (errno == EAGAIN || errno == EINTR)) {
        continue;
      }
      if (n <= 0) {
        throw std::runtime_error("C++ checker session stopped; previous variables retained");
      }
      offset += static_cast<std::size_t>(n);
    }
  }

  std::string read(std::size_t size) {
    std::string text(size, '\0');
    std::size_t offset = 0;
    while (offset < size) {
      ready(POLLIN);
      const auto n = ::read(fd, text.data() + offset, size - offset);
      if (n < 0 && (errno == EAGAIN || errno == EINTR)) {
        continue;
      }
      if (n <= 0) {
        throw std::runtime_error("C++ checker session stopped; previous variables retained");
      }
      offset += static_cast<std::size_t>(n);
    }
    return text;
  }

  AnalysisReply request(const std::string& history, const std::string& source) {
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    write(std::to_string(history.size()) + " " + std::to_string(source.size()) + "\n");
    write(history);
    write(source);
    std::string header;
    for (;;) {
      const auto ch = read(1).front();
      if (ch == '\n') {
        break;
      }
      if (header.size() >= 128) {
        throw std::runtime_error("invalid C++ checker reply");
      }
      header += ch;
    }
    AnalysisReply reply{-1, {}};
    std::size_t size = 0;
    std::string extra;
    std::istringstream input(header);
    if (!(input >> reply.status >> size) || (input >> extra) || reply.status < 0 ||
        reply.status > 2 || size > 65536) {
      throw std::runtime_error("invalid C++ checker reply; previous variables retained");
    }
    reply.output = read(size);
    return reply;
  }
};

CppCheckSession::CppCheckSession() = default;
CppCheckSession::~CppCheckSession() = default;

AnalysisReply CppCheckSession::check(const std::string& helper,
                                     const std::vector<std::string>& arguments,
                                     const std::string& history, const std::string& source) {
  // Preserve the existing large-history behavior without unbounded IPC frames.
  if (history.size() > request_limit || source.size() > request_limit - history.size()) {
    impl_.reset();
    return run_cpp_analysis(helper, "check", arguments, history, source);
  }
  try {
    if (!impl_ || impl_->helper != helper || impl_->arguments != arguments) {
      impl_.reset();
      impl_ = std::make_unique<Impl>();
      impl_->helper = helper;
      impl_->arguments = arguments;
      impl_->start();
    }
    return impl_->request(history, source);
  } catch (...) {
    impl_.reset();
    throw;
  }
}
} // namespace tracer_repl

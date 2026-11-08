#include "check_process.hpp"
#include "controller.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

namespace tracer_repl {
namespace {
struct File {
  int fd = -1;
  ~File() {
    if (fd >= 0)
      ::close(fd);
  }
};

struct Child {
  pid_t pid = -1;
  ~Child() {
    if (pid > 0) {
      // This is an isolated compiler helper, never the tracing server/target.
      ::kill(pid, SIGKILL);
      while (::waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {
      }
    }
  }
};
} // namespace

AnalysisReply run_cpp_analysis(const std::string &helper, const char *mode,
                               const std::vector<std::string> &arguments,
                               const std::string &history,
                               const std::string &source) {
  // A private, immediately unlinked file avoids pipe deadlocks on large inputs.
  char name[] = "/tmp/clang-tracer-check.XXXXXX";
  File input{::mkstemp(name)};
  if (input.fd < 0)
    throw std::runtime_error("cannot create C++ checker input");
  ::unlink(name);
  ::fcntl(input.fd, F_SETFD, FD_CLOEXEC);
  const auto content = history + source;
  std::size_t offset = 0;
  while (offset < content.size()) {
    const auto count =
        ::write(input.fd, content.data() + offset, content.size() - offset);
    if (count < 0 && errno == EINTR)
      continue;
    if (count <= 0)
      throw std::runtime_error("cannot write C++ checker input");
    offset += static_cast<std::size_t>(count);
  }
  if (::lseek(input.fd, 0, SEEK_SET) < 0)
    throw std::runtime_error("cannot rewind C++ checker input");
  int descriptors[2];
  if (::pipe(descriptors) != 0)
    throw std::runtime_error("cannot create C++ checker output");
  File output{descriptors[0]}, writer{descriptors[1]};
  ::fcntl(output.fd, F_SETFD, FD_CLOEXEC);
  ::fcntl(writer.fd, F_SETFD, FD_CLOEXEC);
  ::fcntl(output.fd, F_SETFL, O_NONBLOCK);
  std::vector<std::string> args{helper, mode, std::to_string(history.size())};
  args.insert(args.end(), arguments.begin(), arguments.end());
  std::vector<char *> argv;
  for (auto &arg : args)
    argv.push_back(arg.data());
  argv.push_back(nullptr);
  posix_spawn_file_actions_t actions;
  if (::posix_spawn_file_actions_init(&actions) != 0)
    throw std::runtime_error("cannot initialize C++ checker");
  const int setup =
      ::posix_spawn_file_actions_adddup2(&actions, input.fd, STDIN_FILENO) |
      ::posix_spawn_file_actions_adddup2(&actions, writer.fd, STDOUT_FILENO) |
      ::posix_spawn_file_actions_adddup2(&actions, writer.fd, STDERR_FILENO);
  Child child;
  const int started = setup
                          ? setup
                          : ::posix_spawn(&child.pid, helper.c_str(), &actions,
                                          nullptr, argv.data(), environ);
  ::posix_spawn_file_actions_destroy(&actions);
  if (started != 0) {
    child.pid = -1;
    throw std::runtime_error("cannot start C++ checker: " +
                             std::string(std::strerror(started)));
  }
  ::close(writer.fd);
  writer.fd = -1;
  AnalysisReply reply{-1, {}};
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(15);
  for (;;) {
    if (interrupted.load(std::memory_order_relaxed))
      throw std::runtime_error(
          "C++ check cancelled; previous variables retained");
    if (std::chrono::steady_clock::now() >= deadline)
      throw std::runtime_error(
          "C++ check timed out; previous variables retained");
    std::array<char, 4096> buffer{};
    const auto count = ::read(output.fd, buffer.data(), buffer.size());
    if (count > 0) {
      const auto limit = std::string(mode) == "reflect" ? 4U * 1024U * 1024U : 65536U;
      if (reply.output.size() + static_cast<std::size_t>(count) > limit)
        throw std::runtime_error(
            "C++ checker output exceeded its limit; previous variables retained");
      reply.output.append(buffer.data(), static_cast<std::size_t>(count));
    } else if (count == 0) {
      int status = 0;
      const auto waited = ::waitpid(child.pid, &status, WNOHANG);
      if (waited == child.pid) {
        child.pid = -1;
        if (!WIFEXITED(status))
          throw std::runtime_error(
              "C++ checker stopped unexpectedly; previous variables retained");
        reply.status = WEXITSTATUS(status);
        return reply;
      }
      if (waited < 0 && errno != EINTR)
        throw std::runtime_error("cannot wait for C++ checker");
    } else if (errno != EAGAIN && errno != EINTR) {
      throw std::runtime_error("cannot read C++ checker output");
    }
    struct pollfd event{output.fd, POLLIN, 0};
    ::poll(&event, 1, 20);
  }
}
} // namespace tracer_repl

#include "shell.hpp"
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
  bool exited = false;
  ~Child() {
    if (pid > 0) {
      ::kill(-pid, SIGKILL);
      while (::waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {
      }
    }
  }
};
} // namespace

ShellOutput run_shell(const std::string &command, const std::string &directory) {
  if (command.empty() || command.size() > 65536 ||
      command.find('\0') != std::string::npos)
    throw std::invalid_argument(
        "shell command is empty, oversized or contains NUL");
  int out_pipe[2], err_pipe[2];
  if (::pipe(out_pipe))
    throw std::runtime_error("cannot create shell stdout pipe");
  File output{out_pipe[0]}, writer{out_pipe[1]};
  if (::pipe(err_pipe))
    throw std::runtime_error("cannot create shell stderr pipe");
  File error{err_pipe[0]}, error_writer{err_pipe[1]};
  File input{::open("/dev/null", O_RDONLY)};
  if (input.fd < 0)
    throw std::runtime_error("cannot open shell stdin");
  for (int fd : {output.fd, writer.fd, error.fd, error_writer.fd, input.fd})
    if (::fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)
      throw std::runtime_error("cannot configure shell descriptors");
  for (int fd : {output.fd, error.fd})
    if (::fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
      throw std::runtime_error("cannot configure shell output");
  posix_spawn_file_actions_t actions;
  if (::posix_spawn_file_actions_init(&actions))
    throw std::runtime_error("cannot initialize shell actions");
  posix_spawnattr_t attrs;
  if (::posix_spawnattr_init(&attrs)) {
    ::posix_spawn_file_actions_destroy(&actions);
    throw std::runtime_error("cannot initialize shell attributes");
  }
  int setup =
      ::posix_spawn_file_actions_adddup2(&actions, input.fd, STDIN_FILENO);
  setup |=
      ::posix_spawn_file_actions_adddup2(&actions, writer.fd, STDOUT_FILENO);
  setup |= ::posix_spawn_file_actions_adddup2(&actions, error_writer.fd,
                                              STDERR_FILENO);
  setup |= ::posix_spawnattr_setflags(&attrs, POSIX_SPAWN_SETPGROUP);
  setup |= ::posix_spawnattr_setpgroup(&attrs, 0);
  // Pass the directory and command as data to a fixed shell program.
  const char *direct[] = {"/bin/sh", "-c", command.c_str(), nullptr};
  const char *with_directory[] = {"/bin/sh", "-c", "cd -P \"$1\" && exec /bin/sh -c \"$2\"",
                                  "sh", directory.c_str(), command.c_str(), nullptr};
  auto argv = directory.empty() ? direct : with_directory;
  Child child;
  const auto started = setup ? setup
                             : ::posix_spawn(&child.pid, "/bin/sh", &actions,
                                             &attrs, const_cast<char **>(argv), environ);
  ::posix_spawnattr_destroy(&attrs);
  ::posix_spawn_file_actions_destroy(&actions);
  if (started) {
    child.pid = -1;
    throw std::runtime_error("cannot start shell: " +
                             std::string(std::strerror(started)));
  }
  ::close(writer.fd);
  writer.fd = -1;
  ::close(error_writer.fd);
  error_writer.fd = -1;
  ShellOutput result{-1, {}, {}};
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(10);
  bool stdout_done = false, stderr_done = false;
  while (!child.exited || !stdout_done || !stderr_done) {
    if (interrupted.load(std::memory_order_relaxed))
      throw std::runtime_error("shell cancelled; compiler options retained");
    if (std::chrono::steady_clock::now() >= deadline)
      throw std::runtime_error(
          "shell exceeded ten seconds; compiler options retained");
    const auto drain = [](File &file, std::string &text, bool &done) {
      if (done)
        return;
      std::array<char, 4096> buffer{};
      const auto count = ::read(file.fd, buffer.data(), buffer.size());
      if (count > 0) {
        if (text.size() + count > 65536)
          throw std::runtime_error(
              "shell output exceeded 64 KiB; compiler options retained");
        text.append(buffer.data(), static_cast<std::size_t>(count));
      } else if (count == 0)
        done = true;
      else if (errno != EAGAIN && errno != EINTR)
        throw std::runtime_error("cannot read shell output");
    };
    drain(output, result.output, stdout_done);
    drain(error, result.error, stderr_done);
    if (!child.exited) {
      // Keep the leader unreaped until group cleanup, reserving its PID so a
      // reused process-group id can never refer to an unrelated command.
      siginfo_t status{};
      const auto waited =
          ::waitid(P_PID, child.pid, &status, WEXITED | WNOHANG | WNOWAIT);
      if (waited == 0 && status.si_pid == child.pid) {
        child.exited = true;
        result.status = status.si_code == CLD_EXITED ? status.si_status
                                                     : 128 + status.si_status;
      } else if (waited < 0 && errno != EINTR)
        throw std::runtime_error("cannot wait for shell");
    }
    struct pollfd descriptors[]{{stdout_done ? -1 : output.fd, POLLIN, 0},
                                {stderr_done ? -1 : error.fd, POLLIN, 0}};
    ::poll(descriptors, 2, 10);
  }
  return result;
}

std::vector<std::string> split_compiler_flags(const std::string &text) {
  if (text.size() > 65536 || text.find('\0') != std::string::npos)
    throw std::invalid_argument(
        "compiler flag output is oversized or contains NUL");
  std::vector<std::string> result;
  std::string word;
  char quote = 0;
  bool started = false;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (c == '\\' && quote != '\'') {
      if (++i == text.size())
        throw std::invalid_argument("unfinished flag escape");
      const char escaped = text[i];
      if (quote == '"' && escaped != '"' && escaped != '\\' && escaped != '$' &&
          escaped != '`' && escaped != '\n')
        word += '\\';
      if (escaped != '\n')
        word += escaped;
      started = true;
    } else if (quote) {
      if (c == quote)
        quote = 0;
      else
        word += c;
      started = true;
    } else if (c == '\'' || c == '"') {
      quote = c;
      started = true;
    } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      if (started) {
        result.push_back(word);
        word.clear();
        started = false;
      }
    } else {
      word += c;
      started = true;
    }
  }
  if (quote)
    throw std::invalid_argument("unfinished quote in compiler flag output");
  if (started)
    result.push_back(word);
  if (result.size() > 128)
    throw std::invalid_argument("too many compiler flags");
  return result;
}
} // namespace tracer_repl

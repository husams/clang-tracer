#include "object_launch.hpp"

#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <system_error>
#include <thread>

#include <fcntl.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

namespace tracer::object_launch {

Child::~Child() { terminate(); }

void Child::terminate() noexcept {
  const auto child = release();
  if (child <= 0)
    return;
  ::kill(child, SIGKILL);
  for (;;) {
    int status;
    const auto found = ::waitpid(child, &status, __WALL);
    if ((found < 0 && errno != EINTR) ||
        (found == child && (WIFEXITED(status) || WIFSIGNALED(status))))
      break;
  }
}

Child Child::stop_on_exec(std::vector<std::string> &arguments, unsigned options,
                         std::chrono::steady_clock::time_point deadline,
                         const std::function<bool()> &cancelled) {
  if (arguments.empty() || arguments.front().empty())
    throw std::invalid_argument("object launch requires an executable");
  std::vector<char *> argv;
  for (auto &argument : arguments)
    argv.push_back(argument.data());
  argv.push_back(nullptr);
  int gate[2];
  if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, gate))
    throw std::system_error(errno, std::generic_category(), "create object launch gate");
  const auto pid = ::fork();
  if (pid == 0) {
    ::close(gate[1]);
    char go = 0;
    ssize_t bytes;
    do {
      bytes = ::read(gate[0], &go, 1);
    } while (bytes < 0 && errno == EINTR);
    if (bytes != 1)
      ::_exit(125);
    ::close(gate[0]);
    const auto input = ::open("/dev/null", O_RDONLY);
    if (input == -1 || ::dup2(input, STDIN_FILENO) == -1 ||
        ::dup2(STDERR_FILENO, STDOUT_FILENO) == -1)
      ::_exit(125);
    if (input != STDIN_FILENO)
      ::close(input);
    sigset_t mask;
    if (::sigprocmask(SIG_SETMASK, nullptr, &mask))
      ::_exit(125);
    ::sigdelset(&mask, SIGINT);
    ::sigdelset(&mask, SIGTERM);
    if (::sigprocmask(SIG_SETMASK, &mask, nullptr))
      ::_exit(125);
    ::execv(argv.front(), argv.data());
    ::_exit(127);
  }
  const auto fork_error = errno;
  ::close(gate[0]);
  if (pid < 0) {
    ::close(gate[1]);
    throw std::system_error(fork_error, std::generic_category(), "launch object watch target");
  }
  Child child(pid);
  const auto seize = ::ptrace(PTRACE_SEIZE, pid, nullptr,
      reinterpret_cast<void *>(static_cast<std::uintptr_t>(options | PTRACE_O_TRACEEXEC)));
  const auto seize_error = errno;
  const char go = 1;
  const auto released = seize != -1 && ::send(gate[1], &go, 1, MSG_NOSIGNAL) == 1;
  ::close(gate[1]);
  if (seize == -1)
    throw std::system_error(seize_error, std::generic_category(), "seize target before exec");
  if (!released)
    throw std::runtime_error("cannot release the object launch gate");
  for (;;) {
    int status;
    const auto found = ::waitpid(pid, &status, __WALL | WNOHANG);
    if (found == pid) {
      if (WIFEXITED(status) || WIFSIGNALED(status)) {
        child.release();
        throw std::runtime_error("target exited before the object launch exec stop");
      }
      if (!WIFSTOPPED(status) || (static_cast<unsigned>(status) >> 16) != PTRACE_EVENT_EXEC)
        throw std::runtime_error("target stopped before the expected exec event");
      return Child(child.release(), status);
    }
    if (found < 0 && errno != EINTR) {
      const auto error = errno;
      if (error == ECHILD)
        child.release();
      throw std::system_error(error, std::generic_category(), "wait for target exec");
    }
    if ((cancelled && cancelled()) || std::chrono::steady_clock::now() >= deadline)
      throw std::runtime_error("target did not reach the exec stop");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

} // namespace tracer::object_launch

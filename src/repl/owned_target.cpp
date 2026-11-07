#include "owned_target.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>

#if defined(__linux__) && defined(__x86_64__)
#include "../../include/tracer/launch.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <map>
#include <fcntl.h>
#include <poll.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS 38
#endif

extern char **environ;
#endif

namespace tracer_repl {
namespace {
[[noreturn]] void launch_error(const std::string &message, int error = errno) {
  throw std::system_error(error, std::generic_category(), message);
}

#if defined(__linux__) && defined(__x86_64__)
namespace fs = std::filesystem;
namespace gate = tracer::launch;
namespace call = tracer::launch::detail;

std::string render_call_reply(const call::CallReply &reply) {
  switch (reply.status) {
  case call::CallStatus::ok:
    break;
  case call::CallStatus::unknown_name:
    throw std::invalid_argument("no function registered under this name");
  case call::CallStatus::ambiguous_name:
    throw std::invalid_argument("more than one function is registered under this name");
  case call::CallStatus::wrong_count:
    throw std::invalid_argument("registered function expects " +
                                std::to_string(reply.detail) + " arguments");
  case call::CallStatus::invalid_argument:
    throw std::invalid_argument("argument " + std::to_string(reply.detail) +
                                " is not valid for the registered scalar type");
  case call::CallStatus::exception:
    throw std::runtime_error("target function threw a C++ exception");
  }
  std::ostringstream result;
  result.imbue(std::locale::classic());
  switch (reply.type) {
  case call::CallType::none:
    result << "type=void";
    break;
  case call::CallType::boolean:
    result << "type=bool value=" << std::boolalpha << (reply.unsigned_value != 0);
    break;
  case call::CallType::i32:
  case call::CallType::i64:
    result << (reply.type == call::CallType::i32 ? "type=int32" : "type=int64")
           << " value=" << reply.signed_value;
    break;
  case call::CallType::u32:
  case call::CallType::u64:
    result << (reply.type == call::CallType::u32 ? "type=uint32" : "type=uint64")
           << " value=" << reply.unsigned_value;
    break;
  case call::CallType::f32:
    result << "type=float value=" << std::setprecision(std::numeric_limits<float>::max_digits10)
           << reply.real_value;
    break;
  case call::CallType::f64:
    result << "type=double value=" << std::setprecision(std::numeric_limits<double>::max_digits10)
           << reply.real_value;
    break;
  }
  return result.str();
}

struct Fd {
  int value;
  explicit Fd(int fd) : value(fd) {}
  Fd(const Fd &) = delete;
  ~Fd() { if (value >= 0) ::close(value); }
  int take() noexcept {
    const int fd = value;
    value = -1;
    return fd;
  }

  void above_stdio() {
    if (value > STDERR_FILENO)
      return;
    const int copy = ::fcntl(value, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
    if (copy == -1)
      launch_error("cannot move launch descriptor above standard streams");
    ::close(value);
    value = copy;
  }
};

struct UnpublishedLogs {
  const std::string &directory, &output, &error;
  bool output_created = false, error_created = false, published = false;

  ~UnpublishedLogs() {
    if (published)
      return;
    if (output_created)
      ::unlink(output.c_str());
    if (error_created)
      ::unlink(error.c_str());
    if (!directory.empty())
      ::rmdir(directory.c_str());
  }
};

enum Setup { isolate = 1, authorize, directory, streams, channel, execute };
struct SetupError { int step; int error; };

[[noreturn]] void child_error(int socket, Setup step, int error) noexcept {
  const SetupError report{step, error};
  ssize_t sent;
  do {
    sent = ::send(socket, &report, sizeof(report), MSG_NOSIGNAL);
  } while (sent < 0 && errno == EINTR);
  ::_exit(126);
}

std::vector<char *> pointers(std::vector<std::string> &strings) {
  std::vector<char *> result;
  result.reserve(strings.size() + 1);
  for (auto &value : strings)
    result.push_back(value.data());
  result.push_back(nullptr);
  return result;
}

std::vector<std::string> environment(const LaunchOptions &options, int socket) {
  std::map<std::string, std::string> values;
  for (auto entry = environ; *entry; ++entry) {
    std::string text(*entry);
    const auto equals = text.find('=');
    if (equals != std::string::npos)
      values.emplace(text.substr(0, equals), text.substr(equals + 1));
  }
  for (const auto &text : options.environment) {
    const auto equals = text.find('=');
    const auto key = text.substr(0, equals);
    if (equals == std::string::npos || key.empty() ||
        !(std::isalpha(static_cast<unsigned char>(key.front())) || key.front() == '_') ||
        !std::all_of(key.begin(), key.end(), [](unsigned char c) {
          return std::isalnum(c) || c == '_';
        }) || key == gate::gate_environment || text.find('\0') != std::string::npos)
      throw std::invalid_argument("invalid or reserved launch environment assignment");
    values[key] = text.substr(equals + 1);
  }
  values[gate::gate_environment] = std::to_string(socket);
  std::vector<std::string> result;
  for (const auto &[key, value] : values)
    result.push_back(key + "=" + value);
  return result;
}

std::string log_directory() {
  auto pattern = (fs::temp_directory_path() / "clang-tracer-target.XXXXXX").string();
  if (!::mkdtemp(pattern.data()))
    launch_error("cannot create private target log directory");
  return pattern;
}
#endif
} // namespace

std::unique_ptr<OwnedTarget> OwnedTarget::start(const LaunchOptions &options, int tracer_pid) {
#if defined(__linux__) && defined(__x86_64__)
  if (tracer_pid <= 0)
    throw std::invalid_argument("launch requires a local server owned by this REPL");
  const auto valid = [](const std::string &text) { return text.find('\0') == std::string::npos; };
  if (options.executable.empty() || !valid(options.executable) || !valid(options.cwd) ||
      !valid(options.input) || !valid(options.output) || !valid(options.error) ||
      !std::all_of(options.arguments.begin(), options.arguments.end(), valid))
    throw std::invalid_argument("invalid launch path or argument");
  auto target = std::unique_ptr<OwnedTarget>(new OwnedTarget);
  target->executable_ = fs::canonical(options.executable).string();
  if (!fs::is_regular_file(target->executable_))
    throw std::invalid_argument("launch executable must be a regular file");
  const auto cwd = options.cwd.empty() ? fs::current_path().string()
                                       : fs::canonical(options.cwd).string();
  if (!fs::is_directory(cwd))
    throw std::invalid_argument("launch working directory must be a directory");
  const auto logs = options.output.empty() || options.error.empty() ? log_directory() : "";
  UnpublishedLogs cleanup{logs, target->output_, target->error_};
  target->output_ = options.output.empty() ? logs + "/stdout.log" : fs::absolute(options.output).string();
  target->error_ = options.error.empty() ? logs + "/stderr.log" : fs::absolute(options.error).string();
  const auto input_path = options.input.empty() ? std::string("/dev/null") : options.input;
  Fd input(::open(input_path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK));
  if (input.value < 0)
    launch_error("cannot open target stdin");
  struct stat input_info{};
  if (!options.input.empty() && (::fstat(input.value, &input_info) != 0 ||
                                 !S_ISREG(input_info.st_mode)))
    throw std::invalid_argument("target stdin must be a regular file");
  Fd output(::open(target->output_.c_str(), O_WRONLY | O_CLOEXEC | O_CREAT | O_EXCL, 0600));
  if (output.value < 0)
    launch_error("cannot create target stdout " + target->output_);
  cleanup.output_created = true;
  Fd error(::open(target->error_.c_str(), O_WRONLY | O_CLOEXEC | O_CREAT | O_EXCL, 0600));
  if (error.value < 0)
    launch_error("cannot create target stderr " + target->error_);
  cleanup.error_created = true;
  int sockets[2];
  if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) == -1)
    launch_error("cannot create launch checkpoint channel");
  Fd parent(sockets[0]), child(sockets[1]);
  for (auto *fd : {&input, &output, &error, &parent, &child})
    fd->above_stdio();

  auto env = environment(options, child.value);
  auto envp = pointers(env);
  std::vector<std::string> arguments{target->executable_};
  arguments.insert(arguments.end(), options.arguments.begin(), options.arguments.end());
  auto argv = pointers(arguments);
  auto *const exec_arguments = argv.data();
  auto *const exec_environment = envp.data();
  const auto *path = target->executable_.c_str();
  const auto *working = cwd.c_str();
  const auto parent_pid = ::getpid();
  const auto pid = ::fork();
  if (pid == -1)
    launch_error("cannot start owned target");
  if (pid == 0) {
    // gRPC can have threads in the parent. Use only syscall/POSIX operations
    // between fork and exec, with all arguments already allocated.
    ::close(parent.value);
    if (::setsid() == -1 ||
        ::syscall(SYS_prctl, PR_SET_NO_NEW_PRIVS, 1L, 0L, 0L, 0L) == -1 ||
        ::syscall(SYS_prctl, PR_SET_PDEATHSIG, SIGKILL, 0L, 0L, 0L) == -1)
      child_error(child.value, isolate, errno);
    if (::getppid() != parent_pid)
      ::_exit(126);
    if (::syscall(SYS_prctl, PR_SET_PTRACER, static_cast<long>(tracer_pid), 0L, 0L, 0L) == -1 &&
        errno != EINVAL)
      child_error(child.value, authorize, errno);
    if (::chdir(working) == -1)
      child_error(child.value, directory, errno);
    if (::dup2(input.value, STDIN_FILENO) == -1 ||
        ::dup2(output.value, STDOUT_FILENO) == -1 ||
        ::dup2(error.value, STDERR_FILENO) == -1)
      child_error(child.value, streams, errno);
    if (::fcntl(child.value, F_SETFD, 0) == -1)
      child_error(child.value, channel, errno);
    ::execve(path, exec_arguments, exec_environment);
    child_error(child.value, execute, errno);
  }
  target->pid_ = pid;
  target->gate_ = parent.take();
  cleanup.published = true;
  return target;
#else
  (void)options;
  (void)tracer_pid;
  throw std::runtime_error("owned launch requires Linux x86-64");
#endif
}

void OwnedTarget::close_gate() noexcept {
  if (gate_ >= 0)
    ::close(gate_);
  gate_ = -1;
  call_pending_ = false;
}

bool OwnedTarget::running() {
  if (pid_ < 0)
    return false;
  int status = 0;
  int result;
  do {
    result = ::waitpid(pid_, &status, WNOHANG);
  } while (result == -1 && errno == EINTR);
  if (!result)
    return true;
  if (result == -1 && errno != ECHILD)
    launch_error("cannot check owned target");
  if (result > 0)
    exit_status_ = status;
  pid_ = -1;
  close_gate();
  return false;
}

std::string OwnedTarget::exit_description() const {
  if (exit_status_ >= 0 && WIFEXITED(exit_status_))
    return "exited with status " + std::to_string(WEXITSTATUS(exit_status_));
  if (exit_status_ >= 0 && WIFSIGNALED(exit_status_))
    return "exited on signal " + std::to_string(WTERMSIG(exit_status_));
  return "exited";
}

void OwnedTarget::wait_ready(unsigned seconds, const std::atomic<bool> &cancelled) {
#if defined(__linux__) && defined(__x86_64__)
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
  for (;;) {
    if (cancelled.load())
      throw std::runtime_error("target launch cancelled");
    if (std::chrono::steady_clock::now() >= deadline)
      throw std::runtime_error("target readiness timed out waiting for the application checkpoint");
    SetupError report{};
    const auto count = ::recv(gate_, &report, sizeof(report), MSG_DONTWAIT | MSG_TRUNC);
    const char message = *reinterpret_cast<const char *>(&report);
    if (count == 1 && (message == gate::ready || message == call::call_ready)) {
      if (!running())
        throw std::runtime_error("target " + exit_description() + " at readiness");
      calls_enabled_ = message == call::call_ready;
      return;
    }
    if (count == static_cast<ssize_t>(sizeof(report)) && report.step >= isolate && report.step <= execute) {
      constexpr const char *steps[] = {"", "isolate process", "authorize tracer", "set working directory",
                                       "redirect standard streams", "inherit checkpoint channel", "exec"};
      launch_error(std::string("target launch could not ") + steps[report.step], report.error);
    }
    if (count == 0) {
      // The kernel can close descriptors just before waitpid sees exit.
      for (int attempt = 0; attempt < 3; ++attempt) {
        if (!running())
          throw std::runtime_error("target " + exit_description() + " before readiness");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      throw std::runtime_error("target closed the launch checkpoint before readiness");
    }
    if (count >= 0)
      throw std::runtime_error("invalid target readiness message");
    if (errno == EINTR)
      continue;
    if (errno != EAGAIN && errno != EWOULDBLOCK)
      launch_error("cannot receive target readiness");
    if (!running())
      throw std::runtime_error("target " + exit_description() + " before readiness");
    pollfd descriptor{gate_, POLLIN, 0};
    if (::poll(&descriptor, 1, 20) == -1 && errno != EINTR)
      launch_error("cannot wait for target readiness");
  }
#else
  (void)seconds;
  (void)cancelled;
  throw std::runtime_error("owned launch requires Linux x86-64");
#endif
}

void OwnedTarget::resume() {
#if defined(__linux__) && defined(__x86_64__)
  if (!running())
    throw std::runtime_error("target " + exit_description());
  if (!paused())
    throw std::runtime_error("target has already resumed");
  if (call_pending_)
    throw std::runtime_error("collect the pending result with %call-result before resuming");
  ssize_t sent;
  do {
    sent = ::send(gate_, &gate::resume, 1, MSG_NOSIGNAL);
  } while (sent == -1 && errno == EINTR);
  if (sent != 1)
    launch_error("cannot resume owned target");
  close_gate();
#else
  throw std::runtime_error("owned launch requires Linux x86-64");
#endif
}

void OwnedTarget::start_call(const std::string &name, const std::vector<std::string> &arguments) {
#if defined(__linux__) && defined(__x86_64__)
  if (!running())
    throw std::runtime_error("target " + exit_description());
  if (!calls_enabled())
    throw std::runtime_error("target calls require a paused checkpoint with registered functions");
  if (call_pending_)
    throw std::runtime_error("collect the pending result with %call-result before starting another call");
  call::CallRequest request;
  if (name.empty() || name.size() >= sizeof(request.name) || name.find('\0') != std::string::npos)
    throw std::invalid_argument("function name must contain 1 to 255 non-NUL bytes");
  if (arguments.size() > call::max_call_arguments)
    throw std::invalid_argument("a target call accepts at most eight scalar arguments");
  std::copy(name.begin(), name.end(), request.name);
  request.count = static_cast<std::uint32_t>(arguments.size());
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    const auto &argument = arguments[index];
    if (argument.size() >= sizeof(request.arguments[index]) || argument.find('\0') != std::string::npos)
      throw std::invalid_argument("a scalar argument must contain at most 63 non-NUL bytes");
    std::copy(argument.begin(), argument.end(), request.arguments[index]);
  }
  ssize_t sent;
  do {
    sent = ::send(gate_, &request, sizeof(request), MSG_DONTWAIT | MSG_NOSIGNAL);
  } while (sent == -1 && errno == EINTR);
  if (sent != static_cast<ssize_t>(sizeof(request)))
    launch_error("cannot send target call request");
  call_pending_ = true;
#else
  (void)name;
  (void)arguments;
  throw std::runtime_error("target calls require Linux x86-64");
#endif
}

std::string OwnedTarget::call_result(unsigned seconds, const std::atomic<bool> &cancelled) {
#if defined(__linux__) && defined(__x86_64__)
  if (!call_pending_)
    throw std::runtime_error("no pending target call");
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
  while (!call_protocol_failed_) {
    if (cancelled.load() || std::chrono::steady_clock::now() >= deadline) {
      throw std::runtime_error(
          std::string(cancelled.load() ? "target call wait interrupted" : "target call wait timed out") +
          "; function may still be running; use %call-result to collect its reply");
    }
    call::CallReply reply;
    const auto count = ::recv(gate_, &reply, sizeof(reply), MSG_DONTWAIT | MSG_TRUNC);
    if (count == static_cast<ssize_t>(sizeof(reply)) &&
        reply.version == call::call_protocol && reply.status <= call::CallStatus::exception &&
        reply.type <= call::CallType::f64) {
      call_pending_ = false;
      return render_call_reply(reply);
    }
    if (count < 0 && errno == EINTR)
      continue;
    if (count >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
      break;
    pollfd descriptor{gate_, POLLIN, 0};
    if (::poll(&descriptor, 1, 20) < 0 && errno != EINTR)
      break;
  }
  if (!running())
    throw std::runtime_error("target " + exit_description() + " without returning a call result");
  call_protocol_failed_ = true;
  throw std::runtime_error("target call reply unavailable; execution may have occurred; use %terminate");
#else
  (void)seconds;
  (void)cancelled;
  throw std::runtime_error("target calls require Linux x86-64");
#endif
}

void OwnedTarget::terminate() {
  if (!running())
    return;
  if (::kill(pid_, SIGKILL) != 0 && errno != ESRCH)
    launch_error("cannot terminate owned target");
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (running()) {
    if (std::chrono::steady_clock::now() >= deadline)
      throw std::runtime_error("termination has not completed for owned PID " + std::to_string(pid_));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

OwnedTarget::~OwnedTarget() {
  try {
    terminate();
  } catch (...) {
  }
  close_gate();
}
} // namespace tracer_repl

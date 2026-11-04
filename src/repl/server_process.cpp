#include "server_process.hpp"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <grpcpp/grpcpp.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#include <sys/syscall.h>
#endif

#include "tracer.grpc.pb.h"
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace tracer_repl {
namespace fs = std::filesystem;

std::string executable_path() {
#ifdef __APPLE__
  std::uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::vector<char> path(size);
  if (_NSGetExecutablePath(path.data(), &size) != 0) {
    throw std::runtime_error("cannot locate console executable");
  }
  return fs::canonical(path.data()).string();
#else
  return fs::canonical("/proc/self/exe").string();
#endif
}

std::string find_server(const std::string &override_path) {
  if (!override_path.empty()) {
    auto path = fs::absolute(override_path).string();
    if (::access(path.c_str(), X_OK) != 0) {
      throw std::runtime_error("server executable is unavailable: " + path);
    }
    return path;
  }
  const auto adjacent =
      fs::path(executable_path()).parent_path() / "clang-tracer-server";
  if (::access(adjacent.c_str(), X_OK) == 0) {
    return adjacent.string();
  }
  const char *env = std::getenv("PATH");
  std::string paths = env ? env : "";
  for (std::size_t start = 0; start <= paths.size();) {
    const auto end = paths.find(':', start);
    auto item = paths.substr(start, end - start);
    auto candidate = fs::absolute(fs::path(item.empty() ? "." : item) /
                                  "clang-tracer-server");
    if (::access(candidate.c_str(), X_OK) == 0) {
      return candidate.string();
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  throw std::runtime_error("cannot find clang-tracer-server; use --server or "
                           "%server start --executable PATH");
}

std::string read_source(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("cannot read source: " + path);
  }
  std::string source;
  char block[8192];
  while (in.read(block, sizeof(block)) || in.gcount()) {
    source.append(block, static_cast<std::size_t>(in.gcount()));
    if (source.size() > 4 * 1024 * 1024) {
      throw std::runtime_error("source exceeds 4 MiB: " + path);
    }
  }
  if (in.bad()) {
    throw std::runtime_error("source read failed: " + path);
  }
  if (source.empty()) {
    throw std::runtime_error("empty source: " + path);
  }
  return source;
}

std::string ServerProcess::diagnostics() const {
  std::ifstream input(directory_ + "/server.log", std::ios::binary);
  if (!input) {
    return {};
  }
  input.seekg(0, std::ios::end);
  const auto size = input.tellg();
  input.seekg(size > 8192 ? size - std::streamoff(8192) : std::streampos(0));
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

bool ServerProcess::running() {
  if (pid_ < 0) {
    return false;
  }
  int status = 0;
  const auto result = ::waitpid(pid_, &status, WNOHANG);
  if (result == 0 || (result < 0 && errno == EINTR)) {
    return true;
  }
  if (result < 0 && errno != ECHILD) {
    throw std::runtime_error("cannot wait for owned server: " +
                             std::string(std::strerror(errno)));
  }
  if (result > 0) {
    exit_status_ =
        WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
  }
  pid_ = -1;
  return false;
}

void ServerProcess::start(const std::string &executable,
                          const std::string &agent, unsigned timeout_seconds) {
#ifndef __linux__
  (void)executable;
  (void)agent;
  (void)timeout_seconds;
  throw std::runtime_error(
      "local server startup requires Linux x86_64; use --endpoint to connect");
#else
  if (running()) {
    throw std::runtime_error("this console already owns a server");
  }
  const auto binary = find_server(executable);
  std::string pattern =
      (fs::temp_directory_path() / "clang-tracer-repl.XXXXXX").string();
  std::vector<char> writable(pattern.begin(), pattern.end());
  writable.push_back(0);
  if (!::mkdtemp(writable.data())) {
    throw std::runtime_error("cannot create private server directory");
  }
  directory_ = writable.data();
  endpoint_ = "unix://" + directory_ + "/control.sock";
  const auto log_path = directory_ + "/server.log";
  const int log =
      ::open(log_path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
  if (log < 0) {
    throw std::runtime_error("cannot create server log: " + log_path);
  }
  std::vector<std::string> args = {binary, "--listen", endpoint_};
  if (!agent.empty()) {
    args.push_back("--agent");
    args.push_back(agent);
  }
  std::vector<char *> argv;
  for (auto &arg : args) {
    argv.push_back(arg.data());
  }
  argv.push_back(nullptr);
  const auto parent_pid = ::getpid();
  pid_ = ::fork();
  if (pid_ == 0) {
    // Only async-signal-safe functions are used after fork: gRPC may have
    // threads.
    if (::setsid() < 0) {
      ::_exit(126);
    }
    // Keep terminal interrupts separate, but ask the server to restore probes
    // gracefully even when its console dies without running C++ destructors.
    if (::syscall(SYS_prctl, PR_SET_PDEATHSIG, SIGTERM, 0L, 0L, 0L) != 0 ||
        ::getppid() != parent_pid)
      ::_exit(126);
    const int input = ::open("/dev/null", O_RDONLY);
    if (input < 0 || ::dup2(input, STDIN_FILENO) < 0 ||
        ::dup2(log, STDOUT_FILENO) < 0 || ::dup2(log, STDERR_FILENO) < 0) {
      ::_exit(126);
    }
    ::close(input);
    ::close(log);
    ::execv(binary.c_str(), argv.data());
    ::_exit(127);
  }
  ::close(log);
  if (pid_ < 0) {
    throw std::runtime_error("cannot launch server");
  }
  terminating_ = false;
  exit_status_ = 0;
  auto stub = clangtracer::v1::Tracer::NewStub(
      grpc::CreateChannel(endpoint_, grpc::InsecureChannelCredentials()));
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
  while (std::chrono::steady_clock::now() < deadline) {
    if (!running()) {
      throw std::runtime_error("server exited during startup: " +
                               diagnostics());
    }
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::milliseconds(200));
    clangtracer::v1::ListSessionsRequest request;
    clangtracer::v1::ListSessionsResponse response;
    if (stub->ListSessions(&context, request, &response).ok() && running()) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  try {
    stop();
  } catch (...) { /* Retain recovery identifiers below. */
  }
  throw std::runtime_error("server startup timed out; endpoint=" + endpoint_ +
                           " pid=" + std::to_string(pid_) + " log=" + log_path +
                           "\n" + diagnostics());
#endif
}

void ServerProcess::stop(unsigned timeout_seconds) {
  if (pid_ < 0) {
    return;
  }
  if (!terminating_ && running()) {
    if (::kill(pid_, SIGTERM) != 0 && errno != ESRCH) {
      throw std::runtime_error("cannot terminate owned server");
    }
    terminating_ = true;
  }
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
  while (running()) {
    if (std::chrono::steady_clock::now() >= deadline) {
      throw std::runtime_error(
          "server restoration still pending; retained pid=" +
          std::to_string(pid_) + " endpoint=" + endpoint_ +
          " directory=" + directory_);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  // Logs are retained on disk for diagnostics; never remove a live endpoint.
  terminating_ = false;
  if (exit_status_ != 0) {
    throw std::runtime_error("owned server exited with status " +
                             std::to_string(exit_status_) + ": " +
                             diagnostics());
  }
}

ServerProcess::~ServerProcess() {
  // Explicit controller shutdown reports errors; never force-kill a restoring
  // child.
  if (pid_ >= 0 && !terminating_) {
    ::kill(pid_, SIGTERM);
  }
}
} // namespace tracer_repl

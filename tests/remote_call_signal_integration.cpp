#include "injector.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
bool fail_fault_attribution = false;
bool attribution_failed = false;
}

extern "C" long __real_ptrace(enum __ptrace_request request, ...);

// The Linux x86-64 injector passes four integer or pointer arguments.
extern "C" long __wrap_ptrace(enum __ptrace_request request, pid_t tid,
                              void *address, void *data) {
  const long result = __real_ptrace(request, tid, address, data);
  if (result == 0 && request == PTRACE_GETSIGINFO && fail_fault_attribution &&
      static_cast<siginfo_t *>(data)->si_signo == SIGSEGV) {
    attribution_failed = true;
    errno = EIO;
    return -1;
  }
  return result;
}

namespace {
using namespace std::chrono_literals;
using tracer::injector::IdlePointUnavailable;
using tracer::injector::Process;

void require(bool condition, const std::string &message) {
  if (!condition)
    throw std::runtime_error(message);
}

class Sleeper {
public:
  Sleeper() : pid_(::fork()) {
    require(pid_ >= 0, "fork failed");
    if (pid_ == 0) {
      const rlimit no_core{0, 0};
      if (::prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 ||
          ::setrlimit(RLIMIT_CORE, &no_core) != 0)
        ::_exit(120);
      for (;;) {
        const timespec delay{0, 50 * 1000 * 1000};
        ::nanosleep(&delay, nullptr);
      }
    }
  }

  ~Sleeper() {
    if (pid_ > 0) {
      ::kill(pid_, SIGKILL);
      while (::waitpid(pid_, nullptr, 0) < 0 && errno == EINTR) {
      }
    }
  }

  Sleeper(const Sleeper &) = delete;
  Sleeper &operator=(const Sleeper &) = delete;

  pid_t pid() const { return pid_; }

  void wait_for_stop() {
    const auto status = wait(WUNTRACED);
    require(WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP,
            "child was not parked with SIGSTOP");
  }

  void wait_for_signal(int signal) {
    const auto status = wait(0);
    require(WIFSIGNALED(status) && WTERMSIG(status) == signal,
            "child did not exit from signal " + std::to_string(signal) +
                " (wait status " + std::to_string(status) + ")");
  }

private:
  int wait(int options) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline) {
      int status = 0;
      const pid_t result = ::waitpid(pid_, &status, WNOHANG | options);
      if (result == pid_) {
        if (WIFSIGNALED(status) || WIFEXITED(status))
          pid_ = -1;
        return status;
      }
      require(result == 0 || errno == EINTR, "waitpid failed");
      std::this_thread::sleep_for(10ms);
    }
    throw std::runtime_error("timed out waiting for child status");
  }

  pid_t pid_;
};

void attach(Process &process, pid_t pid) {
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  for (;;) {
    try {
      process.attach(pid);
      return;
    } catch (const IdlePointUnavailable &) {
      if (std::chrono::steady_clock::now() >= deadline)
        throw;
      std::this_thread::sleep_for(10ms);
    }
  }
}

std::uintptr_t send_process_signal(int signal) {
  sigset_t mask;
  ::sigemptyset(&mask);
  ::sigaddset(&mask, signal);
  if (::sigprocmask(SIG_UNBLOCK, &mask, nullptr) != 0 ||
      ::kill(::getpid(), signal) != 0)
    ::_exit(121);
  return 0;
}

template <typename Call>
void run_case(const char *name, int signal, bool suppress, Call call) {
  Sleeper child;
  Process process;
  attach(process, child.pid());
  std::string error;
  try {
    call(process);
  } catch (const std::runtime_error &failure) {
    error = failure.what();
  }
  require(error.find("remote call stopped on signal " + std::to_string(signal) +
                     " at PC") != std::string::npos,
          std::string(name) + ": wrong error: " + error);
  require((error.find("signal delivery suppressed") != std::string::npos) ==
              suppress,
          std::string(name) + ": wrong suppression decision: " + error);
  require(process.unsafe(), "failed call did not mark the target unsafe");
  process.detach();
  child.wait_for_stop();
  require(::kill(child.pid(), SIGCONT) == 0, "SIGCONT failed");
  if (suppress) {
    std::this_thread::sleep_for(100ms);
    require(::kill(child.pid(), SIGTERM) == 0, "SIGTERM failed");
  }
  child.wait_for_signal(suppress ? SIGTERM : signal);
  std::cout << name << ": " << (suppress ? "suppressed" : "preserved")
            << std::endl;
}
} // namespace

int main() {
  try {
    const auto fault = [](Process &process) { (void)process.remote_call(1); };
    run_case("confirmed synchronous SIGSEGV", SIGSEGV, true, fault);
    fail_fault_attribution = true;
    run_case("SIGSEGV with failed siginfo lookup", SIGSEGV, false, fault);
    fail_fault_attribution = false;
    require(attribution_failed, "test did not exercise a failed siginfo lookup");

    run_case("confirmed self-directed SIGABRT", SIGABRT, true, [](Process &process) {
      (void)process.remote_call(reinterpret_cast<std::uintptr_t>(&std::abort));
    });
    for (const int signal : {SIGABRT, SIGTRAP}) {
      run_case(signal == SIGABRT ? "process-directed SIGABRT" : "unexpected SIGTRAP",
               signal, false, [signal](Process &process) {
        (void)process.remote_call(
            reinterpret_cast<std::uintptr_t>(&send_process_signal),
            {static_cast<std::uintptr_t>(signal)}, 1);
      });
    }
  } catch (const std::exception &error) {
    std::cerr << "remote call signal test: " << error.what() << '\n';
    return 1;
  }
}

#include <cerrno>
#include <chrono>
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "admission.hpp"
#include "injector.hpp"

namespace {
void stop_child(pid_t child, int signal) {
  ::kill(child, signal);
  while (::waitpid(child, nullptr, 0) < 0 && errno == EINTR) {
  }
}
} // namespace

// Exercise real ptrace and agent-style dlopen, keeping the target ELF frozen.
int main(int argc, char **argv) {
  if (argc < 3) {
    std::cerr << "usage: bootstrap INJECTED_LIBRARY TARGET "
                 "[APPLICATION_LIBRARY...]\n";
    return 1;
  }
  pid_t child = -1;
  int ready[2]{-1, -1};
  try {
    std::vector<char *> target_arguments{argv[2]};
    for (int i = 3; i < argc; ++i) {
      target_arguments.push_back(argv[i]);
    }
    target_arguments.push_back(nullptr);
    if (::pipe2(ready, O_CLOEXEC) != 0) {
      throw std::runtime_error("pipe failed");
    }
    const auto parent = ::getpid();
    child = ::fork();
    if (child < 0) {
      throw std::runtime_error("fork failed");
    }
    if (child == 0) {
      if (::prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 || ::getppid() != parent ||
          ::dup2(ready[1], STDOUT_FILENO) < 0) {
        ::_exit(127);
      }
      ::close(ready[0]);
      ::close(ready[1]);
      ::execv(argv[2], target_arguments.data());
      ::_exit(127);
    }
    ::close(ready[1]);
    ready[1] = -1;
    char signal = 0;
    ssize_t received = 0;
    do {
      received = ::read(ready[0], &signal, 1);
    } while (received < 0 && errno == EINTR);
    ::close(ready[0]);
    ready[0] = -1;
    if (received != 1 || signal != 'R') {
      throw std::runtime_error("target did not become ready");
    }
    ::usleep(20000);
    const auto begin = std::chrono::steady_clock::now();
    const auto modules = tracer::injector::discover_application_modules(child);
    const auto discovered = std::chrono::steady_clock::now();
    tracer::injector::Process process;
    process.attach(child, modules);
    const auto attached = std::chrono::steady_clock::now();
    const std::string path = argv[1];
    const auto scratch = process.remote_allocate(path.size() + 1);
    process.remote_write(scratch, path.c_str(), path.size() + 1);
    const auto dlopen_address = process.resolve_remote_symbol("dlopen");
    const auto handle = process.remote_call(
        dlopen_address, {scratch, RTLD_NOW | RTLD_GLOBAL}, 2);
    if (!handle) {
      throw std::runtime_error("injected dlopen failed");
    }
    process.remote_free(scratch, path.size() + 1);
    process.detach();
    const auto finished = std::chrono::steady_clock::now();
    const auto milliseconds = [](auto duration) {
      return std::chrono::duration<double, std::milli>(duration).count();
    };
    std::cout << "modules,discover_ms,attach_ms,bootstrap_ms,total_ms\n"
              << modules.size() << ',' << milliseconds(discovered - begin)
              << ',' << milliseconds(attached - discovered) << ','
              << milliseconds(finished - attached) << ','
              << milliseconds(finished - begin) << '\n';
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    for (int fd : ready) {
      if (fd >= 0) {
        ::close(fd);
      }
    }
    if (child > 0) {
      stop_child(child, SIGKILL);
    }
    return 1;
  }
  stop_child(child, SIGTERM);
}

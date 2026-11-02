#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <dlfcn.h>
#include <link.h>
#include <signal.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#include "admission.hpp"
#include "injector.hpp"
#include "shared_stack.hpp"
#include "tracer/compat.hpp"

extern "C" void admission_busy_leaf(std::uint64_t *calls);

namespace {
bool legacy_stripped_startup = false;

using namespace std::chrono_literals;

struct SharedProgress {
  alignas(8) std::uint64_t main_calls = 0;
  alignas(8) std::uint64_t helper_calls = 0;
  alignas(8) std::uint64_t nested_calls = 0;
  alignas(8) std::uint64_t deep_calls = 0;
  alignas(8) std::uint64_t signal_calls = 0;
  alignas(8) std::uint64_t runtime_calls = 0;
  alignas(8) std::uint64_t shared_calls = 0;
  alignas(8) std::uint64_t shared_failures = 0;
};

SharedProgress *signal_progress = nullptr;

[[noreturn]] __attribute__((noinline)) void
busy_helper(SharedProgress *progress) {
  std::uint64_t iteration = 1;
  for (;;) {
    volatile std::uint64_t arithmetic =
        iteration * 0x9e3779b97f4a7c15ULL + 0x632be59bd9b4e019ULL;
    __atomic_add_fetch(&progress->helper_calls, 1ULL, __ATOMIC_RELAXED);
    iteration += arithmetic & 1U;
    ++iteration;
  }
}

class NestedLeaf {
public:
  __attribute__((noinline)) void spin(SharedProgress *progress) const {
    admission_busy_leaf(&progress->nested_calls);
    volatile int preserve_frame = 1;
    (void)preserve_frame;
  }
};

class NestedMethod {
public:
  __attribute__((noinline)) void run(SharedProgress *progress) const {
    NestedLeaf leaf;
    leaf.spin(progress);
    volatile int preserve_frame = 1;
    (void)preserve_frame;
  }
};

__attribute__((noinline)) void nested_entry(SharedProgress *progress) {
  NestedMethod method;
  method.run(progress);
  volatile int preserve_frame = 1;
  (void)preserve_frame;
}

__attribute__((noinline)) void deep_recursive(SharedProgress *progress,
                                              unsigned depth) {
  volatile unsigned preserve_frame = depth;
  if (depth == 0) {
    admission_busy_leaf(&progress->deep_calls);
    return;
  }
  deep_recursive(progress, depth - 1);
  if (preserve_frame == depth + 1) {
    std::puts("unreachable recursion frame guard");
  }
}

void busy_signal_handler(int, siginfo_t *, void *) {
  std::uint64_t iteration = 1;
  for (;;) {
    volatile std::uint64_t arithmetic =
        iteration * 0x9e3779b97f4a7c15ULL + 0x632be59bd9b4e019ULL;
    __atomic_add_fetch(&signal_progress->signal_calls, 1ULL, __ATOMIC_RELAXED);
    iteration += arithmetic & 1U;
    ++iteration;
  }
}

int busy_loader_callback(dl_phdr_info *, std::size_t, void *context) {
  auto *progress = static_cast<SharedProgress *>(context);
  std::uint64_t iteration = 1;
  for (;;) {
    volatile std::uint64_t arithmetic =
        iteration * 0x9e3779b97f4a7c15ULL + 0x632be59bd9b4e019ULL;
    __atomic_add_fetch(&progress->runtime_calls, 1ULL, __ATOMIC_RELAXED);
    iteration += arithmetic & 1U;
    ++iteration;
  }
}

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::uint64_t load_progress(const std::uint64_t *value) {
  return __atomic_load_n(value, __ATOMIC_RELAXED);
}

void clear_progress(SharedProgress *progress) {
  __atomic_store_n(&progress->main_calls, 0ULL, __ATOMIC_RELAXED);
  __atomic_store_n(&progress->helper_calls, 0ULL, __ATOMIC_RELAXED);
  __atomic_store_n(&progress->nested_calls, 0ULL, __ATOMIC_RELAXED);
  __atomic_store_n(&progress->deep_calls, 0ULL, __ATOMIC_RELAXED);
  __atomic_store_n(&progress->signal_calls, 0ULL, __ATOMIC_RELAXED);
  __atomic_store_n(&progress->runtime_calls, 0ULL, __ATOMIC_RELAXED);
  __atomic_store_n(&progress->shared_calls, 0ULL, __ATOMIC_RELAXED);
  __atomic_store_n(&progress->shared_failures, 0ULL, __ATOMIC_RELAXED);
}

std::uint64_t wait_for_progress(const std::uint64_t *value,
                                std::uint64_t after = 0) {
  const auto deadline = std::chrono::steady_clock::now() + 3s;
  do {
    const auto current = load_progress(value);
    if (current > after) {
      return current;
    }
    std::this_thread::sleep_for(1ms);
  } while (std::chrono::steady_clock::now() < deadline);
  throw std::runtime_error("child did not make progress before ptrace");
}

class TracedChild {
public:
  explicit TracedChild(pid_t pid) : pid_(pid) {}

  ~TracedChild() { terminate(); }

  TracedChild(const TracedChild &) = delete;
  TracedChild &operator=(const TracedChild &) = delete;

  pid_t pid() const { return pid_; }

  void seize_and_stop() {
    require(::ptrace(PTRACE_SEIZE, pid_, nullptr, nullptr) == 0,
            "PTRACE_SEIZE failed");
    traced_ = true;
    require(::ptrace(PTRACE_INTERRUPT, pid_, nullptr, nullptr) == 0,
            "PTRACE_INTERRUPT failed");
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    do {
      int status = 0;
      const auto result = ::waitpid(pid_, &status, __WALL | WNOHANG);
      if (result == pid_) {
        require(WIFSTOPPED(status),
                "ptrace child stopped for an unexpected reason");
        stopped_ = true;
        return;
      }
      require(result == 0, "waitpid failed while awaiting ptrace stop");
      std::this_thread::sleep_for(1ms);
    } while (std::chrono::steady_clock::now() < deadline);
    throw std::runtime_error("ptrace child did not stop before timeout");
  }

  user_regs_struct registers() const {
    user_regs_struct regs{};
    require(::ptrace(PTRACE_GETREGS, pid_, nullptr, &regs) == 0,
            "PTRACE_GETREGS failed");
    return regs;
  }

  void detach() {
    require(traced_ && stopped_, "cannot detach a child that is not stopped");
    require(::ptrace(PTRACE_DETACH, pid_, nullptr, nullptr) == 0,
            "PTRACE_DETACH failed");
    traced_ = false;
    stopped_ = false;
  }

  void require_alive() const {
    int status = 0;
    const auto result = ::waitpid(pid_, &status, WNOHANG);
    require(result == 0, "child exited instead of resuming after detach");
  }

  void require_untraced() const {
    std::ifstream status("/proc/" + std::to_string(pid_) + "/status");
    std::string line;
    while (std::getline(status, line)) {
      if (line.rfind("TracerPid:", 0) == 0) {
        int tracer = -1;
        require(std::sscanf(line.c_str(), "TracerPid:%d", &tracer) == 1 &&
                    tracer == 0,
                "child retained ptrace ownership after detach");
        return;
      }
    }
    throw std::runtime_error("could not read child TracerPid after detach");
  }

private:
  void terminate() noexcept {
    if (pid_ <= 0) {
      return;
    }
    if (traced_) {
      if (!stopped_ &&
          ::ptrace(PTRACE_INTERRUPT, pid_, nullptr, nullptr) == 0) {
        int status = 0;
        const auto deadline = std::chrono::steady_clock::now() + 1s;
        while (std::chrono::steady_clock::now() < deadline) {
          if (::waitpid(pid_, &status, __WALL | WNOHANG) == pid_) {
            stopped_ = WIFSTOPPED(status);
            break;
          }
          std::this_thread::sleep_for(1ms);
        }
      }
      if (stopped_) {
        if (::ptrace(PTRACE_DETACH, pid_, nullptr,
                     reinterpret_cast<void *>(
                         static_cast<std::intptr_t>(SIGKILL))) != 0) {
          (void)::ptrace(PTRACE_KILL, pid_, nullptr, nullptr);
        }
        traced_ = false;
      } else {
        (void)::ptrace(PTRACE_KILL, pid_, nullptr, nullptr);
      }
    }
    (void)::kill(pid_, SIGKILL);
    int status = 0;
    while (::waitpid(pid_, &status, 0) < 0 && errno == EINTR) {
    }
    pid_ = -1;
  }

  pid_t pid_ = -1;
  bool traced_ = false;
  bool stopped_ = false;
};

std::vector<tracer::injector::ApplicationModule> verify_module_discovery(
    pid_t pid, const std::vector<std::filesystem::path> &expected_modules) {
  auto discovered = tracer::injector::discover_application_modules(pid);
  std::set<std::filesystem::path> paths;
  for (const auto &module : discovered) {
    const std::filesystem::path path(module.path());
    require(path.is_absolute(),
            "discovery returned a non-absolute module path");
    paths.insert(std::filesystem::canonical(path));
  }
  for (const auto &expected : expected_modules) {
    require(::tracer::compat::contains(paths, std::filesystem::canonical(expected)),
            "automatic discovery omitted expected DSO " + expected.string());
  }

  Dl_info libc_info{};
  const auto libc_symbol = reinterpret_cast<void *>(
      reinterpret_cast<std::uintptr_t>(&::dl_iterate_phdr));
  require(::dladdr(libc_symbol, &libc_info) != 0 && libc_info.dli_fname,
          "could not identify libc for discovery exclusion check");
  require(!::tracer::compat::contains(paths, std::filesystem::canonical(libc_info.dli_fname)),
          "automatic discovery included libc");

  const auto loader_base = ::getauxval(AT_BASE);
  Dl_info loader_info{};
  require(
      loader_base != 0 &&
          ::dladdr(reinterpret_cast<void *>(loader_base + 1), &loader_info) !=
              0 &&
          loader_info.dli_fname,
      "could not identify the dynamic loader for discovery exclusion check");
  require(!::tracer::compat::contains(paths, std::filesystem::canonical(loader_info.dli_fname)),
          "automatic discovery included the dynamic loader");
  std::printf("automatic module snapshot pid=%ld count=%zu\n",
              static_cast<long>(pid), discovered.size());
  return discovered;
}

void run_case(TracedChild& child, const std::uint64_t* progress, bool expect_admitted,
              std::size_t minimum_depth, const char* label,
              ::tracer::compat::span<const tracer::injector::ApplicationModule> approved = {},
              const std::uint64_t* failures = nullptr) {
  const bool require_legacy_rejection = legacy_stripped_startup && expect_admitted;
  if (require_legacy_rejection) {
    expect_admitted = false;
  }
  (void)wait_for_progress(progress);
  child.seize_and_stop();
  const auto before = load_progress(progress);
  const auto regs = child.registers();
  bool admitted = false;
  std::size_t app_depth = 0;
  std::string rejection;
  try {
    app_depth = tracer::injector::require_application_rendezvous(
        child.pid(), regs, approved);
    admitted = true;
  } catch (const tracer::injector::IdlePointUnavailable &error) {
    rejection = error.what();
  }
  require(admitted == expect_admitted,
          std::string(label) + (expect_admitted ? " was rejected: " + rejection
                                                : " unexpectedly admitted"));
  if (require_legacy_rejection) {
    require(rejection == "legacy libc startup requires a proven main application frame",
            std::string(label) +
                " did not enforce the stripped legacy-main boundary: " + rejection);
  }
  if (admitted) {
    require(app_depth >= minimum_depth,
            std::string(label) + " reported too few executable frames: " +
                std::to_string(app_depth));
  }
  child.detach();
  child.require_alive();
  child.require_untraced();
  const auto after = wait_for_progress(progress, before);
  require(after > before,
          std::string(label) + " did not progress after detach");
  if (failures) {
    require(load_progress(failures) == 0,
            std::string(label) + " reported a busy-loop calculation failure");
  }

  int status = 0;
  require(::waitpid(child.pid(), &status, WNOHANG) == 0,
          std::string(label) + " exited after detach");
  std::printf("%s: %s (application frames=%zu) at RIP=0x%llx; progress %llu -> "
              "%llu after detach\n",
              label, admitted ? "admitted" : rejection.c_str(), app_depth,
              static_cast<unsigned long long>(regs.rip),
              static_cast<unsigned long long>(before),
              static_cast<unsigned long long>(after));
}

} // namespace

int main(int argc, char** argv) try {
#if defined(__GLIBC__) && !__GLIBC_PREREQ(2, 34)
  legacy_stripped_startup =
      std::filesystem::path(argv[0]).filename() == "admission-integration-stripped";
#endif
  std::vector<std::filesystem::path> expected_modules;
  for (int index = 1; index < argc; ++index) {
    const std::string option = argv[index];
    if (option != "--expected-module" || index + 1 == argc) {
      throw std::runtime_error("usage: admission-integration "
                               "--expected-module PATH "
                               "--expected-module PATH");
    }
    expected_modules.emplace_back(argv[++index]);
  }
  require(expected_modules.size() == 2,
          "admission integration requires two expected module paths");
  for (const auto &module : expected_modules) {
    require(module.is_absolute(), "--expected-module paths must be absolute");
  }
  auto *progress = static_cast<SharedProgress *>(
      ::mmap(nullptr, sizeof(SharedProgress), PROT_READ | PROT_WRITE,
             MAP_SHARED | MAP_ANONYMOUS, -1, 0));
  require(progress != reinterpret_cast<SharedProgress *>(MAP_FAILED),
          "could not allocate shared progress counters");
  std::memset(progress, 0, sizeof(*progress));

  signal_progress = progress;
  for (int case_index = 0; case_index != 10; ++case_index) {
    const bool helper_case = case_index == 1;
    const bool nested_case = case_index == 2;
    const bool deep_case = case_index == 3;
    const bool signal_case = case_index == 4;
    const bool runtime_case = case_index == 5;
    const bool shared_case = case_index >= 6;
    const bool shared_runtime_case = case_index == 9;
    clear_progress(progress);
    const pid_t pid = ::fork();
    require(pid >= 0, "fork failed");
    if (pid == 0) {
      if (helper_case) {
        busy_helper(progress);
      }
      if (nested_case) {
        nested_entry(progress);
      }
      if (deep_case) {
        deep_recursive(progress, 256);
      }
      if (signal_case) {
        struct sigaction action{};
        action.sa_sigaction = busy_signal_handler;
        action.sa_flags = SA_SIGINFO;
        ::sigemptyset(&action.sa_mask);
        if (::sigaction(SIGUSR1, &action, nullptr) != 0 ||
            ::raise(SIGUSR1) != 0) {
          _exit(5);
        }
      }
      if (runtime_case) {
        (void)::dl_iterate_phdr(busy_loader_callback, progress);
      }
      if (shared_case) {
        if (shared_runtime_case) {
          SharedStackState state{&progress->runtime_calls,
                                 &progress->shared_failures, nullptr};
          (void)::dl_iterate_phdr(shared_stack_runtime_callback, &state);
        } else {
          volatile int *const errno_address = &errno;
          *errno_address = EDOM;
          SharedStackState state{&progress->shared_calls,
                                 &progress->shared_failures, errno_address};
          shared_stack_enter(&state);
        }
      }
      std::uint64_t iteration = 1;
      for (;;) {
        volatile std::uint64_t arithmetic =
            iteration * 0x9e3779b97f4a7c15ULL + 0x632be59bd9b4e019ULL;
        __atomic_add_fetch(&progress->main_calls, 1ULL, __ATOMIC_RELAXED);
        iteration += arithmetic & 1U;
        ++iteration;
      }
    }

    TracedChild child(pid);
    const auto discovered_modules =
        verify_module_discovery(child.pid(), expected_modules);
    if (helper_case) {
      run_case(child, &progress->helper_calls, true, 2, "busy-helper accepted",
               discovered_modules);
    } else if (nested_case) {
      run_case(child, &progress->nested_calls, true, 5,
               "nested C++ frames accepted", discovered_modules);
    } else if (deep_case) {
      run_case(child, &progress->deep_calls, false, 0,
               "over-depth recursive chain rejected", discovered_modules);
    } else if (signal_case) {
      run_case(child, &progress->signal_calls, false, 0,
               "signal-handler frame rejected", discovered_modules);
    } else if (runtime_case) {
      run_case(child, &progress->runtime_calls, false, 0,
               "loader callback frame rejected", discovered_modules);
    } else if (case_index == 6) {
      run_case(child, &progress->shared_calls, true, 4,
               "auto-discovered shared-library stack accepted",
               discovered_modules, &progress->shared_failures);
    } else if (case_index == 7) {
      run_case(child, &progress->shared_calls, false, 0,
               "empty snapshot shared-library stack rejected", {});
    } else if (case_index == 8) {
      std::vector<tracer::injector::ApplicationModule> only_a;
      const auto expected_a =
          std::filesystem::canonical(expected_modules.front());
      for (const auto &module : discovered_modules) {
        if (std::filesystem::canonical(module.path()) == expected_a) {
          only_a.push_back(module);
        }
      }
      require(only_a.size() == 1,
              "automatic snapshot did not contain the first expected DSO");
      run_case(child, &progress->shared_calls, false, 0,
               "partial snapshot shared-library stack rejected", only_a);
    } else if (case_index == 9) {
      run_case(child, &progress->runtime_calls, false, 0,
               "auto-discovered DSO loader callback rejected",
               discovered_modules);
    } else {
      run_case(child, &progress->main_calls, true, 1, "busy-main accepted",
               discovered_modules);
    }
  }

  require(::munmap(progress, sizeof(*progress)) == 0,
          "could not release shared progress counters");
  std::puts("admission integration passed");
  return 0;
} catch (const std::exception& error) {

  std::fprintf(stderr, "admission integration failed: %s\n", error.what());
  return 1;
}

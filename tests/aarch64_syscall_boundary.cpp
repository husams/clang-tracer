#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>

#include "injector.hpp"
#include "object_target.hpp"

#if !defined(__linux__) || !defined(__aarch64__)
#error This regression requires native AArch64 Linux.
#endif

namespace {
std::uintptr_t watched_pc = 0;
unsigned observed_stops = 0;
std::atomic<std::uintptr_t> rendezvous_return{0};
static_assert(decltype(rendezvous_return)::is_always_lock_free);

void release_rendezvous(int, siginfo_t*, void* context) {
  static_cast<ucontext_t*>(context)->uc_mcontext.pc =
      rendezvous_return.load(std::memory_order_relaxed);
}
} // namespace

extern "C" long __real_ptrace(enum __ptrace_request request, ...);

extern "C" long __wrap_ptrace(enum __ptrace_request request, pid_t tid, void* address, void* data) {
  const long result = __real_ptrace(request, tid, address, data);
  if (result == 0 && request == PTRACE_GETREGSET &&
      reinterpret_cast<std::uintptr_t>(address) == NT_PRSTATUS) {
    const auto& buffer = *static_cast<iovec*>(data);
    if (buffer.iov_len == sizeof(user_pt_regs) &&
        static_cast<const user_pt_regs*>(buffer.iov_base)->pc == watched_pc) {
      ++observed_stops;
    }
  }
  return result;
}

namespace {
using namespace std::chrono_literals;
using tracer::injector::IdlePointUnavailable;
using tracer::injector::Process;

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

struct Shared {
  unsigned release;
  unsigned progress;
};

// File-backed RX text followed by a mapped, unreadable, non-executable page.
// Sleep admission deliberately excludes anonymous executable mappings.
class Target {
  std::size_t page_ = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
  unsigned char* code_ = nullptr;
  Shared* shared_ = nullptr;
  std::string path_;
  pid_t pid_ = -1;

public:
  const bool sleep;
  const bool fits;
  std::uintptr_t candidate = 0;
  std::uintptr_t admitted = 0;

  Target(bool sleeping, bool room)
      : sleep(sleeping)
      , fits(room) {
    char path[] = "/tmp/ct-arm-boundary-XXXXXX";
    const int fd = ::mkstemp(path);
    require(fd >= 0, "cannot create boundary text");
    path_ = path;
    const bool sized = ::ftruncate(fd, 2 * page_) == 0;
    void* mapping =
        sized ? ::mmap(nullptr, 2 * page_, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0) : MAP_FAILED;
    ::close(fd);
    if (mapping == MAP_FAILED) {
      ::unlink(path_.c_str());
      throw std::runtime_error("cannot map boundary text");
    }
    code_ = static_cast<unsigned char*>(mapping);
    const auto end = reinterpret_cast<std::uintptr_t>(code_ + page_);
    unsigned char* entry = nullptr;
    if (sleep) {
      constexpr auto sleep_number = 0xd2800008U | (SYS_nanosleep << 5);
      const std::array<std::uint32_t, 5> words{sleep_number, // mov x8, SYS_nanosleep
                                               0xaa1f03e1U,  // mov x1, xzr
                                               0xd4000001U,  // svc #0
                                               room ? 0xd503201fU : 0xd65f03c0U, // nop / ret
                                               0xd65f03c0U};                     // ret
      const auto size = (room ? 5 : 4) * sizeof(words[0]);
      entry = code_ + page_ - size;
      std::memcpy(entry, words.data(), size);
      candidate = reinterpret_cast<std::uintptr_t>(entry) + 8;
      admitted = candidate + 4;
    } else {
      // A stable PC makes the rejected boundary observable on every CPU.
      // SIGUSR1 releases the spin through the preceding RET after detachment.
      const std::array<std::uint32_t, 3> words{0xd65f03c0U, 0x14000000U, 0xd503201fU};
      const auto size = (room ? 3 : 2) * sizeof(words[0]);
      std::memcpy(code_ + page_ - size, words.data(), size);
      entry = code_ + page_ - size + 4;
      candidate = admitted = end - (room ? 8 : 4);
    }
    __builtin___clear_cache(reinterpret_cast<char*>(code_), reinterpret_cast<char*>(code_ + page_));
    require(::mprotect(code_, page_, PROT_READ | PROT_EXEC) == 0 &&
                ::mprotect(code_ + page_, page_, PROT_NONE) == 0,
            "cannot protect the boundary pages");
    void* shared =
        ::mmap(nullptr, sizeof(Shared), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    require(shared != MAP_FAILED, "cannot map target progress");
    shared_ = static_cast<Shared*>(shared);
    pid_ = ::fork();
    require(pid_ >= 0, "cannot fork boundary target");
    if (pid_ == 0) {
      if (::prctl(PR_SET_PDEATHSIG, SIGKILL) != 0) {
        ::_exit(120);
      }
      if (sleep) {
        const timespec delay{0, 50 * 1000 * 1000};
        const auto call = reinterpret_cast<long (*)(const timespec*)>(entry);
        while (!__atomic_load_n(&shared_->release, __ATOMIC_ACQUIRE)) {
          call(&delay);
          __atomic_fetch_add(&shared_->progress, 1, __ATOMIC_RELEASE);
        }
      } else {
        rendezvous_return = candidate - 4;
        struct sigaction action{};
        action.sa_sigaction = release_rendezvous;
        action.sa_flags = SA_SIGINFO;
        ::sigemptyset(&action.sa_mask);
        if (::sigaction(SIGUSR1, &action, nullptr) != 0) {
          ::_exit(121);
        }
        reinterpret_cast<void (*)()>(entry)();
        __atomic_fetch_add(&shared_->progress, 1, __ATOMIC_RELEASE);
      }
      ::_exit(0);
    }
  }

  ~Target() {
    if (pid_ > 0) {
      ::kill(pid_, SIGKILL);
      while (::waitpid(pid_, nullptr, 0) < 0 && errno == EINTR) {
      }
    }
    if (shared_) {
      ::munmap(shared_, sizeof(Shared));
    }
    if (code_) {
      ::munmap(code_, 2 * page_);
    }
    ::unlink(path_.c_str());
  }

  Target(const Target&) = delete;
  Target& operator=(const Target&) = delete;

  pid_t pid() const { return pid_; }

  std::uintptr_t start() const { return reinterpret_cast<std::uintptr_t>(code_); }

  std::size_t size() const { return page_; }

  const std::string& path() const { return path_; }

  void attach(Process& process) const {
    if (sleep) {
      process.attach(pid_);
    } else {
      process.attach_at_application_point(pid_, candidate, fits ? 8 : 4);
    }
  }

  void progress() const {
    const auto before = __atomic_load_n(&shared_->progress, __ATOMIC_ACQUIRE);
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (__atomic_load_n(&shared_->progress, __ATOMIC_ACQUIRE) == before) {
      require(std::chrono::steady_clock::now() < deadline,
              "boundary target made no progress after detach");
      std::this_thread::sleep_for(1ms);
    }
  }

  void finish() {
    if (sleep) {
      progress();
    }
    __atomic_store_n(&shared_->release, 1, __ATOMIC_RELEASE);
    if (!sleep) {
      require(::kill(pid_, SIGUSR1) == 0, "cannot release boundary rendezvous");
    }
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    for (;;) {
      int status = 0;
      const auto result = ::waitpid(pid_, &status, WNOHANG);
      if (result == pid_) {
        pid_ = -1;
        require(WIFEXITED(status) && WEXITSTATUS(status) == 0,
                "boundary target did not survive attachment");
        require(__atomic_load_n(&shared_->progress, __ATOMIC_ACQUIRE) > 0,
                "boundary target made no progress after detach");
        return;
      }
      require(result == 0 || errno == EINTR, "cannot wait for boundary target");
      require(std::chrono::steady_clock::now() < deadline,
              "boundary target remained stopped after detach");
      std::this_thread::sleep_for(1ms);
    }
  }
};

template <typename T>
T regset(pid_t tid, unsigned note) {
  T value{};
  iovec buffer{&value, sizeof(value)};
  require(::__real_ptrace(PTRACE_GETREGSET, tid,
                          reinterpret_cast<void*>(static_cast<std::uintptr_t>(note)),
                          &buffer) == 0 &&
              buffer.iov_len == sizeof(value),
          "cannot snapshot boundary registers");
  return value;
}

struct State {
  user_pt_regs general{};
  user_fpsimd_state floating{};
  int syscall = -1;
  std::uint64_t signals = 0;

  explicit State(pid_t tid)
      : general(regset<user_pt_regs>(tid, NT_PRSTATUS))
      , floating(regset<user_fpsimd_state>(tid, NT_PRFPREG))
      , syscall(regset<int>(tid, NT_ARM_SYSTEM_CALL)) {
    require(::__real_ptrace(PTRACE_GETSIGMASK, tid, sizeof(signals), &signals) == 0,
            "cannot snapshot target signal mask");
  }

  void check(pid_t tid) const {
    const State after(tid);
    require(std::memcmp(&general, &after.general, sizeof(general)) == 0 &&
                std::memcmp(&floating, &after.floating, sizeof(floating)) == 0 &&
                syscall == after.syscall && signals == after.signals,
            "remote syscall changed target registers or signal mask");
  }
};

void admitted_case(bool sleep) {
  Target target(sleep, true);
  Process process;
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  for (;;) {
    try {
      target.attach(process);
      break;
    } catch (const IdlePointUnavailable&) {
      require(std::chrono::steady_clock::now() < deadline,
              "boundary attachment never reached the admitted PC");
      std::this_thread::sleep_for(1ms);
    }
  }
  require(process.instruction_pointer() == target.admitted,
          "attachment did not stop eight bytes before the executable boundary");
  const State before(target.pid());
  std::vector<unsigned char> code(target.size()), after(target.size());
  process.remote_read(target.start(), code.data(), code.size());
  const auto check = [&] {
    before.check(target.pid());
    process.remote_read(target.start(), after.data(), after.size());
    require(code == after && !process.unsafe(), "boundary text was not restored");
  };
  for (int repeat = 0; repeat != 3; ++repeat) {
    const auto memory = process.remote_allocate(target.size());
    check();
    process.remote_free(memory, target.size());
    check();
    const auto file = process.remote_map_file(target.path(), target.size());
    check();
    process.remote_free(file, target.size());
    check();
    require(process.remote_call(reinterpret_cast<std::uintptr_t>(&::getpid)) ==
                static_cast<std::uintptr_t>(target.pid()),
            "remote getpid returned the wrong PID");
    check(); // Includes scratch allocation, call-page mprotect and remote-call restoration.
    bool failed = false;
    try {
      process.remote_free(target.start() + 1, target.size());
    } catch (const std::runtime_error& error) {
      failed =
          std::string(error.what()).find("remote syscall failed with errno") != std::string::npos;
    }
    require(failed, "unaligned remote munmap did not exercise syscall error restoration");
    check();
  }
  process.detach(); // Frees the call page and scratch stack at the same boundary PC.
  target.finish();
  std::cout << (sleep ? "sleep" : "rendezvous")
            << ": eight-byte boundary admitted; bytes/registers restored; target progressed\n";
}

void rejected_case(bool sleep) {
  Target target(sleep, false);
  watched_pc = target.candidate;
  observed_stops = 0;
  Process process;
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  do {
    bool rejected = false;
    try {
      target.attach(process);
    } catch (const IdlePointUnavailable&) {
      rejected = true;
    }
    require(rejected && !process.attached() && !process.unsafe(),
            "attachment admitted an incomplete syscall stub at the boundary");
    require(std::chrono::steady_clock::now() < deadline,
            "did not observe the rejected boundary PC");
    std::this_thread::sleep_for(1ms);
  } while (observed_stops < 3);
  watched_pc = 0;
  // Patch-only admission is allowed at arbitrary PCs, but cannot enter any syscall path.
  process.attach_for_patch(target.pid());
  bool rejected = false;
  try {
    (void)process.remote_allocate(target.size());
  } catch (const std::logic_error&) {
    rejected = true;
  }
  require(rejected && !process.unsafe(), "patch-only attachment executed a syscall");
  process.detach();
  target.finish();
  std::cout << (sleep ? "sleep" : "rendezvous")
            << ": four-byte boundary rejected repeatedly; attachment survived; target progressed\n";
}

} // namespace

int main() {
  try {
    for (bool sleep : {true, false}) {
      admitted_case(sleep);
      rejected_case(sleep);
    }
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

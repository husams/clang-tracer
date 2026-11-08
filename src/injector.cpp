#include "injector.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <istream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <dirent.h>
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#include "admission.hpp"
#include "linux_syscalls.hpp"
#include "tracer/compat.hpp"

#ifndef NT_X86_XSTATE
#define NT_X86_XSTATE 0x202
#endif

// glibc exposes the seize requests even with this SDK's older kernel headers.
// The running kernel must still support the existing seize/interrupt protocol.
#ifndef PTRACE_EVENT_STOP
#define PTRACE_EVENT_STOP 128
#endif

namespace tracer::injector {
namespace {

constexpr std::size_t kXstateCapacity = 64 * 1024;
constexpr std::size_t kRemoteStackSize = 8 * 1024 * 1024;
constexpr auto kStopTimeout = std::chrono::seconds(3);
constexpr auto kCallTimeout = std::chrono::seconds(30);
constexpr std::uint64_t kX86TraceFlag = 0x100;
constexpr std::uint64_t kX86DirectionFlag = 0x400;
constexpr std::size_t kKernelSignalMaskSize = sizeof(std::uint64_t);

[[noreturn]] void throw_errno(std::string_view operation) {
  const int error = errno;
  throw std::runtime_error(std::string(operation) + ": " +
                           std::strerror(error));
}

long ptrace_checked(enum __ptrace_request request, pid_t tid, void *address,
                    void *data, std::string_view operation) {
  errno = 0;
  const long result = ::ptrace(request, tid, address, data);
  if (result == -1 && errno != 0) {
    throw_errno(operation);
  }
  return result;
}

bool wait_stopped(pid_t tid, std::chrono::steady_clock::time_point deadline,
                  int &status) {
  for (;;) {
    const pid_t result = ::waitpid(tid, &status, __WALL | WNOHANG);
    if (result == tid) {
      return WIFSTOPPED(status);
    }
    if (result == -1 && errno != EINTR) {
      throw_errno("waitpid for tracee");
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

void wait_stopped_or_throw(pid_t tid,
                           std::chrono::steady_clock::time_point deadline,
                           std::string_view context) {
  int status = 0;
  if (!wait_stopped(tid, deadline, status)) {
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
      throw std::runtime_error(std::string(context) +
                               ": tracee exited while waiting");
    }
    throw std::runtime_error(std::string(context) +
                             ": timed out waiting for tracee stop");
  }
}

std::vector<pid_t> thread_ids(pid_t process_id) {
  const std::string path = "/proc/" + std::to_string(process_id) + "/task";
  DIR *directory = ::opendir(path.c_str());
  if (directory == nullptr) {
    throw_errno("open process task directory");
  }
  std::unique_ptr<DIR, decltype(&::closedir)> guard(directory, ::closedir);
  std::vector<pid_t> result;
  while (const dirent *entry = ::readdir(directory)) {
    if (entry->d_name[0] == '.') {
      continue;
    }
    char *end = nullptr;
    const long value = std::strtol(entry->d_name, &end, 10);
    if (end != entry->d_name && *end == '\0' && value > 0 &&
        value <= std::numeric_limits<pid_t>::max()) {
      result.push_back(static_cast<pid_t>(value));
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}

bool thread_exists(pid_t process_id, pid_t tid) {
  const std::string path =
      "/proc/" + std::to_string(process_id) + "/task/" + std::to_string(tid);
  struct stat status{};
  return ::stat(path.c_str(), &status) == 0;
}

long parse_syscall_number(std::istream &input) {
  std::string first;
  input >> first;
  if (first.empty() || first == "running") {
    return -1;
  }
  long number = -1;
  const auto parsed =
      std::from_chars(first.data(), first.data() + first.size(), number);
  return parsed.ec == std::errc{} && parsed.ptr == first.data() + first.size()
             ? number
             : -1;
}

long stopped_syscall(pid_t tid) {
  std::ifstream input("/proc/" + std::to_string(tid) + "/syscall");
  if (!input) {
    throw std::runtime_error(
        "cannot inspect /proc/<pid>/syscall; refusing an unknown stop");
  }
  return parse_syscall_number(input);
}

std::string join_rejections(const std::vector<std::string> &rejections) {
  constexpr std::size_t kMaxListed = 32;
  std::string joined;
  const std::size_t listed = std::min(rejections.size(), kMaxListed);
  for (std::size_t i = 0; i < listed; ++i) {
    if (i != 0) {
      joined += "; ";
    }
    joined += rejections[i];
  }
  if (rejections.size() > listed) {
    joined += "; and " + std::to_string(rejections.size() - listed) +
              " more";
  }
  return joined;
}

std::string_view syscall_name(long number) {
  switch (number) {
  case SYS_futex:
    return "futex";
  case SYS_epoll_wait:
    return "epoll_wait";
  case SYS_poll:
    return "poll";
  case SYS_read:
    return "read";
  case SYS_accept:
    return "accept";
  case SYS_accept4:
    return "accept4";
  case SYS_nanosleep:
    return "nanosleep";
  case SYS_clock_nanosleep:
    return "clock_nanosleep";
  default:
    return "unknown";
  }
}

void verify_target_executable(pid_t process_id) {
  const std::string path = "/proc/" + std::to_string(process_id) + "/exe";
  struct stat executable_status{};
  if (::stat(path.c_str(), &executable_status) != 0) {
    throw_errno("stat target executable");
  }
  if ((executable_status.st_mode & (S_ISUID | S_ISGID)) != 0) {
    throw std::runtime_error(
        "refusing to trace a setuid/setgid target executable");
  }
  std::ifstream executable(path, std::ios::binary);
  Elf64_Ehdr header{};
  executable.read(reinterpret_cast<char *>(&header), sizeof(header));
  if (!executable || std::memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
      header.e_ident[EI_CLASS] != ELFCLASS64 ||
      header.e_ident[EI_DATA] != ELFDATA2LSB || header.e_machine != EM_X86_64) {
    throw std::runtime_error(
        "target is not a native little-endian Linux x86_64 ELF executable");
  }
}

struct RegisterState {
  user_regs_struct general{};
  std::vector<unsigned char> xstate =
      std::vector<unsigned char>(kXstateCapacity);
  std::size_t xstate_size = 0;
};

RegisterState read_registers(pid_t tid) {
  RegisterState state;
  iovec general_iov{&state.general, sizeof(state.general)};
  ptrace_checked(
      PTRACE_GETREGSET, tid,
      reinterpret_cast<void *>(static_cast<std::uintptr_t>(NT_PRSTATUS)),
      &general_iov, "read general registers");
  iovec xstate_iov{state.xstate.data(), state.xstate.size()};
  if (::ptrace(
          PTRACE_GETREGSET, tid,
          reinterpret_cast<void *>(static_cast<std::uintptr_t>(NT_X86_XSTATE)),
          &xstate_iov) == -1) {
    if (errno != EINVAL && errno != EIO) {
      throw_errno("read extended x86 state");
    }
    // Kernels without NT_X86_XSTATE expose the legacy floating point set.
    xstate_iov = {state.xstate.data(), state.xstate.size()};
    ptrace_checked(
        PTRACE_GETREGSET, tid,
        reinterpret_cast<void *>(static_cast<std::uintptr_t>(NT_FPREGSET)),
        &xstate_iov, "read legacy floating point state");
  }
  state.xstate_size = xstate_iov.iov_len;
  state.xstate.resize(state.xstate_size);
  return state;
}

void write_registers(pid_t tid, const RegisterState &state) {
  iovec general_iov{const_cast<user_regs_struct *>(&state.general),
                    sizeof(state.general)};
  ptrace_checked(
      PTRACE_SETREGSET, tid,
      reinterpret_cast<void *>(static_cast<std::uintptr_t>(NT_PRSTATUS)),
      &general_iov, "restore general registers");
  iovec xstate_iov{const_cast<unsigned char *>(state.xstate.data()),
                   state.xstate.size()};
  if (::ptrace(
          PTRACE_SETREGSET, tid,
          reinterpret_cast<void *>(static_cast<std::uintptr_t>(NT_X86_XSTATE)),
          &xstate_iov) == -1) {
    if (errno != EINVAL && errno != EIO) {
      throw_errno("restore extended x86 state");
    }
    xstate_iov = {const_cast<unsigned char *>(state.xstate.data()),
                  state.xstate.size()};
    ptrace_checked(
        PTRACE_SETREGSET, tid,
        reinterpret_cast<void *>(static_cast<std::uintptr_t>(NT_FPREGSET)),
        &xstate_iov, "restore legacy floating point state");
  }
}

std::uint64_t read_signal_mask(pid_t tid) {
  std::uint64_t mask = 0;
  ptrace_checked(PTRACE_GETSIGMASK, tid,
                 reinterpret_cast<void *>(kKernelSignalMaskSize), &mask,
                 "read tracee signal mask");
  return mask;
}

void write_signal_mask(pid_t tid, std::uint64_t mask) {
  ptrace_checked(PTRACE_SETSIGMASK, tid,
                 reinterpret_cast<void *>(kKernelSignalMaskSize), &mask,
                 "set tracee signal mask");
}

std::uintptr_t register_ip(const user_regs_struct &registers) {
  return static_cast<std::uintptr_t>(registers.rip);
}

bool confirmed_injected_fault(const siginfo_t &info,
                              const user_regs_struct &registers,
                              pid_t process_id, pid_t tid) {
  // A signal-specific synchronous fault was raised while this thread was
  // executing the injected code. Generic SI_KERNEL and async codes do not
  // establish that, even when their value is positive.
  switch (info.si_signo) {
  case SIGSEGV:
    return info.si_code == SEGV_MAPERR || info.si_code == SEGV_ACCERR;
  case SIGBUS:
    return info.si_code == BUS_ADRALN || info.si_code == BUS_ADRERR ||
           info.si_code == BUS_OBJERR;
  case SIGILL:
    return info.si_code >= ILL_ILLOPC && info.si_code <= ILL_BADSTK;
  case SIGFPE:
    return info.si_code >= FPE_INTDIV && info.si_code <= FPE_FLTSUB;
  case SIGABRT:
    // si_pid is shared by peer threads. Confirm that the stopped thread
    // itself issued a successful thread-directed abort to its own TID.
    return info.si_code == SI_TKILL && info.si_pid == process_id &&
           registers.rax == 0 &&
           ((registers.orig_rax == SYS_tgkill &&
             registers.rdi == static_cast<unsigned long long>(process_id) &&
             registers.rsi == static_cast<unsigned long long>(tid) &&
             registers.rdx == SIGABRT) ||
            (registers.orig_rax == SYS_tkill &&
             registers.rdi == static_cast<unsigned long long>(tid) &&
             registers.rsi == SIGABRT));
  default:
    return false;
  }
}

enum class InjectedStopKind { unexpected, completion, confirmed_fault };

struct InjectedStop {
  user_regs_struct registers{};
  int signal;
  InjectedStopKind kind = InjectedStopKind::unexpected;
};

InjectedStop inspect_injected_stop(pid_t process_id, pid_t tid, int status,
                                   std::uintptr_t completion_ip,
                                   int &pending_signal) {
  InjectedStop stop{{}, WSTOPSIG(status)};
  const bool signal_stop = (static_cast<unsigned int>(status) >> 16) == 0;
  const int earlier_signal = pending_signal;
  // Record the signal first so a failed attribution or register read leaves
  // it available to the normal recovery path.
  if (signal_stop)
    pending_signal = stop.signal;
  ptrace_checked(PTRACE_GETREGS, tid, nullptr, &stop.registers,
                 "read injected stop registers");
  siginfo_t info{};
  if (!signal_stop || ::ptrace(PTRACE_GETSIGINFO, tid, nullptr, &info) != 0 ||
      info.si_signo != stop.signal)
    return stop;
  if (stop.signal == SIGTRAP && stop.registers.rip == completion_ip &&
      (info.si_code == TRAP_BRKPT || info.si_code == SI_KERNEL))
    stop.kind = InjectedStopKind::completion;
  else if (confirmed_injected_fault(info, stop.registers, process_id, tid))
    stop.kind = InjectedStopKind::confirmed_fault;
  if (stop.kind != InjectedStopKind::unexpected)
    pending_signal = earlier_signal;
  return stop;
}

std::vector<unsigned char> read_remote(pid_t tid, std::uintptr_t address,
                                       std::size_t size) {
  std::vector<unsigned char> result(size);
  std::size_t copied = 0;
  while (copied < size) {
    errno = 0;
    const long word =
        ::ptrace(PTRACE_PEEKDATA, tid,
                 reinterpret_cast<void *>(address + copied), nullptr);
    if (word == -1 && errno != 0) {
      throw_errno("read tracee memory");
    }
    const auto amount = std::min(sizeof(word), size - copied);
    std::memcpy(result.data() + copied, &word, amount);
    copied += amount;
  }
  return result;
}

void write_remote(pid_t tid, std::uintptr_t address, const void *source,
                  std::size_t size) {
  const auto *bytes = static_cast<const unsigned char *>(source);
  std::size_t copied = 0;
  while (copied < size) {
    const auto current = address + copied;
    const auto aligned =
        current & ~(static_cast<std::uintptr_t>(sizeof(long)) - 1);
    const auto offset = static_cast<std::size_t>(current - aligned);
    long word = 0;
    const auto amount = std::min(sizeof(word) - offset, size - copied);
    errno = 0;
    word = ::ptrace(PTRACE_PEEKDATA, tid, reinterpret_cast<void *>(aligned),
                    nullptr);
    if (word == -1 && errno != 0) {
      throw_errno("read tracee memory before write");
    }
    std::memcpy(reinterpret_cast<unsigned char *>(&word) + offset,
                bytes + copied, amount);
    ptrace_checked(PTRACE_POKEDATA, tid, reinterpret_cast<void *>(aligned),
                   reinterpret_cast<void *>(static_cast<std::uintptr_t>(word)),
                   "write tracee memory");
    copied += amount;
  }
}

} // namespace

struct Process::PeerFreeze {
  struct Peer {
    pid_t tid = -1;
    int pending_signal = 0;
    bool seized = false;
    bool stopped = false;
  };

  using PeerIterator = std::vector<Peer>::iterator;

  Process &process;
  std::vector<Peer> peers;
  bool released = false;

  explicit PeerFreeze(Process &owner) : process(owner) {
    const auto deadline = std::chrono::steady_clock::now() + kStopTimeout;
    try {
      for (;;) {
        bool added = false;
        for (const pid_t tid : thread_ids(process.process_id_)) {
          if (tid == process.tid_ || find(tid) != peers.end()) {
            continue;
          }
          Peer peer;
          peer.tid = tid;
          if (::ptrace(PTRACE_SEIZE, tid, nullptr, nullptr) == -1) {
            const int seize_error = errno;
            if (seize_error == EPERM) {
              // A churned TID can disappear (or be reused outside this
              // process) between enumeration and SEIZE. Only a confirmed
              // missing task is safe to discard; permissions or an existing
              // tracer on a surviving task must still reject the freeze.
              const std::string path = "/proc/" + std::to_string(process.process_id_) +
                                       "/task/" + std::to_string(tid);
              struct stat task_status{};
              if (::stat(path.c_str(), &task_status) != 0 && errno == ENOENT)
                continue;
              errno = seize_error;
            }
            if (errno == ESRCH || errno == ECHILD) {
              if (thread_exists(process.process_id_, tid)) {
                throw std::runtime_error(
                    "PTRACE_SEIZE reported a missing peer that remains in "
                    "/proc; refusing an incomplete freeze");
              }
              continue;
            }
            throw_errno("PTRACE_SEIZE peer thread");
          }
          peer.seized = true;
          peers.push_back(peer);
          process.traced_peers_.emplace_back(tid, 0);
          added = true;
          if (::ptrace(PTRACE_INTERRUPT, tid, nullptr, nullptr) == -1) {
            if (errno == ESRCH || errno == ECHILD) {
              if (thread_exists(process.process_id_, tid)) {
                throw std::runtime_error(
                    "PTRACE_INTERRUPT reported a missing peer that remains in "
                    "/proc; refusing an incomplete freeze");
              }
              peers.back().seized = false;
              forget(tid);
              continue;
            }
            throw_errno("PTRACE_INTERRUPT peer thread");
          }
          try {
            wait_peer(peers.back(), deadline, process.process_id_);
          } catch (...) {
            update_signal(tid, peers.back().pending_signal);
            throw;
          }
          update_signal(tid, peers.back().pending_signal);
          if (!peers.back().seized) {
            forget(tid);
          }
        }

        // Threads may be created while the current snapshot is being stopped.
        // Repeat until a full enumeration adds no peer.
        const auto current = thread_ids(process.process_id_);
        const bool missing =
            std::any_of(current.begin(), current.end(), [&](pid_t tid) {
              return tid != process.tid_ && find(tid) == peers.end();
            });
        if (!added && !missing) {
          return;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
          throw std::runtime_error(
              "timed out stabilizing the target thread set");
        }
      }
    } catch (...) {
      release_noexcept();
      throw;
    }
  }

  ~PeerFreeze() { release_noexcept(); }

  [[nodiscard]] bool contains_instruction(std::uintptr_t address,
                                          std::size_t size) const {
    const auto end = address + size;
    for (const auto &peer : peers) {
      if (!peer.stopped) {
        continue;
      }
      const auto ip = register_ip(read_registers(peer.tid).general);
      if (ip > address && ip < end) {
        return true;
      }
    }
    const auto leader_ip = process.instruction_pointer();
    return leader_ip > address && leader_ip < end;
  }

  void promote(pid_t tid) {
    const auto selected = find(tid);
    if (selected == peers.end() || !selected->stopped) {
      throw std::logic_error("selected injection thread is not stopped");
    }
    const int selected_signal = selected->pending_signal;
    ptrace_checked(PTRACE_DETACH, process.tid_, nullptr,
                   reinterpret_cast<void *>(static_cast<std::uintptr_t>(
                       process.pending_signal_)),
                   "detach original injection thread after worker selection");
    process.tid_ = tid;
    process.pending_signal_ = selected_signal;
    process.tracee_stopped_ = true;
    selected->seized = false;
    forget(tid);
  }

  void release() {
    if (released) {
      return;
    }
    std::exception_ptr failure;
    for (auto it = peers.rbegin(); it != peers.rend(); ++it) {
      if (!it->seized) {
        continue;
      }
      if (process.poisoned_ && it->pending_signal != 0 &&
          it->pending_signal != SIGSTOP &&
          ::syscall(SYS_tgkill, process.process_id_, it->tid,
                    it->pending_signal) !=
              0 &&
          !(errno == ESRCH &&
            !thread_exists(process.process_id_, it->tid))) {
        if (!failure) {
          try {
            throw_errno("preserve peer signal before poisoned detach");
          } catch (...) {
            failure = std::current_exception();
          }
        }
        continue;
      }
      const int detach_signal =
          process.poisoned_ ? SIGSTOP : it->pending_signal;
      errno = 0;
      if (::ptrace(PTRACE_DETACH, it->tid, nullptr,
                   reinterpret_cast<void *>(
                       static_cast<std::uintptr_t>(detach_signal))) == -1) {
        if (errno == ESRCH || errno == ECHILD) {
          if (!thread_exists(process.process_id_, it->tid)) {
            it->seized = false;
            forget(it->tid);
            continue;
          }
        }
        if (!failure) {
          const int first_error = errno;
          // A failed detach can mean the task has not reached a ptrace stop.
          // Stop it explicitly and retry while retaining ownership on failure.
          if (it->seized &&
              ::ptrace(PTRACE_INTERRUPT, it->tid, nullptr, nullptr) == 0) {
            try {
              wait_peer(*it, std::chrono::steady_clock::now() + kStopTimeout,
                        process.process_id_);
            } catch (...) {
            }
            update_signal(it->tid, it->pending_signal);
          }
          errno = 0;
          if (::ptrace(PTRACE_DETACH, it->tid, nullptr,
                       reinterpret_cast<void *>(static_cast<std::uintptr_t>(
                           process.poisoned_ ? SIGSTOP
                                             : it->pending_signal))) == 0) {
            it->seized = false;
            forget(it->tid);
          } else if ((errno == ESRCH || errno == ECHILD) &&
                     !thread_exists(process.process_id_, it->tid)) {
            it->seized = false;
            forget(it->tid);
          } else {
            errno = first_error;
            try {
              throw_errno("detach peer thread after freeze");
            } catch (...) {
              failure = std::current_exception();
            }
          }
        }
      } else {
        it->seized = false;
        forget(it->tid);
      }
    }
    released = true;
    if (failure) {
      process.poison_process();
      std::rethrow_exception(failure);
    }
  }

private:
  [[nodiscard]] PeerIterator find(pid_t tid) {
    return std::find_if(peers.begin(), peers.end(), [tid](const Peer &peer) {
      return peer.tid == tid && peer.seized;
    });
  }

  void update_signal(pid_t tid, int signal) {
    for (auto &entry : process.traced_peers_) {
      if (entry.first == tid) {
        entry.second = signal;
        return;
      }
    }
  }

  void forget(pid_t tid) {
    auto &owned = process.traced_peers_;
    owned.erase(
        std::remove_if(owned.begin(), owned.end(),
                       [tid](const auto &entry) { return entry.first == tid; }),
        owned.end());
  }

  static void wait_peer(Peer &peer,
                        std::chrono::steady_clock::time_point deadline,
                        pid_t process_id) {
    for (;;) {
      int status = 0;
      const pid_t result = ::waitpid(peer.tid, &status, __WALL | WNOHANG);
      if (result == peer.tid) {
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
          peer.seized = false;
          return;
        }
        if (!WIFSTOPPED(status)) {
          throw std::runtime_error("peer thread entered an unsupported state");
        }
        const unsigned int event = static_cast<unsigned int>(status) >> 16;
        const int signal = WSTOPSIG(status);
        if (event == PTRACE_EVENT_STOP && signal == SIGTRAP) {
          peer.stopped = true;
          return;
        }
        if (event == PTRACE_EVENT_STOP) {
          peer.pending_signal = signal;
          throw IdlePointUnavailable(
              "peer thread is in a job-control group stop; refusing to thaw "
              "or inject through an unsupported stop");
        }
        if (event == 0) {
          peer.pending_signal = signal;
          peer.stopped = true;
          return;
        }
        throw std::runtime_error(
            "peer thread entered an unsupported ptrace event stop");
      }
      if (result == -1 && (errno == ESRCH || errno == ECHILD)) {
        if (!thread_exists(process_id, peer.tid)) {
          peer.seized = false;
          return;
        }
        throw std::runtime_error(
            "waitpid lost a peer that remains in /proc; retaining ptrace "
            "ownership");
      }
      if (result == -1 && errno != EINTR) {
        throw_errno("waitpid for peer thread");
      }
      if (std::chrono::steady_clock::now() >= deadline) {
        throw std::runtime_error("timed out waiting for peer thread stop");
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  void release_noexcept() noexcept {
    if (released) {
      return;
    }
    try {
      release();
    } catch (const std::exception &error) {
      std::fprintf(stderr, "injector: peer thaw failed: %s\n", error.what());
    } catch (...) {
      std::fprintf(stderr, "injector: peer thaw failed\n");
    }
  }
};

Process::~Process() {
  if (attached()) {
    if (poisoned_) {
      const pid_t unsafe_pid = process_id_;
      try {
        preserve_poisoned_stop();
      } catch (const std::exception &error) {
        std::fprintf(stderr,
                     "injector: PID %d unsafe recovery could not be parked "
                     "before tracer exit "
                     "(%s); kernel auto-detach may resume it, final target "
                     "state is uncertain\n",
                     static_cast<int>(unsafe_pid), error.what());
      }
      return;
    }
    try {
      detach();
    } catch (...) {
      // Destructors cannot report errors. Call detach() explicitly when its
      // diagnostic matters; never kill the target as a recovery measure.
    }
  }
}

Process::Process(Process &&other) noexcept
    : process_id_(std::exchange(other.process_id_, -1)),
      tid_(std::exchange(other.tid_, -1)),
      traced_peers_(std::move(other.traced_peers_)),
      pending_signal_(std::exchange(other.pending_signal_, 0)),
      scratch_stack_(std::exchange(other.scratch_stack_, 0)),
      scratch_stack_size_(std::exchange(other.scratch_stack_size_, 0)),
      call_page_(std::exchange(other.call_page_, 0)),
      call_page_size_(std::exchange(other.call_page_size_, 0)),
      poisoned_(std::exchange(other.poisoned_, false)),
      tracee_stopped_(std::exchange(other.tracee_stopped_, false)) {}

Process &Process::operator=(Process &&other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (attached()) {
    if (poisoned_) {
      std::fprintf(stderr,
                   "injector: refusing move assignment over poisoned PID %d; "
                   "it remains ptrace-stopped\n",
                   static_cast<int>(process_id_));
      return *this;
    }
    try {
      detach();
    } catch (const std::exception &error) {
      std::fprintf(
          stderr,
          "injector: cannot release PID %d during move assignment: %s\n",
          static_cast<int>(process_id_), error.what());
      return *this;
    } catch (...) {
      std::fprintf(stderr,
                   "injector: cannot release PID %d during move assignment; "
                   "ownership retained\n",
                   static_cast<int>(process_id_));
      return *this;
    }
  }
  process_id_ = std::exchange(other.process_id_, -1);
  tid_ = std::exchange(other.tid_, -1);
  traced_peers_ = std::move(other.traced_peers_);
  pending_signal_ = std::exchange(other.pending_signal_, 0);
  scratch_stack_ = std::exchange(other.scratch_stack_, 0);
  scratch_stack_size_ = std::exchange(other.scratch_stack_size_, 0);
  call_page_ = std::exchange(other.call_page_, 0);
  call_page_size_ = std::exchange(other.call_page_size_, 0);
  poisoned_ = std::exchange(other.poisoned_, false);
  tracee_stopped_ = std::exchange(other.tracee_stopped_, false);
  return *this;
}

void Process::ensure_attached() const {
  if (!attached()) {
    throw std::logic_error("injector is not attached to a process");
  }
}

void Process::ensure_usable() const {
  ensure_attached();
  if (poisoned_) {
    throw std::logic_error(
        "injector target is poisoned and may only be preserved or detached");
  }
}

void Process::poison_process() noexcept {
  poisoned_ = true;
  if (process_id_ > 0 && ::kill(process_id_, SIGSTOP) != 0 && errno != ESRCH) {
    std::fprintf(stderr,
                 "injector: could not request SIGSTOP for unsafe PID %d: %s\n",
                 static_cast<int>(process_id_), std::strerror(errno));
  }
}

void Process::mark_unsafe() noexcept { poison_process(); }

void Process::attach(pid_t process_id,
                     ::tracer::compat::span<const ApplicationModule> application_modules,
                     pid_t excluded_tid, pid_t required_tid) {
#if !defined(__linux__) || !defined(__x86_64__)
  (void)process_id;
  throw std::runtime_error("native injector is supported only on Linux x86_64");
#else
  if (attached()) {
    throw std::logic_error("injector is already attached");
  }
  if (process_id <= 0) {
    throw std::invalid_argument("process id must be positive");
  }
  verify_target_executable(process_id);
  if (::ptrace(PTRACE_SEIZE, process_id, nullptr, nullptr) == -1) {
    throw_errno("PTRACE_SEIZE (check ptrace permissions and target ownership)");
  }
  process_id_ = process_id;
  tid_ = process_id;
  bool stopped = false;
  try {
    ptrace_checked(PTRACE_INTERRUPT, tid_, nullptr, nullptr,
                   "PTRACE_INTERRUPT");
    int status = 0;
    if (!wait_stopped(tid_, std::chrono::steady_clock::now() + kStopTimeout,
                      status)) {
      throw std::runtime_error("timed out waiting for PTRACE_INTERRUPT stop");
    }
    if (!WIFSTOPPED(status)) {
      throw std::runtime_error("target exited during ptrace attach");
    }
    stopped = true;
    tracee_stopped_ = true;
    const unsigned int event = static_cast<unsigned int>(status) >> 16;
    if (event == PTRACE_EVENT_STOP && WSTOPSIG(status) != SIGTRAP) {
      pending_signal_ = WSTOPSIG(status);
      throw IdlePointUnavailable(
          "target leader is in a job-control group stop; refusing injection");
    }
    if (event != PTRACE_EVENT_STOP) {
      // Preserve a genuine signal-delivery stop if one raced with interrupt.
      pending_signal_ = WSTOPSIG(status);
    }
    validate_application_modules(process_id_, application_modules);
    struct Rejection {
      pid_t tid;
      long syscall_number;
      std::string reason;
    };
    std::vector<Rejection> rejected;
    auto eligible = [&](pid_t candidate_tid, int pending_signal) {
      if (candidate_tid == excluded_tid || (required_tid && candidate_tid != required_tid)) {
        rejected.push_back({candidate_tid, -1, "reserved for instruction tracing"});
        return false;
      }
      if (required_tid && pending_signal) {
        rejected.push_back({candidate_tid, -1, "selected control thread has a pending signal"});
        return false;
      }
      const long syscall_number = stopped_syscall(candidate_tid);
      const auto registers = read_registers(candidate_tid).general;
      if (candidate_tid != process_id_ && registers.fs_base == 0) {
        rejected.push_back({candidate_tid, syscall_number,
                            "worker has no thread pointer for dlopen"});
        return false;
      }
      if (syscall_number == SYS_nanosleep ||
          syscall_number == SYS_clock_nanosleep) {
        return true;
      }
      if (required_tid) {
        rejected.push_back({candidate_tid, syscall_number, "selected control thread must be sleeping"});
        return false;
      }
      if (syscall_number >= 0) {
        rejected.push_back({candidate_tid, syscall_number,
                            "syscall is outside the approved sleep policy"});
        return false;
      }
      try {
        (void)require_application_rendezvous(
            process_id_, registers, application_modules);
        return true;
      } catch (const IdlePointUnavailable &error) {
        rejected.push_back({candidate_tid, syscall_number, error.what()});
        return false;
      }
    };

    if (!eligible(tid_, pending_signal_)) {
      PeerFreeze freeze(*this);
      pid_t selected_tid = -1;
      for (const auto &peer : freeze.peers) {
        if (peer.stopped && eligible(peer.tid, peer.pending_signal)) {
          selected_tid = peer.tid;
          break;
        }
      }
      if (selected_tid < 0) {
        std::ostringstream message;
        message << "no eligible injection thread; blocked TIDs/syscalls:";
        std::vector<std::string> rejection_lines;
        rejection_lines.reserve(rejected.size());
        for (const auto &rejection : rejected) {
          std::ostringstream line;
          line << "tid=" << rejection.tid << " syscall="
               << rejection.syscall_number << "(";
          if (rejection.syscall_number < 0) {
            line << "userspace";
          } else {
            line << syscall_name(rejection.syscall_number);
          }
          line << ") reason=" << rejection.reason;
          rejection_lines.push_back(line.str());
        }
        message << " " << join_rejections(rejection_lines);
        freeze.release();
        throw IdlePointUnavailable(message.str());
      }
      freeze.promote(selected_tid);
      freeze.release();
    }
  } catch (...) {
    const auto original_failure = std::current_exception();
    if (!stopped) {
      // A timeout does not prove the tracee stopped. Keep ownership if it
      // cannot be stopped; detaching a running tracee would be unsafe.
      (void)::ptrace(PTRACE_INTERRUPT, tid_, nullptr, nullptr);
      int recovery_status = 0;
      stopped =
          wait_stopped(tid_, std::chrono::steady_clock::now() + kStopTimeout,
                       recovery_status);
      tracee_stopped_ = stopped;
      if (stopped && ((static_cast<unsigned int>(recovery_status) >> 16) !=
                          PTRACE_EVENT_STOP ||
                      WSTOPSIG(recovery_status) != SIGTRAP)) {
        pending_signal_ = WSTOPSIG(recovery_status);
      }
    }
    if (poisoned_ || !traced_peers_.empty()) {
      poison_process();
      throw std::runtime_error(
          "attach failed and peer cleanup is uncertain; injector retains "
          "ptrace ownership");
    }
    if (!stopped) {
      poison_process();
      throw std::runtime_error(
          "attach failed and the tracee could not be confirmed stopped; "
          "injector retains ptrace "
          "ownership; do not resume or kill the target blindly");
    }
    try {
      ptrace_checked(PTRACE_DETACH, tid_, nullptr,
                     reinterpret_cast<void *>(
                         static_cast<std::uintptr_t>(pending_signal_)),
                     "detach after failed attach");
    } catch (const std::exception &error) {
      poison_process();
      throw std::runtime_error(
          std::string("attach failed and cleanup could not detach: ") +
          error.what() + "; injector retains ptrace ownership");
    }
    tid_ = -1;
    process_id_ = -1;
    pending_signal_ = 0;
    tracee_stopped_ = false;
    std::rethrow_exception(original_failure);
  }
#endif
}

void Process::detach() {
  ensure_attached();
  if (poisoned_) {
    preserve_poisoned_stop();
    return;
  }
  if (call_page_ != 0) {
    remote_free(call_page_, call_page_size_);
  }
  if (scratch_stack_ != 0) {
    remote_free(scratch_stack_, scratch_stack_size_);
  }
  const pid_t tid = tid_;
  ptrace_checked(
      PTRACE_DETACH, tid, nullptr,
      reinterpret_cast<void *>(static_cast<std::uintptr_t>(pending_signal_)),
      "PTRACE_DETACH");
  tid_ = -1;
  process_id_ = -1;
  pending_signal_ = 0;
  tracee_stopped_ = false;
  scratch_stack_ = 0;
  scratch_stack_size_ = 0;
  call_page_ = 0;
  call_page_size_ = 0;
}

void Process::preserve_poisoned_stop() {
  ensure_attached();
  const pid_t unsafe_pid = process_id_;
  const pid_t unsafe_tid = tid_;
  if (!tracee_stopped_) {
    ptrace_checked(PTRACE_INTERRUPT, unsafe_tid, nullptr, nullptr,
                   "interrupt unsafe tracee before preserving stop");
    wait_stopped_or_throw(unsafe_tid,
                          std::chrono::steady_clock::now() + kStopTimeout,
                          "stop unsafe tracee before detach");
    tracee_stopped_ = true;
  }
  if (::kill(unsafe_pid, SIGSTOP) != 0) {
    throw_errno("request SIGSTOP for unsafe tracee");
  }
  for (const auto &[peer_tid, signal] : traced_peers_) {
    user_regs_struct peer_registers{};
    const bool already_stopped =
        ::ptrace(PTRACE_GETREGS, peer_tid, nullptr, &peer_registers) == 0;
    if (!already_stopped &&
        ::ptrace(PTRACE_INTERRUPT, peer_tid, nullptr, nullptr) == -1) {
      if ((errno == ESRCH || errno == ECHILD) &&
          !thread_exists(unsafe_pid, peer_tid)) {
        continue;
      }
      throw_errno("interrupt peer while preserving poisoned process stop");
    }
    if (!already_stopped) {
      wait_stopped_or_throw(peer_tid,
                            std::chrono::steady_clock::now() + kStopTimeout,
                            "stop peer while preserving poisoned process");
    }
    if (signal != 0 && signal != SIGSTOP &&
        ::syscall(SYS_tgkill, unsafe_pid, peer_tid, signal) != 0 &&
        !(errno == ESRCH && !thread_exists(unsafe_pid, peer_tid))) {
      throw_errno("preserve peer signal during poisoned stop");
    }
    errno = 0;
    if (::ptrace(PTRACE_DETACH, peer_tid, nullptr,
                 reinterpret_cast<void *>(
                     static_cast<std::uintptr_t>(SIGSTOP))) == -1) {
      if ((errno == ESRCH || errno == ECHILD) &&
          !thread_exists(unsafe_pid, peer_tid)) {
        continue;
      }
      throw_errno("detach peer while preserving poisoned process stop");
    }
  }
  traced_peers_.clear();
  if (pending_signal_ != 0 && pending_signal_ != SIGSTOP &&
      ::syscall(SYS_tgkill, unsafe_pid, unsafe_tid, pending_signal_) != 0) {
    throw_errno("preserve leader signal during poisoned stop");
  }
  ptrace_checked(PTRACE_DETACH, unsafe_tid, nullptr,
                 reinterpret_cast<void *>(static_cast<std::uintptr_t>(SIGSTOP)),
                 "detach poisoned tracee while delivering SIGSTOP");
  tid_ = -1;
  process_id_ = -1;
  pending_signal_ = 0;
  tracee_stopped_ = false;
  scratch_stack_ = 0;
  scratch_stack_size_ = 0;
  call_page_ = 0;
  call_page_size_ = 0;
  poisoned_ = false;
  std::fprintf(
      stderr,
      "injector: PID %d detached with SIGSTOP delivered after unsafe recovery; "
      "target is left stopped until resumed\n",
      static_cast<int>(unsafe_pid));
}

std::uintptr_t Process::instruction_pointer() const {
  ensure_usable();
  return register_ip(read_registers(tid_).general);
}

bool Process::signal_blocked(int signal) const {
  ensure_usable();
  return signal > 0 && signal <= 64 &&
         (read_signal_mask(tid_) & (std::uint64_t{1} << (signal - 1)));
}

void Process::remote_read(std::uintptr_t address, void *destination,
                          std::size_t size) const {
  ensure_usable();
  if (destination == nullptr && size != 0) {
    throw std::invalid_argument("remote_read destination is null");
  }
  const auto data = read_remote(tid_, address, size);
  std::memcpy(destination, data.data(), size);
}

void Process::remote_write(std::uintptr_t address, const void *source,
                           std::size_t size) {
  ensure_usable();
  if (source == nullptr && size != 0) {
    throw std::invalid_argument("remote_write source is null");
  }
  if (size == 0) {
    return;
  }
  const auto expected = read_remote(tid_, address, size);
  if (!patch_code(address, expected.data(), source, size)) {
    throw std::runtime_error("remote_write range contains a thread instruction "
                             "pointer; retry later");
  }
}

bool Process::patch_code(std::uintptr_t address, const void *expected,
                         const void *replacement, std::size_t size) {
  ensure_usable();
  if (address == 0 || size == 0 || expected == nullptr ||
      replacement == nullptr ||
      size > std::numeric_limits<std::uintptr_t>::max() - address) {
    throw std::invalid_argument("patch_code requires a valid nonempty range");
  }

  PeerFreeze peers(*this);
  if (peers.contains_instruction(address, size)) {
    peers.release();
    return false;
  }

  const auto actual = read_remote(tid_, address, size);
  if (std::memcmp(actual.data(), expected, size) != 0) {
    throw std::runtime_error(
        "code patch precondition did not match target bytes");
  }
  try {
    write_remote(tid_, address, replacement, size);
    const auto committed = read_remote(tid_, address, size);
    if (std::memcmp(committed.data(), replacement, size) != 0) {
      throw std::runtime_error("code patch verification failed");
    }
  } catch (const std::exception &write_error) {
    try {
      write_remote(tid_, address, expected, size);
      const auto restored = read_remote(tid_, address, size);
      if (std::memcmp(restored.data(), expected, size) != 0) {
        throw std::runtime_error("rollback byte verification failed");
      }
    } catch (const std::exception &rollback_error) {
      poison_process();
      throw std::runtime_error(
          std::string("code patch failed (") + write_error.what() +
          ") and rollback is uncertain (" + rollback_error.what() +
          "); target process was stopped for recovery");
    }
    throw;
  } catch (...) {
    poison_process();
    throw std::runtime_error(
        "code patch failed with an unknown error; target process was stopped "
        "for recovery");
  }
  peers.release();
  return true;
}

bool Process::patch_code_batch(::tracer::compat::span<const CodePatch> patches,
                               const std::function<void()> &verify) {
  ensure_usable();
  if (patches.empty())
    return true;
  std::vector<std::size_t> order(patches.size());
  for (std::size_t i = 0; i < patches.size(); ++i) {
    const auto &patch = patches[i];
    if (patch.address == 0 || patch.size == 0 || !patch.expected ||
        !patch.replacement ||
        patch.size > std::numeric_limits<std::uintptr_t>::max() - patch.address)
      throw std::invalid_argument("patch_code_batch contains an invalid range");
    order[i] = i;
  }
  std::sort(order.begin(), order.end(),
            [&](std::size_t left, std::size_t right) {
              return patches[left].address < patches[right].address;
            });
  for (std::size_t i = 1; i < order.size(); ++i) {
    const auto &prior = patches[order[i - 1]];
    const auto &next = patches[order[i]];
    if (prior.address + prior.size > next.address)
      throw std::invalid_argument("patch_code_batch ranges overlap");
  }

  PeerFreeze peers(*this);
  if (verify)
    verify();
  for (const auto &patch : patches) {
    if (peers.contains_instruction(patch.address, patch.size)) {
      peers.release();
      return false;
    }
  }

  std::vector<std::vector<unsigned char>> before;
  before.reserve(patches.size());
  for (const auto &patch : patches) {
    before.push_back(read_remote(tid_, patch.address, patch.size));
    if (std::memcmp(before.back().data(), patch.expected, patch.size) != 0)
      throw std::runtime_error(
          "patch_code_batch precondition did not match target bytes");
  }

  std::size_t attempted = 0;
  try {
    for (std::size_t i = 0; i < patches.size(); ++i) {
      attempted = i + 1; // write_remote can fail after a partial word write.
      write_remote(tid_, patches[i].address, patches[i].replacement,
                   patches[i].size);
      const auto committed =
          read_remote(tid_, patches[i].address, patches[i].size);
      if (std::memcmp(committed.data(), patches[i].replacement,
                      patches[i].size) != 0)
        throw std::runtime_error("patch_code_batch verification failed");
    }
  } catch (...) {
    const auto failure = std::current_exception();
    try {
      while (attempted != 0) {
        const auto i = --attempted;
        write_remote(tid_, patches[i].address, before[i].data(),
                     before[i].size());
        const auto restored =
            read_remote(tid_, patches[i].address, before[i].size());
        if (std::memcmp(restored.data(), before[i].data(), before[i].size()) !=
            0)
          throw std::runtime_error(
              "patch_code_batch rollback verification failed");
      }
    } catch (const std::exception &rollback_error) {
      poison_process();
      throw std::runtime_error(
          std::string("patch batch failed and rollback is uncertain: ") +
          rollback_error.what() + "; target process was stopped for recovery");
    } catch (...) {
      poison_process();
      throw std::runtime_error("patch batch failed and rollback is uncertain; "
                               "target process was stopped for recovery");
    }
    std::rethrow_exception(failure);
  }
  peers.release();
  return true;
}

bool Process::retired_code_quiescent(const TracerRetiredCode &code) {
  ensure_usable();
  for (const auto &range : code.ranges)
    if (!range.begin || range.begin >= range.end)
      return false;
  PeerFreeze peers(*this);
  std::vector<std::pair<std::uintptr_t, std::uintptr_t>> readable;
  std::ifstream maps("/proc/" + std::to_string(process_id_) + "/maps");
  for (std::string line; std::getline(maps, line);) {
    std::istringstream row(line);
    std::uintptr_t begin = 0, end = 0;
    char dash = 0;
    std::string permissions;
    if (row >> std::hex >> begin >> dash >> end >> permissions &&
        dash == '-' && !permissions.empty() && permissions.front() == 'r')
      readable.emplace_back(begin, end);
  }
  const auto references = [&](const void *data, std::size_t size) {
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t i = 0; i + sizeof(std::uintptr_t) <= size; i += sizeof(std::uintptr_t)) {
      std::uintptr_t value = 0;
      std::memcpy(&value, bytes + i, sizeof(value));
      for (const auto &range : code.ranges)
        if (value >= range.begin && value < range.end)
          return true;
    }
    return false;
  };
  const auto clean = [&](pid_t tid, int pending) {
    if (pending)
      return false;
    const auto registers = read_registers(tid);
    if (references(&registers.general, sizeof(registers.general)) ||
        references(registers.xstate.data(), registers.xstate_size))
      return false;
    const auto sp = static_cast<std::uintptr_t>(registers.general.rsp) & ~std::uintptr_t{7};
    const auto map = std::find_if(readable.begin(), readable.end(), [&](const auto &range) {
      return range.first <= sp && sp < range.second;
    });
    if (map == readable.end() || map->second - sp > 128 * 1024)
      return false;
    const auto start = std::max(map->first, sp >= 128 ? sp - 128 : sp);
    std::vector<unsigned char> stack(map->second - start);
    iovec local{stack.data(), stack.size()};
    iovec remote{reinterpret_cast<void *>(start), stack.size()};
    return linux_api::process_vm_readv(process_id_, &local, 1, &remote, 1, 0) ==
               static_cast<ssize_t>(stack.size()) && !references(stack.data(), stack.size());
  };
  const bool result = clean(tid_, pending_signal_) &&
      std::all_of(peers.peers.begin(), peers.peers.end(), [&](const auto &peer) {
        return !peer.stopped || clean(peer.tid, peer.pending_signal);
      });
  peers.release();
  return result;
}

void Process::remote_free(std::uintptr_t address, std::size_t size) {
  ensure_usable();
  if (address == 0 || size == 0) {
    throw std::invalid_argument(
        "remote_free requires a nonzero address and size");
  }
  const std::array<std::uintptr_t, 6> arguments{address, size, 0, 0, 0, 0};
  if (remote_syscall(SYS_munmap, arguments) != 0) {
    throw std::runtime_error(
        "remote munmap returned an unexpected nonzero result");
  }
  if (address == call_page_ && size == call_page_size_) {
    call_page_ = 0;
    call_page_size_ = 0;
  }
  if (address == scratch_stack_ && size == scratch_stack_size_) {
    scratch_stack_ = 0;
    scratch_stack_size_ = 0;
  }
}

std::uintptr_t Process::remote_allocate(std::size_t size) {
  ensure_usable();
  if (size == 0) {
    throw std::invalid_argument("remote allocation size must be nonzero");
  }
  const std::array<std::uintptr_t, 6> arguments{0,
                                                size,
                                                PROT_READ | PROT_WRITE,
                                                MAP_PRIVATE | MAP_ANONYMOUS,
                                                static_cast<std::uintptr_t>(-1),
                                                0};
  return remote_syscall(SYS_mmap, arguments);
}

std::uintptr_t
Process::remote_syscall(long number,
                        const std::array<std::uintptr_t, 6> &arguments) {
  ensure_usable();
  PeerFreeze peers(*this);
  const auto saved = read_registers(tid_);
  const auto saved_signal_mask = read_signal_mask(tid_);
  const auto ip = register_ip(saved.general);
  constexpr std::array<unsigned char, 3> stub{0x0f, 0x05,
                                              0xcc}; // syscall; int3
  if (peers.contains_instruction(ip, stub.size())) {
    throw std::runtime_error("cannot install native syscall stub while a "
                             "thread is inside its bytes");
  }
  const auto original = read_remote(tid_, ip, stub.size());
  auto modified = saved;
  modified.general.rax = static_cast<unsigned long long>(number);
  modified.general.orig_rax = static_cast<unsigned long long>(-1);
  modified.general.rdi = arguments[0];
  modified.general.rsi = arguments[1];
  modified.general.rdx = arguments[2];
  modified.general.r10 = arguments[3];
  modified.general.r8 = arguments[4];
  modified.general.r9 = arguments[5];

  bool code_changed = false;
  bool execution_started = false;
  bool tracee_stopped = true;
  bool signal_mask_blocked = false;
  bool timed_out = false;
  std::exception_ptr failure;
  std::uintptr_t result = 0;
  try {
    code_changed = true;
    write_remote(tid_, ip, stub.data(), stub.size());
    write_registers(tid_, modified);
    write_signal_mask(tid_, ~std::uint64_t{0});
    signal_mask_blocked = true;
    ptrace_checked(PTRACE_CONT, tid_, nullptr, nullptr,
                   "resume tracee for native syscall");
    execution_started = true;
    tracee_stopped = false;
    tracee_stopped_ = false;
    int status = 0;
    if (!wait_stopped(tid_, std::chrono::steady_clock::now() + kStopTimeout,
                      status)) {
      timed_out = true;
      poison_process();
      ptrace_checked(PTRACE_INTERRUPT, tid_, nullptr, nullptr,
                     "interrupt timed out native syscall");
      wait_stopped_or_throw(tid_,
                            std::chrono::steady_clock::now() + kStopTimeout,
                            "recover native syscall");
      tracee_stopped = true;
      tracee_stopped_ = true;
      throw std::runtime_error(
          "native syscall timed out; tracee was stopped before recovery");
    }
    tracee_stopped = true;
    tracee_stopped_ = true;
    const auto stop = inspect_injected_stop(process_id_, tid_, status,
                                           ip + stub.size(), pending_signal_);
    if (stop.kind != InjectedStopKind::completion) {
      throw std::runtime_error(
          "native syscall stopped on signal " + std::to_string(stop.signal) +
          (stop.kind == InjectedStopKind::confirmed_fault
               ? " (confirmed injected fault; signal delivery suppressed)"
               : ""));
    }
    result = stop.registers.rax;
    if (static_cast<long long>(result) < 0 &&
        static_cast<long long>(result) >= -4095) {
      throw std::runtime_error("remote syscall failed with errno " +
                               std::to_string(-static_cast<long long>(result)));
    }
  } catch (...) {
    failure = std::current_exception();
    if (execution_started && !tracee_stopped) {
      // A failed wait or signal may leave the tracee running. Stop it before
      // touching its code/register state; do not resume a partially restored
      // call.
      try {
        ptrace_checked(PTRACE_INTERRUPT, tid_, nullptr, nullptr,
                       "interrupt tracee for native-syscall recovery");
        wait_stopped_or_throw(tid_,
                              std::chrono::steady_clock::now() + kStopTimeout,
                              "stop tracee for native-syscall recovery");
        tracee_stopped = true;
        tracee_stopped_ = true;
      } catch (...) {
        poison_process();
        throw std::runtime_error(
            "native-syscall recovery could not confirm a stopped tracee; "
            "code/register state may be modified; "
            "do not resume or detach it blindly");
      }
    }
  }
  if (timed_out) {
    std::rethrow_exception(failure);
  }
  try {
    if (signal_mask_blocked) {
      write_signal_mask(tid_, saved_signal_mask);
    }
    if (code_changed) {
      write_remote(tid_, ip, original.data(), original.size());
    }
    write_registers(tid_, saved);
  } catch (const std::exception &error) {
    poison_process();
    throw std::runtime_error(
        std::string("failed to restore tracee after native syscall: ") +
        error.what() +
        "; tracee remains ptrace-stopped, do not resume it blindly");
  }
  peers.release();
  if (failure) {
    std::rethrow_exception(failure);
  }
  return result;
}

std::uintptr_t
Process::remote_call(std::uintptr_t function,
                     const std::array<std::uintptr_t, 6> &arguments,
                     std::size_t argument_count) {
  ensure_usable();
  if (function == 0) {
    throw std::invalid_argument("remote function address is null");
  }
  if (argument_count > arguments.size()) {
    throw std::invalid_argument(
        "remote_call supports up to six integer/pointer arguments");
  }
  const auto errno_location = resolve_remote_symbol("__errno_location");
  const auto remote_errno_address = remote_call_raw(errno_location, {}, 0);
  if (remote_errno_address == 0) {
    throw std::runtime_error("remote __errno_location returned null");
  }
  int saved_errno = 0;
  remote_read(remote_errno_address, &saved_errno, sizeof(saved_errno));

  std::uintptr_t result = 0;
  try {
    result = remote_call_raw(function, arguments, argument_count);
  } catch (...) {
    // The callee may have changed errno even when the raw call reports a
    // recovery error. Keep the target stopped rather than expose that state.
    poison_process();
    throw;
  }
  try {
    remote_write(remote_errno_address, &saved_errno, sizeof(saved_errno));
  } catch (...) {
    // The callee completed, so a failed errno write leaves target TLS with an
    // unknown value. Preserve the stop instead of letting the target resume.
    poison_process();
    throw;
  }
  return result;
}

std::uintptr_t
Process::remote_call_raw(std::uintptr_t function,
                         const std::array<std::uintptr_t, 6> &arguments,
                         std::size_t argument_count) {
  ensure_usable();
  if (function == 0) {
    throw std::invalid_argument("remote function address is null");
  }
  if (argument_count > arguments.size()) {
    throw std::invalid_argument(
        "remote_call supports up to six integer/pointer arguments");
  }
  if (scratch_stack_ == 0) {
    scratch_stack_ = remote_allocate(kRemoteStackSize);
    scratch_stack_size_ = kRemoteStackSize;
  }
  if (call_page_ == 0) {
    const long page_size = ::sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
      throw std::runtime_error("cannot determine target page size");
    }
    call_page_size_ = static_cast<std::size_t>(page_size);
    call_page_ = remote_allocate(call_page_size_);
    constexpr std::array<unsigned char, 3> stub{0xff, 0xd0,
                                                0xcc}; // call rax; int3
    constexpr std::array<unsigned char, 3> zero_bytes{};
    if (!patch_code(call_page_, zero_bytes.data(), stub.data(), stub.size())) {
      throw std::runtime_error(
          "call page instruction pointer was occupied during initialization");
    }
    const std::array<std::uintptr_t, 6> protect_arguments{
        call_page_, call_page_size_, PROT_READ | PROT_EXEC, 0, 0, 0};
    if (remote_syscall(SYS_mprotect, protect_arguments) != 0) {
      throw std::runtime_error(
          "remote mprotect failed while preparing call page");
    }
  }

  const auto saved = read_registers(tid_);
  const auto saved_signal_mask = read_signal_mask(tid_);
  auto modified = saved;
  modified.general.rip = call_page_;
  modified.general.rax = function;
  modified.general.orig_rax = static_cast<unsigned long long>(-1);
  modified.general.eflags &= ~(kX86TraceFlag | kX86DirectionFlag);
  modified.general.rdi = argument_count > 0 ? arguments[0] : 0;
  modified.general.rsi = argument_count > 1 ? arguments[1] : 0;
  modified.general.rdx = argument_count > 2 ? arguments[2] : 0;
  modified.general.rcx = argument_count > 3 ? arguments[3] : 0;
  modified.general.r8 = argument_count > 4 ? arguments[4] : 0;
  modified.general.r9 = argument_count > 5 ? arguments[5] : 0;
  modified.general.rsp =
      (scratch_stack_ + scratch_stack_size_) & ~std::uintptr_t{15};

  bool execution_started = false;
  bool tracee_stopped = true;
  bool signal_mask_blocked = false;
  bool timed_out = false;
  std::exception_ptr failure;
  std::uintptr_t result = 0;
  try {
    write_registers(tid_, modified);
    write_signal_mask(tid_, ~std::uint64_t{0});
    signal_mask_blocked = true;
    ptrace_checked(PTRACE_CONT, tid_, nullptr, nullptr,
                   "resume tracee for remote call");
    execution_started = true;
    tracee_stopped = false;
    tracee_stopped_ = false;
    int status = 0;
    if (!wait_stopped(tid_, std::chrono::steady_clock::now() + kCallTimeout,
                      status)) {
      timed_out = true;
      poison_process();
      ptrace_checked(PTRACE_INTERRUPT, tid_, nullptr, nullptr,
                     "interrupt timed out remote call");
      wait_stopped_or_throw(tid_,
                            std::chrono::steady_clock::now() + kStopTimeout,
                            "recover remote call");
      tracee_stopped = true;
      tracee_stopped_ = true;
      throw std::runtime_error(
          "remote call timed out and was interrupted; the callee may have left "
          "locks or other "
          "side effects active, so resuming the target may be unsafe");
    }
    tracee_stopped = true;
    tracee_stopped_ = true;
    const auto stop = inspect_injected_stop(process_id_, tid_, status,
                                           call_page_ + 3, pending_signal_);
    if (stop.kind != InjectedStopKind::completion) {
      throw std::runtime_error(
          "remote call stopped on signal " + std::to_string(stop.signal) +
          " at PC " + std::to_string(stop.registers.rip) +
          " before its completion; the callee may have side effects" +
          (stop.kind == InjectedStopKind::confirmed_fault
               ? "; confirmed injected fault, signal delivery suppressed"
               : ""));
    }
    result = stop.registers.rax;
  } catch (...) {
    failure = std::current_exception();
    if (execution_started && !tracee_stopped) {
      try {
        ptrace_checked(PTRACE_INTERRUPT, tid_, nullptr, nullptr,
                       "interrupt tracee for remote-call recovery");
        wait_stopped_or_throw(tid_,
                              std::chrono::steady_clock::now() + kStopTimeout,
                              "stop tracee for remote-call recovery");
        tracee_stopped = true;
        tracee_stopped_ = true;
      } catch (...) {
        poison_process();
        throw std::runtime_error(
            "remote-call recovery could not confirm a stopped tracee; "
            "code/register state may be "
            "modified; do not resume or detach it blindly");
      }
    }
  }
  if (timed_out) {
    std::rethrow_exception(failure);
  }
  try {
    if (signal_mask_blocked) {
      write_signal_mask(tid_, saved_signal_mask);
    }
    write_registers(tid_, saved);
  } catch (const std::exception &error) {
    poison_process();
    throw std::runtime_error(
        std::string("failed to restore tracee after remote call: ") +
        error.what() +
        "; tracee remains ptrace-stopped, do not resume it blindly");
  }
  if (failure) {
    if (execution_started) {
      poison_process();
    }
    std::rethrow_exception(failure);
  }
  return result;
}

std::uintptr_t Process::resolve_remote_symbol(std::string_view name, const CodePatch* owned_patch) {
  ensure_usable();
  if (name.empty() || name.find('\0') != std::string_view::npos) {
    throw std::invalid_argument(
        "symbol name must be nonempty and contain no NUL byte");
  }
  const auto entry = target_runtime_entry(process_id_, name);
  decltype(entry.bytes) live{};
  remote_read(entry.address, live.data(), live.size());
  if (owned_patch) {
    // A second shared probe sees this session's installed pthread adapter.
    // Verify its live bytes before substituting the original entry for comparison.
    if (name != "pthread_create" || !owned_patch->expected || !owned_patch->replacement ||
        owned_patch->address < entry.address || owned_patch->address - entry.address > 4 ||
        !owned_patch->size ||
        owned_patch->size > live.size() - (owned_patch->address - entry.address)) {
      throw std::runtime_error("invalid owned runtime patch identity");
    }
    const auto offset = owned_patch->address - entry.address;
    if (std::memcmp(live.data() + offset, owned_patch->replacement, owned_patch->size) != 0) {
      throw std::runtime_error("owned runtime patch bytes changed");
    }
    std::memcpy(live.data() + offset, owned_patch->expected, owned_patch->size);
  }
  if (live != entry.bytes) {
    throw std::runtime_error("runtime entry instructions do not match");
  }
  return entry.address;
}

} // namespace tracer::injector

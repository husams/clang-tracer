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
#include <iomanip>
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
#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#include "admission.hpp"

#ifndef NT_X86_XSTATE
#define NT_X86_XSTATE 0x202
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

long stopped_syscall(pid_t tid) {
  std::ifstream input("/proc/" + std::to_string(tid) + "/syscall");
  if (!input) {
    throw std::runtime_error(
        "cannot inspect /proc/<pid>/syscall; refusing an unknown stop");
  }
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

struct MapEntry {
  std::uintptr_t start = 0;
  std::uintptr_t end = 0;
  std::uintptr_t offset = 0;
  unsigned int device_major = 0;
  unsigned int device_minor = 0;
  std::uint64_t inode = 0;
  std::string path;
};

std::vector<MapEntry> read_maps(pid_t process_id) {
  std::ifstream input("/proc/" + std::to_string(process_id) + "/maps");
  if (!input) {
    throw std::runtime_error("cannot read /proc/<pid>/maps");
  }
  std::vector<MapEntry> maps;
  std::string line;
  while (std::getline(input, line)) {
    std::istringstream row(line);
    std::string range, permissions, offset, device, inode;
    if (!(row >> range >> permissions >> offset >> device >> inode)) {
      continue;
    }
    MapEntry entry;
    const auto dash = range.find('-');
    if (dash == std::string::npos) {
      continue;
    }
    entry.start = std::stoull(range.substr(0, dash), nullptr, 16);
    entry.end = std::stoull(range.substr(dash + 1), nullptr, 16);
    entry.offset = std::stoull(offset, nullptr, 16);
    const auto colon = device.find(':');
    if (colon == std::string::npos) {
      continue;
    }
    entry.device_major = static_cast<unsigned int>(
        std::stoul(device.substr(0, colon), nullptr, 16));
    entry.device_minor = static_cast<unsigned int>(
        std::stoul(device.substr(colon + 1), nullptr, 16));
    entry.inode = std::stoull(inode);
    std::getline(row >> std::ws, entry.path);
    maps.push_back(std::move(entry));
  }
  return maps;
}

bool same_file(const MapEntry &map, const struct stat &status) {
  return map.inode == static_cast<std::uint64_t>(status.st_ino) &&
         map.device_major == static_cast<unsigned int>(major(status.st_dev)) &&
         map.device_minor == static_cast<unsigned int>(minor(status.st_dev));
}

std::uintptr_t map_load_bias(const std::vector<MapEntry> &maps,
                             const struct stat &status) {
  std::uintptr_t bias = 0;
  bool found = false;
  for (const auto &map : maps) {
    if (!same_file(map, status)) {
      continue;
    }
    const auto candidate = map.start - map.offset;
    if (!found || candidate < bias) {
      bias = candidate;
      found = true;
    }
  }
  if (!found) {
    throw std::runtime_error(
        "module backing file is not mapped in the target process");
  }
  return bias;
}

std::string elf_build_id(const char *path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error(
        "cannot read local ELF module to verify its build id");
  }
  Elf64_Ehdr header{};
  file.read(reinterpret_cast<char *>(&header), sizeof(header));
  if (!file || std::memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
      header.e_ident[EI_CLASS] != ELFCLASS64 ||
      header.e_ident[EI_DATA] != ELFDATA2LSB || header.e_machine != EM_X86_64 ||
      header.e_phentsize != sizeof(Elf64_Phdr)) {
    throw std::runtime_error(
        "local symbol module is not a compatible ELF64 x86_64 image");
  }
  if (header.e_phnum == 0 || header.e_phnum > 4096) {
    throw std::runtime_error(
        "local ELF module has an invalid program-header table");
  }
  file.seekg(static_cast<std::streamoff>(header.e_phoff));
  std::vector<Elf64_Phdr> programs(header.e_phnum);
  file.read(reinterpret_cast<char *>(programs.data()),
            static_cast<std::streamsize>(programs.size() * sizeof(Elf64_Phdr)));
  if (!file) {
    throw std::runtime_error("cannot read local ELF program headers");
  }
  for (const auto &program : programs) {
    if (program.p_type != PT_NOTE || program.p_filesz == 0 ||
        program.p_filesz > 1024 * 1024) {
      continue;
    }
    std::vector<unsigned char> notes(
        static_cast<std::size_t>(program.p_filesz));
    file.seekg(static_cast<std::streamoff>(program.p_offset));
    file.read(reinterpret_cast<char *>(notes.data()),
              static_cast<std::streamsize>(notes.size()));
    if (!file) {
      throw std::runtime_error("cannot read local ELF note segment");
    }
    std::size_t offset = 0;
    while (offset + sizeof(Elf64_Nhdr) <= notes.size()) {
      Elf64_Nhdr note{};
      std::memcpy(&note, notes.data() + offset, sizeof(note));
      offset += sizeof(note);
      const auto name_size = static_cast<std::size_t>(note.n_namesz);
      const auto description_size = static_cast<std::size_t>(note.n_descsz);
      const auto padded_name = (name_size + 3) & ~std::size_t{3};
      const auto padded_description = (description_size + 3) & ~std::size_t{3};
      if (padded_name > notes.size() - offset) {
        break;
      }
      const auto *name = notes.data() + offset;
      offset += padded_name;
      if (padded_description > notes.size() - offset) {
        break;
      }
      const auto *description = notes.data() + offset;
      offset += padded_description;
      if (note.n_type == NT_GNU_BUILD_ID && name_size >= 3 &&
          std::memcmp(name, "GNU", 3) == 0 && description_size != 0) {
        std::ostringstream encoded;
        encoded << std::hex << std::setfill('0');
        for (std::size_t i = 0; i < description_size; ++i) {
          encoded << std::setw(2) << static_cast<unsigned int>(description[i]);
        }
        return encoded.str();
      }
    }
  }
  throw std::runtime_error("local symbol module has no GNU ELF build id; "
                           "refusing address translation");
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
        for (const pid_t tid : thread_ids(process.tid_)) {
          if (tid == process.tid_ || find(tid) != peers.end()) {
            continue;
          }
          Peer peer;
          peer.tid = tid;
          if (::ptrace(PTRACE_SEIZE, tid, nullptr, nullptr) == -1) {
            if (errno == ESRCH || errno == ECHILD) {
              if (thread_exists(process.tid_, tid)) {
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
              if (thread_exists(process.tid_, tid)) {
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
            wait_peer(peers.back(), deadline, process.tid_);
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
        const auto current = thread_ids(process.tid_);
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
          ::syscall(SYS_tgkill, process.tid_, it->tid, it->pending_signal) !=
              0 &&
          !(errno == ESRCH && !thread_exists(process.tid_, it->tid))) {
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
          if (!thread_exists(process.tid_, it->tid)) {
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
                        process.tid_);
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
                     !thread_exists(process.tid_, it->tid)) {
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
          throw std::runtime_error(
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
      const pid_t unsafe_pid = tid_;
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
    : tid_(std::exchange(other.tid_, -1)),
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
                   static_cast<int>(tid_));
      return *this;
    }
    try {
      detach();
    } catch (const std::exception &error) {
      std::fprintf(
          stderr,
          "injector: cannot release PID %d during move assignment: %s\n",
          static_cast<int>(tid_), error.what());
      return *this;
    } catch (...) {
      std::fprintf(stderr,
                   "injector: cannot release PID %d during move assignment; "
                   "ownership retained\n",
                   static_cast<int>(tid_));
      return *this;
    }
  }
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
  if (tid_ > 0 && ::kill(tid_, SIGSTOP) != 0 && errno != ESRCH) {
    std::fprintf(stderr,
                 "injector: could not request SIGSTOP for unsafe PID %d: %s\n",
                 static_cast<int>(tid_), std::strerror(errno));
  }
}

void Process::mark_unsafe() noexcept { poison_process(); }

void Process::attach(pid_t process_id,
                     std::span<const ApplicationModule> application_modules) {
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
      throw std::runtime_error(
          "target leader is in a job-control group stop; refusing injection");
    }
    if (event != PTRACE_EVENT_STOP) {
      // Preserve a genuine signal-delivery stop if one raced with interrupt.
      pending_signal_ = WSTOPSIG(status);
    }
    validate_application_modules(tid_, application_modules);
    const long syscall_number = stopped_syscall(tid_);
    if (syscall_number != SYS_nanosleep &&
        syscall_number != SYS_clock_nanosleep) {
      if (syscall_number >= 0) {
        throw IdlePointUnavailable(
            "busy-main admission requires a userspace stop");
      }
      (void)require_application_rendezvous(tid_, read_registers(tid_).general,
                                           application_modules);
    }
    (void)read_registers(
        tid_); // Fail early if complete register preservation is unavailable.
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
  pending_signal_ = 0;
  tracee_stopped_ = false;
  scratch_stack_ = 0;
  scratch_stack_size_ = 0;
  call_page_ = 0;
  call_page_size_ = 0;
}

void Process::preserve_poisoned_stop() {
  ensure_attached();
  const pid_t unsafe_pid = tid_;
  if (!tracee_stopped_) {
    ptrace_checked(PTRACE_INTERRUPT, unsafe_pid, nullptr, nullptr,
                   "interrupt unsafe tracee before preserving stop");
    wait_stopped_or_throw(unsafe_pid,
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
      ::syscall(SYS_tgkill, unsafe_pid, unsafe_pid, pending_signal_) != 0) {
    throw_errno("preserve leader signal during poisoned stop");
  }
  ptrace_checked(PTRACE_DETACH, unsafe_pid, nullptr,
                 reinterpret_cast<void *>(static_cast<std::uintptr_t>(SIGSTOP)),
                 "detach poisoned tracee while delivering SIGSTOP");
  tid_ = -1;
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
    if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGTRAP) {
      if (WIFSTOPPED(status) &&
          (static_cast<unsigned int>(status) >> 16) == 0) {
        pending_signal_ = WSTOPSIG(status);
      }
      throw std::runtime_error("native syscall ended at an unexpected stop; "
                               "tracee stopped for recovery");
    }
    const auto after = read_registers(tid_);
    if (after.general.rip != ip + stub.size()) {
      throw std::runtime_error("native syscall stub stopped at an unexpected "
                               "instruction; state restored");
    }
    result = after.general.rax;
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
    if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGTRAP) {
      if (WIFSTOPPED(status) &&
          (static_cast<unsigned int>(status) >> 16) == 0) {
        pending_signal_ = WSTOPSIG(status);
      }
      throw std::runtime_error(
          "remote call stopped on signal " +
          std::to_string(WIFSTOPPED(status) ? WSTOPSIG(status) : 0) +
          " at PC " + std::to_string(read_registers(tid_).general.rip) +
          " before its completion; the callee "
          "may have side effects");
    }
    const auto after = read_registers(tid_);
    if (after.general.rip != call_page_ + 3) {
      throw std::runtime_error(
          "remote call stopped at an unexpected instruction");
    }
    result = after.general.rax;
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

std::uintptr_t Process::resolve_remote_symbol(std::string_view name) {
  ensure_usable();
  if (name.empty() || name.find('\0') != std::string_view::npos) {
    throw std::invalid_argument(
        "symbol name must be nonempty and contain no NUL byte");
  }
  const std::string symbol_name(name);
  ::dlerror();
  void *local_address = ::dlsym(RTLD_DEFAULT, symbol_name.c_str());
  if (const char *error = ::dlerror()) {
    throw std::runtime_error("local dlsym(" + symbol_name +
                             ") failed: " + error);
  }
  if (local_address == nullptr) {
    throw std::runtime_error("local dlsym(" + symbol_name + ") returned null");
  }

  Dl_info info{};
  if (::dladdr(local_address, &info) == 0 || info.dli_fname == nullptr ||
      info.dli_fbase == nullptr) {
    throw std::runtime_error("dladdr could not identify the local module for " +
                             symbol_name);
  }
  struct stat module_status{};
  if (::stat(info.dli_fname, &module_status) != 0) {
    throw_errno("stat local symbol module");
  }
  const std::string build_id = elf_build_id(info.dli_fname);

  const auto local_maps = read_maps(::getpid());
  const auto remote_maps = read_maps(tid_);
  const auto local_bias = map_load_bias(local_maps, module_status);
  const auto remote_bias = map_load_bias(remote_maps, module_status);
  const auto actual_local_base =
      reinterpret_cast<std::uintptr_t>(info.dli_fbase);
  if (local_bias != actual_local_base) {
    throw std::runtime_error(
        "local module mapping identity/base is inconsistent for " +
        symbol_name);
  }
  // Requiring a build id plus matching device/inode on both hosts binds the
  // translated offset to the exact local ELF image, rather than a same-named
  // library from a different runtime installation.
  if (build_id.empty()) {
    throw std::runtime_error("local module build id is empty for " +
                             symbol_name);
  }
  const auto symbol = reinterpret_cast<std::uintptr_t>(local_address);
  if (symbol < local_bias) {
    throw std::runtime_error(
        "local symbol address precedes its module load base");
  }
  return remote_bias + (symbol - local_bias);
}

} // namespace tracer::injector

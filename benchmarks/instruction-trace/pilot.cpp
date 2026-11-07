#include "pilot.h"

#include <array>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

#include <dlfcn.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern "C" {
std::uint64_t pilot_work(unsigned, int, int, int);
std::uint64_t pilot_native(unsigned, int, int, int);
extern char pilot_work_end[], pilot_native_trap[], pilot_native_done[];
}

namespace {
constexpr unsigned max_events = 8192;
constexpr int trap_unknown = 5; // Linux TRAP_UNK, absent in the GCC 8.5 SDK headers.
using Offsets = std::array<std::uint32_t, max_events>;
enum class Mode : unsigned { baseline, native, gum, post };
struct Request { Mode mode; unsigned rounds, limit, signal; };

const auto begin = reinterpret_cast<std::uintptr_t>(&pilot_work);
const auto end = reinterpret_cast<std::uintptr_t>(pilot_work_end);
volatile sig_atomic_t signals = 0;
volatile sig_atomic_t expired = 0;

void on_alarm(int) { expired = 1; }
void on_usr1(int) { ++signals; }

[[noreturn]] void fail(const char *operation) {
  throw std::system_error(errno, std::generic_category(), operation);
}

std::uint64_t now_ns() {
  timespec ts{};
  if (::clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
    fail("clock_gettime");
  return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
}

void signal_handler(int signo, void (*handler)(int)) {
  struct sigaction action{};
  action.sa_handler = handler;
  sigemptyset(&action.sa_mask);
  if (sigaction(signo, &action, nullptr) < 0)
    fail("sigaction");
}

struct Deadline {
  Deadline() {
    expired = 0;
    itimerval timeout{};
    timeout.it_value.tv_sec = 5;
    timeout.it_interval.tv_usec = 100'000;
    if (setitimer(ITIMER_REAL, &timeout, nullptr) < 0)
      fail("setitimer");
  }
  ~Deadline() {
    const itimerval stopped{};
    setitimer(ITIMER_REAL, &stopped, nullptr);
  }
};

void check_time() {
  if (expired)
    throw std::runtime_error("five-second phase deadline expired");
}

bool read_all(int fd, void *buffer, std::size_t size) {
  auto *out = static_cast<char *>(buffer);
  std::size_t used = 0;
  while (used < size) {
    check_time();
    const auto count = ::read(fd, out + used, size - used);
    if (count == 0 && used == 0)
      return false;
    if (count == 0)
      throw std::runtime_error("truncated fixture reply");
    if (count < 0) {
      if (errno == EINTR)
        continue;
      fail("read fixture pipe");
    }
    used += count;
  }
  return true;
}

void write_all(int fd, const void *buffer, std::size_t size) {
  const auto *in = static_cast<const char *>(buffer);
  std::size_t used = 0;
  while (used < size) {
    check_time();
    const auto count = ::write(fd, in + used, size - used);
    if (count <= 0) {
      if (count < 0 && errno == EINTR)
        continue;
      fail("write fixture pipe");
    }
    used += count;
  }
}

struct Child {
  pid_t pid;
  ~Child() {
    if (pid > 0) {
      ::kill(pid, SIGKILL);
      for (;;) {
        int status;
        const auto waited = ::waitpid(pid, &status, 0);
        if ((waited == pid && (WIFEXITED(status) || WIFSIGNALED(status))) ||
            (waited < 0 && errno != EINTR))
          break;
      }
    }
  }
};

class GumModule {
  void *handle_ = nullptr;
  void (*shutdown_)() = nullptr;

  template <class Function> Function symbol(const char *name) {
    const auto function = reinterpret_cast<Function>(::dlsym(handle_, name));
    if (!function)
      throw std::runtime_error(::dlerror());
    return function;
  }
public:
  PilotGumRun run = nullptr;

  void load(const char *file) {
    handle_ = ::dlopen(file, RTLD_LOCAL | RTLD_NOW);
    if (!handle_)
      throw std::runtime_error(::dlerror());
    const auto init = symbol<int (*)()>("pilot_gum_init");
    shutdown_ = symbol<void (*)()>("pilot_gum_shutdown");
    run = symbol<PilotGumRun>("pilot_gum_run");
    if (init() != 0)
      throw std::runtime_error("Gum Stalker is unsupported");
  }

  void close() {
    if (handle_) {
      shutdown_();
      if (::dlclose(handle_) != 0)
        throw std::runtime_error(::dlerror());
      handle_ = nullptr;
      run = nullptr;
    }
  }
};

int fixture(int request_fd, int result_fd, const char *plugin, pid_t parent) {
  if (prctl(PR_SET_PDEATHSIG, SIGKILL) < 0)
    fail("PR_SET_PDEATHSIG");
  if (getppid() != parent)
    return 2;
  signal_handler(SIGUSR1, on_usr1);
  const int pid = getpid();
  const int tid = ::syscall(SYS_gettid);
  if (tid != pid)
    throw std::logic_error("fixture thread is not the owned leader");

  GumModule gum;
  Request request{};
  while (read_all(request_fd, &request, sizeof(request))) {
    PilotResult result{};
    Offsets offsets;
    signals = 0;
    if (request.mode == Mode::gum) {
      if (!gum.run)
        gum.load(plugin);
      if (gum.run(&pilot_work, request.rounds, pid, tid, request.signal,
                  begin, end, request.limit, &result, offsets.data()) != 0)
        throw std::runtime_error("Gum failed to stop and reclaim its trace");
    } else {
      if (request.mode == Mode::post)
        gum.close();
      const auto start = now_ns();
      const auto work = request.mode == Mode::native ? &pilot_native : &pilot_work;
      result.value = work(request.rounds, pid, tid, request.signal);
      result.work_ns = now_ns() - start;
    }
    result.signals = signals;
    write_all(result_fd, &result, sizeof(result));
    write_all(result_fd, offsets.data(), result.count * sizeof(offsets[0]));
  }
  gum.close();
  return 0;
}

void trace(enum __ptrace_request request, pid_t pid, void *data = nullptr) {
  if (ptrace(request, pid, nullptr, data) < 0)
    fail("ptrace");
}

int wait_for(Child &child) {
  int status;
  for (;;) {
    check_time();
    const auto result = waitpid(child.pid, &status, 0);
    if (result >= 0)
      break;
    if (errno != EINTR)
      fail("waitpid");
  }
  if (WIFEXITED(status) || WIFSIGNALED(status))
    child.pid = -1;
  return status;
}

siginfo_t trap(Child &child) {
  const auto status = wait_for(child);
  if (!WIFSTOPPED(status))
    throw std::runtime_error("fixture exited during native capture");
  siginfo_t info{};
  trace(PTRACE_GETSIGINFO, child.pid, &info);
  return info;
}

std::uintptr_t ip(pid_t pid) {
  user_regs_struct registers{};
  trace(PTRACE_GETREGS, pid, &registers);
  return registers.rip;
}

bool syscall_instruction(pid_t pid, std::uintptr_t pc) {
  errno = 0;
  const auto bytes = ptrace(PTRACE_PEEKTEXT, pid, reinterpret_cast<void *>(pc), nullptr);
  if (bytes == -1 && errno)
    fail("inspect syscall single-step trap");
  return (bytes & 0xffff) == 0x050f;
}

PilotResult native_capture(Child &child, unsigned limit, Offsets &offsets) {
  const auto initial = trap(child);
  auto pc = ip(child.pid);
  if (initial.si_signo != SIGTRAP ||
      pc != reinterpret_cast<std::uintptr_t>(pilot_native_trap) + 1)
    throw std::runtime_error("unexpected stop before native workload");
  PilotResult result{};
  int pending_signal = 0;
  for (unsigned step = 0; pc != reinterpret_cast<std::uintptr_t>(pilot_native_done); ++step) {
    if (step == 32768)
      throw std::runtime_error("native single-step budget exhausted");
    const bool capture = !pending_signal && pc >= begin && pc < end;
    if (capture && result.count == limit) {
      result.truncated = 1;
      break;
    }
    const bool delivering = pending_signal != 0;
    trace(PTRACE_SINGLESTEP, child.pid,
          reinterpret_cast<void *>(static_cast<std::intptr_t>(std::exchange(pending_signal, 0))));
    const auto info = trap(child);
    const auto next = ip(child.pid);
    if (info.si_signo == SIGUSR1 && info.si_code == SI_TKILL) {
      pending_signal = SIGUSR1;
      if (capture && next != pc)
        offsets[result.count++] = static_cast<std::uint32_t>(pc - begin);
    } else if (info.si_signo == SIGTRAP &&
               (info.si_code == TRAP_TRACE ||
                (info.si_code == TRAP_BRKPT && syscall_instruction(child.pid, pc)) ||
                (info.si_code == trap_unknown && delivering))) {
      if (capture)
        offsets[result.count++] = static_cast<std::uint32_t>(pc - begin);
    } else {
      throw std::runtime_error("unexpected native stop signo=" + std::to_string(info.si_signo) +
                               " code=" + std::to_string(info.si_code));
    }
    pc = next;
  }
  trace(PTRACE_DETACH, child.pid,
        reinterpret_cast<void *>(static_cast<std::intptr_t>(pending_signal)));
  return result;
}

unsigned number(std::string_view input, unsigned minimum, unsigned maximum) {
  unsigned value = 0;
  const auto result = std::from_chars(input.data(), input.data() + input.size(), value);
  if (result.ec != std::errc{} || result.ptr != input.data() + input.size() ||
      value < minimum || value > maximum)
    throw std::invalid_argument("pilot argument out of range");
  return value;
}

void run(const char *plugin, unsigned rounds, unsigned limit, unsigned deliver) {
  signal_handler(SIGALRM, on_alarm);
  signal_handler(SIGPIPE, SIG_IGN);
  int requests[2], replies[2];
  if (::pipe(requests) < 0 || ::pipe(replies) < 0)
    fail("pipe");
  const auto parent = getpid();
  Child child{fork()};
  if (child.pid < 0)
    fail("fork");
  if (child.pid == 0) {
    close(requests[1]);
    close(replies[0]);
    try {
      ::_exit(fixture(requests[0], replies[1], plugin, parent));
    } catch (const std::exception &e) {
      std::cerr << "fixture: " << e.what() << '\n';
      ::_exit(2);
    }
  }
  close(requests[0]);
  close(replies[1]);
  const auto pid = child.pid;
  for (const auto mode : {Mode::baseline, Mode::native, Mode::gum, Mode::post}) {
    if (mode == Mode::gum && std::string_view(plugin) == "-")
      continue;
    Deadline deadline;
    const auto started = now_ns();
    if (mode == Mode::native)
      trace(PTRACE_SEIZE, pid, reinterpret_cast<void *>(PTRACE_O_EXITKILL));
    const Request request{mode, rounds, limit, deliver};
    write_all(requests[1], &request, sizeof(request));
    Offsets offsets;
    PilotResult native{};
    if (mode == Mode::native)
      native = native_capture(child, limit, offsets);
    PilotResult result{};
    if (!read_all(replies[0], &result, sizeof(result)) || result.count > limit ||
        !read_all(replies[0], offsets.data(), result.count * sizeof(offsets[0])))
      throw std::runtime_error("invalid or missing fixture reply");
    if (mode == Mode::native) {
      result.count = native.count;
      result.truncated = native.truncated;
    }
    const auto wall = now_ns() - started;
    constexpr const char *names[] = {"baseline", "native", "gum", "post"};
    std::cout << "{\"mode\":\"" << names[static_cast<unsigned>(mode)] << "\",\"pid\":" << pid
              << ",\"tid\":" << pid << ",\"begin\":" << begin << ",\"end\":" << end
              << ",\"value\":" << result.value << ",\"signals\":" << result.signals
              << ",\"truncated\":" << result.truncated << ",\"work_ns\":" << result.work_ns
              << ",\"wall_ns\":" << wall << ",\"offsets\":[";
    for (unsigned i = 0; i < result.count; ++i)
      std::cout << (i ? "," : "") << offsets[i];
    std::cout << "]}\n" << std::flush;
  }
  close(requests[1]);
  close(replies[0]);
  Deadline deadline;
  const auto status = wait_for(child);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    throw std::runtime_error("fixture did not exit cleanly");
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 5)
      throw std::invalid_argument("usage: pilot GUM_PLUGIN|- ROUNDS LIMIT SIGNAL_0_OR_1");
    run(argv[1], number(argv[2], 1, 1000), number(argv[3], 1, max_events), number(argv[4], 0, 1));
  } catch (const std::exception &e) {
    std::cerr << "pilot: " << e.what() << '\n';
    return 1;
  }
}

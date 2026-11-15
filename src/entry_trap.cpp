#include "entry_trap.hpp"

#include "injector.hpp"
#include "linux_syscalls.hpp"
#include "proc_identity.hpp"
#include "ptrace_wait.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <map>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <thread>

#include <signal.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>

namespace tracer::injector {
namespace {
using Clock = std::chrono::steady_clock;
constexpr unsigned char int3 = 0xcc;
constexpr unsigned long trace_creations = PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK |
                                          PTRACE_O_TRACEVFORK | PTRACE_O_TRACEEXEC;

void *arg(unsigned long value) { return reinterpret_cast<void *>(value); }

void control(__ptrace_request op, pid_t tid, void *data = nullptr) {
  if (::ptrace(op, tid, nullptr, data) == -1)
    throw std::system_error(errno, std::generic_category(), "entry trap ptrace");
}

struct Task {
  bool stopped = false;
  bool listening = false;
  bool child = false;
  bool rewind = false;
  int signal = 0;
};

bool is_creation(unsigned event) {
  return event == PTRACE_EVENT_CLONE || event == PTRACE_EVENT_FORK ||
         event == PTRACE_EVENT_VFORK;
}
} // namespace

struct EntryTrap::Worker {
  const pid_t pid;
  const std::uint64_t start;
  const Request patch;
  const std::uintptr_t remote_request;
  const std::chrono::seconds timeout;
  const std::filesystem::path proc_dir;
  std::map<pid_t, Task> tasks;
  bool changed_text = false;
  bool retired_image = false;
  std::atomic<bool> stop_requested{false};

  mutable std::mutex mutex;
  std::condition_variable changed;
  bool ready = false;
  bool installed = false;
  bool done = false;
  bool abandoning = false;
  unsigned retry = 0;
  unsigned failed_retry = 0;
  std::string problem;
  std::string restore_error;
  std::thread thread;

  Worker(std::uint32_t target, std::uint64_t process_start, const Request &metadata,
         std::uintptr_t request_address, std::chrono::seconds operation_timeout)
      : pid(target), start(process_start), patch(metadata), remote_request(request_address),
        timeout(operation_timeout),
        proc_dir(proc::directory(pid)), thread([this] { run(); }) {}

  ~Worker() {
    {
      std::lock_guard lock(mutex);
      abandoning = true;
      stop_requested = true;
    }
    changed.notify_all();
    PtraceWait::wake();
    thread.join();
  }

  void startup() {
    std::unique_lock lock(mutex);
    if (!changed.wait_for(lock, timeout, [&] { return ready; })) {
      stop_requested = true;
      PtraceWait::wake();
      restore_error = "timed out waiting for entry trap installation or cleanup";
      changed.notify_all();
    }
    // Keep the worker if patch recovery is pending so the session can retry.
    if (!installed && restore_error.empty())
      throw std::runtime_error(problem);
  }

  std::string stop() {
    std::unique_lock lock(mutex);
    const auto attempt = ++retry;
    stop_requested = true;
    PtraceWait::wake();
    changed.notify_all();
    if (!changed.wait_for(lock, timeout, [&] { return done || failed_retry == attempt; }))
      return "timed out waiting for entry trap restoration";
    return restore_error;
  }

  void verify_process() const {
    if (proc::start_time(proc_dir / "stat") != start)
      throw std::runtime_error("entry trap target identity changed");
  }

  void verify_wrapper_owner() const {
    Request live{};
    iovec local{&live, sizeof(live)};
    iovec remote{reinterpret_cast<void *>(remote_request), sizeof(live)};
    if (linux_api::process_vm_readv(pid, &local, 1, &remote, 1, 0) !=
            static_cast<ssize_t>(sizeof(live)) ||
        live.version != patch.version || live.probe_id != patch.probe_id ||
        live.wrapper_address != patch.wrapper_address ||
        live.patch_address != patch.patch_address ||
        std::memcmp(live.session_id, patch.session_id, sizeof(live.session_id)) != 0)
      throw std::runtime_error("entry trap wrapper no longer belongs to this process image");
  }

  std::string read_code(pid_t tid) const {
    std::string bytes(patch.verified_size, '\0');
    iovec local{bytes.data(), bytes.size()};
    iovec remote{reinterpret_cast<void *>(patch.original_entry), bytes.size()};
    if (linux_api::process_vm_readv(tid, &local, 1, &remote, 1, 0) !=
        static_cast<ssize_t>(bytes.size()))
      throw std::runtime_error("cannot read the verified entry trap range");
    return bytes;
  }

  std::size_t offset() const { return patch.patch_address - patch.original_entry; }

  void require_code(pid_t tid, bool armed) const {
    auto expected = std::string(reinterpret_cast<const char *>(patch.verified_bytes),
                                patch.verified_size);
    if (armed)
      expected[offset()] = static_cast<char>(int3);
    if (read_code(tid) != expected)
      throw std::runtime_error("live entry bytes differ from the owned entry trap");
  }

  void restore_code(pid_t tid) {
    const auto bytes = read_code(tid);
    const std::string_view original(reinterpret_cast<const char *>(patch.verified_bytes),
                                    patch.verified_size);
    if (bytes == original)
      return;
    require_code(tid, true);
    write_stopped_code_byte(tid, patch.patch_address, patch.original[0]);
    require_code(tid, false);
  }

  void interrupt(pid_t tid) {
    if (::ptrace(PTRACE_INTERRUPT, tid, nullptr, nullptr) == -1 && errno != ESRCH)
      throw std::system_error(errno, std::generic_category(), "interrupt entry trap thread");
  }

  void pause(Clock::time_point deadline) {
    std::unique_lock lock(mutex);
    if (abandoning || Clock::now() >= deadline)
      throw std::runtime_error("could not confirm all entry trap stops");
    changed.wait_for(lock, std::chrono::milliseconds(1));
  }

  void set_ip(pid_t tid, std::uint64_t pc) {
    user_regs_struct regs{};
    control(PTRACE_GETREGS, tid, &regs);
    regs.rip = pc;
    control(PTRACE_SETREGS, tid, &regs);
  }

  void observe(pid_t tid, int status) {
    if (!WIFSTOPPED(status)) {
      tasks.erase(tid);
      return;
    }
    auto [it, inserted] = tasks.try_emplace(tid);
    auto &task = it->second;
    task.child |= inserted;
    task.stopped = true;
    task.listening = false;
    task.signal = 0;
    const auto event = static_cast<unsigned>(status) >> 16;
    const auto signal = WSTOPSIG(status);
    if (event == PTRACE_EVENT_EXEC) {
      // Only the execing task survives exec, possibly with the leader's TID.
      retired_image = true;
      stop_requested = true;
      tasks.clear();
      tasks[tid].stopped = true;
      return;
    }
    if (is_creation(event)) {
      unsigned long child = 0;
      control(PTRACE_GETEVENTMSG, tid, &child);
      tasks[static_cast<pid_t>(child)].child = true;
      stop_requested = true;
      return;
    }
    if (event == PTRACE_EVENT_STOP) {
      task.listening = signal != SIGTRAP && !task.child;
      return;
    }
    if (event)
      throw std::runtime_error("unexpected entry trap ptrace event");
    task.signal = signal;
    if (signal != SIGTRAP || !changed_text)
      return;
    siginfo_t info{};
    control(PTRACE_GETSIGINFO, tid, &info);
    if (info.si_code != TRAP_BRKPT && info.si_code != SI_KERNEL)
      return;
    user_regs_struct regs{};
    control(PTRACE_GETREGS, tid, &regs);
    if (regs.rip == patch.patch_address + 1) {
      task.signal = 0;
      task.rewind = true;
    }
  }

  // __WNOTHREAD prevents stealing ptrace waits from injection or object watches
  // running for other sessions on different server threads.
  pid_t receive() {
    int status = 0;
    const auto tid = ::waitpid(-1, &status, __WALL | __WNOTHREAD | WNOHANG);
    if (tid > 0)
      observe(tid, status);
    else if (tid < 0 && errno != ECHILD && errno != EINTR)
      throw std::system_error(errno, std::generic_category(), "wait for entry trap");
    return tid;
  }

  void await_stops(Clock::time_point deadline) {
    while (std::any_of(tasks.begin(), tasks.end(), [](const auto &item) {
      return !item.second.stopped;
    })) {
      if (receive() <= 0)
        pause(deadline);
    }
  }

  void seize_all(Clock::time_point deadline) {
    verify_process();
    for (;;) {
      if (stop_requested || Clock::now() >= deadline)
        throw std::runtime_error("entry trap thread admission expired");
      bool found = false;
      for (const auto &entry : std::filesystem::directory_iterator(proc_dir / "task")) {
        const auto tid = static_cast<pid_t>(std::stol(entry.path().filename().string()));
        if (tasks.contains(tid))
          continue;
        if (::ptrace(PTRACE_SEIZE, tid, nullptr, nullptr) == -1) {
          if (errno == ESRCH)
            continue;
          throw std::system_error(errno, std::generic_category(), "seize entry trap thread");
        }
        tasks.emplace(tid, Task{});
        interrupt(tid);
        found = true;
      }
      if (!found)
        break;
      await_stops(deadline);
    }
    verify_process();
    if (!tasks.contains(pid))
      throw std::runtime_error("entry trap leader exited during admission");
    for (const auto &[tid, task] : tasks) {
      if (!std::filesystem::exists(proc_dir / "task" / std::to_string(tid)))
        throw std::runtime_error("entry trap task identity changed during admission");
      control(PTRACE_SETOPTIONS, tid, arg(trace_creations));
      user_regs_struct regs{};
      control(PTRACE_GETREGS, tid, &regs);
      if (regs.rip > patch.patch_address &&
          regs.rip - patch.original_entry < patch.verified_size)
        throw std::runtime_error("target stopped inside the getter; retry probe installation");
    }
  }

  void resume(pid_t tid, Task &task) {
    if (task.rewind) {
      set_ip(tid, patch.wrapper_address);
      task.rewind = false;
    }
    control(task.listening ? PTRACE_LISTEN : PTRACE_CONT, tid, arg(task.signal));
    task.stopped = false;
    task.signal = 0;
  }

  void trace() {
    const auto deadline = Clock::now() + timeout;
    bool notifications = PtraceWait::enable();
    seize_all(deadline);
    verify_wrapper_owner();
    require_code(pid, false);
    {
      std::lock_guard lock(mutex);
      if (stop_requested)
        throw std::runtime_error("entry trap installation cancelled");
    }
    changed_text = true;
    write_stopped_code_byte(pid, patch.patch_address, int3);
    require_code(pid, true);
    for (auto &[tid, task] : tasks)
      resume(tid, task);
    {
      std::lock_guard lock(mutex);
      installed = ready = true;
      changed.notify_all();
    }
    while (!stop_requested && !tasks.empty()) {
      const auto generation = PtraceWait::generation();
      const auto tid = receive();
      if (tid > 0) {
        const auto found = tasks.find(tid);
        if (found != tasks.end() && found->second.child) {
          stop_requested = true;
          break;
        }
        if (found != tasks.end() && !stop_requested)
          resume(tid, found->second);
      } else if (notifications) {
        if (!stop_requested)
          notifications = PtraceWait::wait(generation, Clock::time_point::max());
      } else {
        pause(Clock::time_point::max());
      }
    }
  }

  void restore() {
    const auto deadline = Clock::now() + timeout;
    for (const auto &[tid, task] : tasks)
      if (!task.stopped)
        interrupt(tid);
    await_stops(deadline);
    if (changed_text && !retired_image) {
      const auto parent = std::find_if(tasks.begin(), tasks.end(), [](const auto &item) {
        return !item.second.child;
      });
      if (parent != tasks.end())
        restore_code(parent->first);
      // A fork can inherit a private copy of the patched executable mapping.
      // All child tasks remain stopped until both images have been restored.
      for (const auto &[tid, task] : tasks)
        if (task.child)
          restore_code(tid);
    }
    for (auto it = tasks.begin(); it != tasks.end();) {
      auto &[tid, task] = *it;
      if (task.rewind) {
        set_ip(tid, patch.patch_address);
        task.rewind = false;
      }
      if (::ptrace(PTRACE_DETACH, tid, nullptr, arg(task.signal)) == -1 && errno != ESRCH)
        throw std::system_error(errno, std::generic_category(), "release entry trap thread");
      it = tasks.erase(it);
    }
    changed_text = false;
  }

  void run() noexcept {
    try {
      trace();
      std::lock_guard lock(mutex);
      if (retired_image)
        problem = "target executed a different image";
      else if (std::any_of(tasks.begin(), tasks.end(), [](const auto &item) {
                 return item.second.child;
               }))
        problem = "trap coverage ended at thread or process creation";
    } catch (const std::exception &error) {
      std::lock_guard lock(mutex);
      problem = error.what();
    }
    for (;;) {
      unsigned attempt;
      {
        std::lock_guard lock(mutex);
        attempt = retry;
      }
      try {
        restore();
        break;
      } catch (const std::exception &error) {
        std::unique_lock lock(mutex);
        restore_error = error.what();
        failed_retry = attempt;
        ready = true;
        changed.notify_all();
        changed.wait(lock, [&] { return abandoning || retry != attempt; });
        if (abandoning) {
          // Prefer a parked target to resuming with an unverified live trap.
          try {
            verify_process();
            (void)::kill(pid, SIGSTOP);
          } catch (const std::exception &) {}
          for (const auto &[tid, task] : tasks) {
            if (task.child)
              (void)::kill(tid, SIGSTOP);
            if (task.stopped)
              (void)::ptrace(PTRACE_DETACH, tid, nullptr, arg(SIGSTOP));
          }
          return;
        }
      }
    }
    std::lock_guard lock(mutex);
    installed = false;
    restore_error.clear();
    if (problem.empty())
      problem = "original entry bytes restored or target exited";
    ready = done = true;
    changed.notify_all();
  }
};

EntryTrap::EntryTrap(std::uint32_t pid, std::uint64_t process_start, const Request &patch,
                     std::uintptr_t remote_request, std::chrono::seconds timeout)
    : worker_(std::make_unique<Worker>(pid, process_start, patch, remote_request, timeout)) {
  worker_->startup();
}
EntryTrap::~EntryTrap() = default;

std::string EntryTrap::stop() { return worker_->stop(); }

std::string EntryTrap::diagnostic() const {
  std::lock_guard lock(worker_->mutex);
  return worker_->restore_error.empty() ? worker_->problem : worker_->restore_error;
}

bool EntryTrap::armed() const {
  std::lock_guard lock(worker_->mutex);
  return worker_->installed && worker_->restore_error.empty();
}

bool EntryTrap::image_replaced() const {
  std::lock_guard lock(worker_->mutex);
  return worker_->done && worker_->retired_image;
}
} // namespace tracer::injector

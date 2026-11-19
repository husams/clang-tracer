#include "tracer/page_monitor.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <mutex>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

namespace {
constexpr unsigned read_write = PROT_READ | PROT_WRITE;
std::mutex control;
std::atomic<bool> running{false};
static_assert(std::atomic<bool>::is_always_lock_free);
std::uintptr_t begin;
std::size_t size, page;
std::uint32_t tid;
std::uint64_t deadline;
bool active = false;
bool watchdog_joinable = false;
bool expired = false;
pthread_t watchdog;
TracerPageMonitorResult result;
bool seen[TRACER_PAGE_MONITOR_MAX_PAGES];
struct sigaction original_action;

std::uint64_t now() {
  timespec time{};
  ::clock_gettime(CLOCK_MONOTONIC, &time);
  return static_cast<std::uint64_t>(time.tv_sec) * 1'000'000'000 + time.tv_nsec;
}

void on_fault(int signal, siginfo_t *info, void *context) {
  const auto saved = errno;
  const auto current = static_cast<std::uint32_t>(::syscall(SYS_gettid));
  const auto address = reinterpret_cast<std::uintptr_t>(info->si_addr);
  const auto *cpu = static_cast<ucontext_t *>(context);
  const auto error = cpu->uc_mcontext.gregs[REG_ERR];
  // Only the nominated thread accesses these dedicated pages while armed.
  // The controller runs both setup and teardown on that same thread.
  if (current == tid && info->si_code == SEGV_ACCERR && !(error & 16) &&
      address >= begin && address - begin < size) {
    const auto index = (address - begin) / page;
    if (::syscall(SYS_mprotect, begin + index * page, page, read_write) == 0) {
      if (running.load(std::memory_order_relaxed) && now() < deadline && !seen[index]) {
        seen[index] = true;
        result.accesses[result.count++] = {
            address, static_cast<std::uint64_t>(cpu->uc_mcontext.gregs[REG_RIP]),
            current, (error & 2) ? 2u : 1u};
      }
      errno = saved;
      return;
    }
  }
  // An unrelated synchronous fault must retain the application's default
  // disposition. Re-deliver asynchronous SIGSEGV explicitly.
  struct sigaction action{};
  action.sa_handler = SIG_DFL;
  ::sigemptyset(&action.sa_mask);
  ::sigaction(signal, &action, nullptr);
  if (info->si_code <= 0)
    (void)::syscall(SYS_tgkill, ::getpid(), current, signal);
  errno = saved;
}

void *expire(void *) {
  while (running.load(std::memory_order_relaxed)) {
    if (now() >= deadline) {
      if (running.exchange(false, std::memory_order_relaxed)) {
        expired = true;
        (void)::syscall(SYS_mprotect, begin, size, read_write);
      }
      break;
    }
    (void)::syscall(SYS_poll, nullptr, 0, 10);
  }
  return nullptr;
}
} // namespace

extern "C" int tracer_agent_page_monitor_start_v1(std::uintptr_t address, std::size_t length,
                                                   unsigned seconds) {
  std::lock_guard lock(control);
  const auto page_size = ::sysconf(_SC_PAGESIZE);
  if (active || page_size <= 0 || !address || !length || address % page_size ||
      length % page_size || length / page_size > TRACER_PAGE_MONITOR_MAX_PAGES ||
      !seconds || seconds > 60)
    return -1;
  struct sigaction previous{}, action{};
  if (::sigaction(SIGSEGV, nullptr, &previous) != 0 || previous.sa_handler != SIG_DFL)
    return -3;
  original_action = previous;
  begin = address;
  size = length;
  page = page_size;
  tid = ::syscall(SYS_gettid);
  deadline = now() + static_cast<std::uint64_t>(seconds) * 1'000'000'000;
  result = {};
  expired = false;
  for (auto &value : seen)
    value = false;
  action.sa_sigaction = on_fault;
  action.sa_flags = SA_SIGINFO;
  ::sigemptyset(&action.sa_mask);
  if (::sigaction(SIGSEGV, &action, nullptr) != 0)
    return -1;
  active = true;
  if (::mprotect(reinterpret_cast<void *>(begin), size, PROT_NONE) != 0)
    return -2; // Stop restores the range and the original disposition.
  running.store(true, std::memory_order_relaxed);
  if (::pthread_create(&watchdog, nullptr, expire, nullptr) != 0) {
    running.store(false, std::memory_order_relaxed);
    return -2;
  }
  watchdog_joinable = true;
  return 0;
}

extern "C" int tracer_agent_page_monitor_stop_v1(TracerPageMonitorResult *output) {
  std::lock_guard lock(control);
  if (!active || !output ||
      (static_cast<std::uint32_t>(::syscall(SYS_gettid)) != tid &&
       !(::syscall(SYS_tgkill, ::getpid(), tid, 0) == -1 && errno == ESRCH)))
    return -1;
  running.store(false, std::memory_order_relaxed);
  if (watchdog_joinable) {
    if (::pthread_join(watchdog, nullptr) != 0)
      return -2;
    watchdog_joinable = false;
  }
  if (::mprotect(reinterpret_cast<void *>(begin), size, read_write) != 0)
    return -2;
  struct sigaction current{};
  if (::sigaction(SIGSEGV, nullptr, &current) != 0)
    return -2;
  if ((current.sa_flags & SA_SIGINFO) && current.sa_sigaction == on_fault) {
    if (::sigaction(SIGSEGV, &original_action, nullptr) != 0)
      return -2;
  }
  result.expired = expired;
  *output = result;
  active = false;
  return 0;
}

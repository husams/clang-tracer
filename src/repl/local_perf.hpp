#pragma once

#include <atomic>
#include <cstdint>
#include <iosfwd>

namespace tracer_repl {
enum class LocalPerfMode { syscalls, cpu, watchpoint };
struct Watchpoint {
  std::uint32_t tid;
  std::uint64_t address;
  unsigned size;
  bool reads;
};
inline constexpr unsigned max_local_perf_seconds = 60;
inline constexpr unsigned max_local_perf_events = 8192;

void capture_local_perf(LocalPerfMode mode, std::uint32_t pid,
                        std::uint64_t start_time, unsigned seconds, unsigned limit,
                        const std::atomic<bool> &interrupted, std::ostream &out,
                        const Watchpoint *watchpoint = nullptr);
} // namespace tracer_repl

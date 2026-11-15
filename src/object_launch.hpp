#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <sys/types.h>

namespace tracer::object_launch {

class Child {
  pid_t pid_;
  int exec_status_;

  explicit Child(pid_t pid, int exec_status = 0) noexcept
      : pid_(pid), exec_status_(exec_status) {}

public:
  Child(const Child &) = delete;
  Child &operator=(const Child &) = delete;
  ~Child();

  static Child stop_on_exec(std::vector<std::string> &arguments, unsigned options,
                            std::chrono::steady_clock::time_point deadline,
                            const std::function<bool()> &cancelled = {});
  pid_t pid() const noexcept { return pid_; }
  int exec_status() const noexcept { return exec_status_; }
  pid_t release() noexcept { return std::exchange(pid_, -1); }
  void terminate() noexcept;
};

} // namespace tracer::object_launch

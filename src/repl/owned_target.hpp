#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace tracer_repl {
struct LaunchOptions {
  std::string executable, cwd, input, output, error;
  std::vector<std::string> arguments, environment;
};

class OwnedTarget {
  int pid_ = -1;
  int gate_ = -1;
  int exit_status_ = -1;
  std::string executable_, output_, error_;

  OwnedTarget() = default;
  void close_gate() noexcept;
  std::string exit_description() const;

public:
  ~OwnedTarget();
  OwnedTarget(const OwnedTarget &) = delete;
  OwnedTarget &operator=(const OwnedTarget &) = delete;

  static std::unique_ptr<OwnedTarget> start(const LaunchOptions &options, int tracer_pid);
  bool running();
  void wait_ready(unsigned seconds, const std::atomic<bool> &cancelled);
  void resume();
  void terminate();

  std::uint32_t pid() const { return static_cast<std::uint32_t>(pid_); }
  bool paused() const { return gate_ >= 0; }
  const std::string &executable() const { return executable_; }
  const std::string &output_path() const { return output_; }
  const std::string &error_path() const { return error_; }
};
} // namespace tracer_repl

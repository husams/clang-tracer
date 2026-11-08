#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tracer_repl {
struct LaunchOptions {
  std::string executable, cwd, input, output, error, preload;
  std::vector<std::string> arguments, environment;
  bool supervise = false;
};

class OwnedTarget {
  int pid_ = -1;
  int gate_ = -1;
  int exit_status_ = -1;
  bool calls_enabled_ = false;
  bool call_pending_ = false;
  [[maybe_unused]] bool call_protocol_failed_ = false;
  std::string executable_, output_, error_;
  std::mutex lifecycle_;
  std::thread supervisor_;
  bool supervise_ = false;

  OwnedTarget() = default;
  void close_gate() noexcept;
  void supervise_group();
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
  void start_call(const std::string &name, const std::vector<std::string> &arguments);
  std::string call_result(unsigned seconds, const std::atomic<bool> &cancelled);

  std::uint32_t pid() const { return static_cast<std::uint32_t>(pid_); }
  bool paused() const { return gate_ >= 0; }
  bool call_pending() const { return call_pending_; }
  bool calls_enabled() const { return paused() && calls_enabled_; }
  const std::string &executable() const { return executable_; }
  const std::string &output_path() const { return output_; }
  const std::string &error_path() const { return error_; }
};
} // namespace tracer_repl

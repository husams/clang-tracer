#pragma once
#include <cstdint>
#include <string>

namespace tracer_repl {
class ServerProcess {
  int pid_ = -1;
  bool terminating_ = false;
  int exit_status_ = 0;
  std::string directory_;
  std::string endpoint_;

public:
  ~ServerProcess();
  void start(const std::string &executable, const std::string &agent,
             unsigned timeout_seconds = 30,
             std::uint32_t context_event_bytes = 0);
  void stop(unsigned timeout_seconds = 30);
  bool running();

  int pid() const { return pid_; }

  const std::string &endpoint() const { return endpoint_; }

  std::string diagnostics() const;
};

std::string executable_path();
std::string find_server(const std::string &override_path);
std::string read_source(const std::string &path);
} // namespace tracer_repl

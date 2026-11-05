#pragma once

#include <memory>
#include <string>
#include <vector>

namespace tracer_repl {
struct AnalysisReply {
  int status;
  std::string output;
};

// Only the disposable checker parses untrusted, potentially incomplete input.
AnalysisReply run_cpp_analysis(const std::string& helper, const char* mode,
                               const std::vector<std::string>& arguments,
                               const std::string& history, const std::string& source);

// Retains accepted syntax in a helper; each candidate is parsed in a fork.
// Destroying/resetting the session terminates only its own helper group.
class CppCheckSession {
  struct Impl;
  std::unique_ptr<Impl> impl_;

public:
  CppCheckSession();
  ~CppCheckSession();
  AnalysisReply check(const std::string& helper, const std::vector<std::string>& arguments,
                      const std::string& history, const std::string& source);
};
} // namespace tracer_repl

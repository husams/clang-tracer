#pragma once

#include <string>
#include <vector>

namespace tracer_repl {
struct AnalysisReply {
  int status;
  std::string output;
};

// Only the disposable checker parses untrusted, potentially incomplete input.
AnalysisReply run_cpp_analysis(const std::string &helper, const char *mode,
                               const std::vector<std::string> &arguments,
                               const std::string &history,
                               const std::string &source);
} // namespace tracer_repl

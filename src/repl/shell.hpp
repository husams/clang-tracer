#pragma once
#include <string>
#include <vector>

namespace tracer_repl {
struct ShellOutput {
  int status;
  std::string output;
  std::string error;
};
// Explicit user-requested shell execution; never used for compilation
// databases.
ShellOutput run_shell(const std::string &command,
                      const std::string &directory = {});
std::vector<std::string> split_compiler_flags(const std::string &text);
} // namespace tracer_repl

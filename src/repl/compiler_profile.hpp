#pragma once
#include <string>
#include <vector>

namespace tracer_repl {
struct DatabaseProfile {
  std::string source;
  std::vector<std::string> arguments;
};
DatabaseProfile compilation_database_profile(const std::string &database,
                                             const std::string &file);
} // namespace tracer_repl

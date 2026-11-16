#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace tracer_repl {
struct DatabaseProfile {
  std::string source;
  std::vector<std::string> arguments;
};
enum class DatabaseSelection { SameDirectory, ExactOrUnique, Exact };

std::vector<std::string> resolve_plugin_flag_paths(const std::vector<std::string>& flags,
                                                   const std::filesystem::path& directory);

DatabaseProfile
compilation_database_profile(const std::string& database, const std::string& file,
                             DatabaseSelection selection = DatabaseSelection::SameDirectory);
} // namespace tracer_repl

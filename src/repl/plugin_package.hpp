#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace tracer_repl {
struct PluginPackage {
  std::string id;
  std::filesystem::path directory;
  std::filesystem::path include;
  std::vector<std::filesystem::path> sources;
  std::filesystem::path compile_commands;
  std::filesystem::path translation_unit;
  std::vector<std::string> extra_flags;
};

std::filesystem::path installed_plugin_directory();
std::optional<PluginPackage> read_plugin_package(const std::filesystem::path& manifest);
} // namespace tracer_repl

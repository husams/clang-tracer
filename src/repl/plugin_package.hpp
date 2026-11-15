#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace tracer_repl {
enum class PluginComponent { Repl, Agent };

struct PluginPackage {
  std::string id;
  std::filesystem::path directory;
  std::filesystem::path include;
  std::vector<std::filesystem::path> sources;
  std::filesystem::path compile_commands;
  std::filesystem::path translation_unit;
  std::vector<std::string> extra_flags;
  std::string executable;
};

std::filesystem::path installed_plugin_directory();
std::vector<std::filesystem::path> installed_plugin_manifests();
std::optional<PluginPackage> read_plugin_package(const std::filesystem::path& manifest,
                                                 PluginComponent component = PluginComponent::Repl);
} // namespace tracer_repl

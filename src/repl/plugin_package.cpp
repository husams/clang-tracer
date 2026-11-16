#include "plugin_package.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/JSON.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../native_plugins.hpp"

namespace tracer_repl {
namespace {
namespace fs = std::filesystem;

class Manifest {
  llvm::StringRef input_;

  void skip_space() { input_ = input_.ltrim(" \t"); }

  void skip_array_space() {
    while (true) {
      input_ = input_.ltrim();
      if (!input_.starts_with("#")) {
        return;
      }
      next_line();
    }
  }

public:
  explicit Manifest(llvm::StringRef input)
      : input_(input) {}

  bool empty() const { return input_.empty(); }

  llvm::StringRef line() const { return input_.split('\n').first.trim(); }

  void next_line() { input_ = input_.split('\n').second; }

  bool key(llvm::StringRef name) {
    auto value = input_.ltrim(" \t");
    if (!value.consume_front(name) || !value.ltrim(" \t").starts_with("=")) {
      return false;
    }
    input_ = value.ltrim(" \t").drop_front();
    return true;
  }

  std::string string() {
    skip_space();
    if (input_.empty()) {
      throw std::invalid_argument("expected a TOML string");
    }
    const char quote = input_.front();
    if (quote != '"' && quote != '\'') {
      throw std::invalid_argument("expected a TOML string");
    }
    for (size_t end = 1; end < input_.size(); ++end) {
      const char ch = input_[end];
      if (ch == '\n' || ch == '\r') {
        break;
      }
      if (quote == '"' && ch == '\\') {
        if (end + 1 >= input_.size() || input_[end + 1] == '\n') {
          break;
        }
        ++end;
        continue;
      }
      if (ch != quote) {
        continue;
      }
      const auto token = input_.take_front(end + 1);
      input_ = input_.drop_front(end + 1);
      if (quote == '\'') {
        return token.drop_front().drop_back().str();
      }
      auto parsed = llvm::json::parse(token);
      if (!parsed) {
        throw std::invalid_argument(llvm::toString(parsed.takeError()));
      }
      const auto value = parsed->getAsString();
      if (!value || value->contains('\0')) {
        throw std::invalid_argument("invalid manifest string");
      }
      return value->str();
    }
    throw std::invalid_argument("unterminated manifest string");
  }

  std::vector<std::string> strings() {
    skip_space();
    if (!input_.consume_front("[")) {
      throw std::invalid_argument("expected an array of TOML strings");
    }
    std::vector<std::string> result;
    while (true) {
      skip_array_space();
      if (input_.consume_front("]")) {
        return result;
      }
      result.push_back(string());
      skip_array_space();
      if (input_.consume_front("]")) {
        return result;
      }
      if (!input_.consume_front(",")) {
        throw std::invalid_argument("expected a comma in manifest array");
      }
    }
  }

  void end_value() {
    const auto remainder = line();
    if (!remainder.empty() && !remainder.starts_with("#")) {
      throw std::invalid_argument("unexpected text after manifest value");
    }
    next_line();
  }
};

void validate_package_tree(const fs::path& root) {
  for (const auto& entry : fs::recursive_directory_iterator(root)) {
    const auto& path = entry.path();
    struct stat info{};
    if (::lstat(path.c_str(), &info) || info.st_uid != ::geteuid() ||
        (!S_ISLNK(info.st_mode) && (info.st_mode & 0022))) {
      throw std::runtime_error("plugin package entry must be controlled by its owner: " +
                               path.string());
    }
    if (S_ISLNK(info.st_mode)) {
      const auto relative = fs::canonical(path).lexically_relative(root);
      if (relative.empty() || *relative.begin() == "..")
        throw std::runtime_error("plugin package symlink must stay inside the package: " +
                                 path.string());
    } else if (!S_ISDIR(info.st_mode) && !S_ISREG(info.st_mode)) {
      throw std::runtime_error("plugin package entry must be a regular file or directory: " +
                               path.string());
    }
  }
}

fs::path package_source(const fs::path& root, const std::string& source) {
  if (source.empty() || fs::path(source).is_absolute() || source.find('\0') != source.npos) {
    throw std::invalid_argument("plugin source must be relative to the package");
  }
  std::error_code error;
  const auto path = fs::canonical(root / source, error);
  if (error) {
    throw std::invalid_argument("cannot resolve plugin source " + source + ": " + error.message());
  }
  const auto relative = path.lexically_relative(root);
  if (relative.empty() || *relative.begin() == ".." || !fs::is_regular_file(path)) {
    throw std::invalid_argument("plugin source must be a file inside the package");
  }
  return path;
}
} // namespace

fs::path installed_plugin_directory() {
  if (const auto* data = std::getenv("XDG_DATA_HOME"); data && fs::path(data).is_absolute()) {
    return fs::path(data) / "clang-tracer/plugins";
  }
  if (const auto* home = std::getenv("HOME"); home && fs::path(home).is_absolute()) {
    return fs::path(home) / ".local/share/clang-tracer/plugins";
  }
  return {};
}

std::vector<fs::path> installed_plugin_manifests() {
  auto directory = installed_plugin_directory();
  if (directory.empty() || !fs::exists(directory)) {
    return {};
  }
  directory = tracer::private_plugin_directory(directory, false, false);
  std::vector<fs::path> manifests;
  for (const auto& entry : fs::directory_iterator(directory)) {
    const auto file = entry.path() / "plugin.toml";
    if (fs::is_directory(entry.symlink_status()) && fs::is_regular_file(fs::symlink_status(file))) {
      manifests.push_back(file);
    }
  }
  std::sort(manifests.begin(), manifests.end());
  return manifests;
}

std::optional<PluginPackage> read_plugin_package(const fs::path& manifest,
                                                 PluginComponent component) {
  const auto directory = tracer::private_plugin_directory(manifest.parent_path(), false, false);
  struct stat info{};
  if (::lstat(manifest.c_str(), &info) || !S_ISREG(info.st_mode) || info.st_uid != ::geteuid() ||
      (info.st_mode & 0022)) {
    throw std::runtime_error("plugin manifest must be controlled by its owner");
  }
  std::ifstream file(manifest);
  if (!file) {
    throw std::runtime_error("cannot read plugin manifest: " + manifest.string());
  }
  const std::string text{std::istreambuf_iterator<char>(file), {}};
  Manifest reader(text);
  PluginPackage package;
  package.directory = directory;
  std::vector<std::string> sources;
  bool supported_sdk = true;
  const bool agent = component == PluginComponent::Agent;
  const auto section = agent ? "[agent]" : "[repl]";
  bool has_component = false;
  bool in_component = false;
  bool in_target = false;
  bool at_root = true;
  while (!reader.empty()) {
    const auto line = reader.line();
    if (line.starts_with("[")) {
      at_root = false;
      const auto name = line.split('#').first.trim();
      in_component = name == section;
      in_target = agent && name == "[agent.target]";
      has_component |= in_component;
    } else if (at_root && reader.key("id")) {
      package.id = reader.string();
      reader.end_value();
      continue;
    } else if (at_root && reader.key("sdk")) {
      const auto sdk = reader.line().split('#').first.trim();
      supported_sdk = sdk == "1" || sdk == "2";
    } else if (in_target && reader.key("executable")) {
      package.executable = reader.string();
      reader.end_value();
      continue;
    } else if (in_component) {
      if (reader.key("sources")) {
        sources = reader.strings();
      } else if (reader.key("extra_flags")) {
        package.extra_flags = reader.strings();
      } else if (reader.key("compile_commands")) {
        const fs::path path = reader.string();
        if (agent && !path.is_absolute()) {
          throw std::invalid_argument("agent.compile_commands must be an absolute target path");
        }
        package.compile_commands = directory / path;
      } else if (reader.key("translation_unit")) {
        const fs::path path = reader.string();
        if (agent && !path.is_absolute()) {
          throw std::invalid_argument("agent.translation_unit must be an absolute target path");
        }
        package.translation_unit = directory / path;
      } else {
        reader.next_line();
        continue;
      }
      reader.end_value();
      continue;
    }
    reader.next_line();
  }
  if (!has_component) {
    return std::nullopt;
  }
  if (!supported_sdk) {
    throw std::invalid_argument("unsupported plugin SDK version");
  }
  if (package.id.empty() || sources.empty()) {
    throw std::invalid_argument(std::string("plugin manifest needs an id and ") + section +
                                " sources");
  }
  if (agent && (!fs::path(package.executable).is_absolute() ||
                package.executable.find('\0') != std::string::npos)) {
    throw std::invalid_argument("agent.target.executable must be an absolute target path");
  }
  validate_package_tree(directory);
  for (const auto& source : sources) {
    package.sources.push_back(package_source(directory, source));
  }
  const auto include = directory / "include";
  if (fs::is_directory(fs::symlink_status(include))) {
    package.include = include;
  }
  return package;
}
} // namespace tracer_repl

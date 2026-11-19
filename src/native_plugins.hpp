#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include "tracer/compat.hpp"
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "agent_plugin_observers.hpp"

namespace tracer {
// Keep local compilation and target-side offered-object validation identical.
inline std::vector<std::string>
native_plugin_compiler_arguments(const std::string& resource_directory,
                                 const std::vector<std::string>& platform,
                                 const std::vector<std::string>& user = {}) {
  std::vector<std::string> arguments{"-std=c++17", "-fPIC", "-O0", "-resource-dir",
                                     resource_directory};
  arguments.insert(arguments.end(), platform.begin(), platform.end());
  arguments.insert(arguments.end(), user.begin(), user.end());
  arguments.emplace_back("-fno-access-control");
  return arguments;
}

std::filesystem::path private_plugin_directory(const std::filesystem::path& directory, bool create,
                                               bool owner_only = true);

struct NativePluginFile {
  std::filesystem::path path;
  std::string_view contents;
};

struct NativePluginSource {
  std::filesystem::path path;
  std::optional<std::string_view> contents;
  std::string_view sdk;
  std::filesystem::path sdk_file;
  std::string_view cpp_sdk;
  ::tracer::compat::span<const NativePluginFile> files{};
  std::string_view agent_sdk{};
};

struct NativePluginUnit {
  NativePluginSource source;
  std::vector<std::string> arguments;
};

struct NativePluginSymbol {
  std::string name;
  std::uint64_t address;
  bool callable;
};

struct NativePluginObject {
  std::string fingerprint;
  std::string checksum;
  std::string bytes;
};

struct NativePluginObjectView {
  std::string_view fingerprint;
  std::string_view checksum;
  std::string_view bytes;
};

class NativePlugins {
  struct Impl;
  std::unique_ptr<Impl> impl_;
  bool allow_observers_;

public:
  struct LoadResult {
    std::uint64_t generation;
    bool cache_hit;
    bool from_repl;
  };
  struct Info {
    std::string id;
    std::filesystem::path source;
    std::uint64_t generation;
  };

  explicit NativePlugins(bool allow_observers = false);
  ~NativePlugins();
  LoadResult load(const std::string &id, NativePluginSource source,
                  const std::vector<std::string> &profile,
                  const std::filesystem::path &cache, bool reload);
  std::optional<LoadResult>
  load_offered(const std::string &id, NativePluginSource source,
               const std::vector<std::string> &profile,
               const std::filesystem::path &cache, bool reload,
               NativePluginObjectView offer);
  LoadResult load_package(const std::string& id, const std::vector<NativePluginUnit>& units,
                          const std::filesystem::path& include, const std::filesystem::path& cache,
                          bool reload);
  bool has_observers() const noexcept;
  void notify_return(const agent::ReturnSample& sample, void* context,
                     PluginEventPublisher publish) const noexcept;
  std::vector<NativePluginSymbol> public_symbols(const std::string& id) const;
  void discard(const std::string& id);
  NativePluginObject prepare(NativePluginSource source,
                             const std::vector<std::string> &profile,
                             const std::filesystem::path &cache);
  Info info(const std::string &id) const;
  std::vector<Info> list() const;
  std::vector<std::pair<std::string, std::string>>
  commands(const std::string &id) const;
  std::uint64_t run(const std::string &id, const std::string &command) const;
};
} // namespace tracer

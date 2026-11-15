#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tracer {
struct NativePluginSource {
  std::filesystem::path path;
  std::optional<std::string_view> contents;
  std::string_view sdk;
  std::filesystem::path sdk_file;
  std::string_view cpp_sdk;
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

  NativePlugins();
  ~NativePlugins();
  LoadResult load(const std::string &id, NativePluginSource source,
                  const std::vector<std::string> &profile,
                  const std::filesystem::path &cache, bool reload);
  std::optional<LoadResult>
  load_offered(const std::string &id, NativePluginSource source,
               const std::vector<std::string> &profile,
               const std::filesystem::path &cache, bool reload,
               NativePluginObjectView offer);
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

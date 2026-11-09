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
};

class NativePlugins {
  struct Impl;
  std::unique_ptr<Impl> impl_;

public:
  struct LoadResult {
    std::uint64_t generation;
    bool cache_hit;
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
  Info info(const std::string &id) const;
  std::vector<Info> list() const;
  std::vector<std::pair<std::string, std::string>>
  commands(const std::string &id) const;
  std::uint64_t run(const std::string &id, const std::string &command) const;
};
} // namespace tracer

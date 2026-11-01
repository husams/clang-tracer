#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace tracer::context_package {

struct PackageLineage {
  std::array<std::uint8_t, 16> id{};
  std::string package_root;
  bool retained = false;
  std::string diagnostic;
};

// The package root is used only to find engine-owned lineage metadata. UUIDs
// are generated randomly and stored in that metadata; neither source text nor
// paths are used to derive an identity.
class PackageCatalog final {
public:
  static PackageLineage admit_source(
      const std::filesystem::path &source_path,
      const std::filesystem::path &project_root = {});
  static PackageLineage resolve_source(
      const std::filesystem::path &source_path);
  static PackageLineage distinct_unverified(std::string_view reason);
};

} // namespace tracer::context_package

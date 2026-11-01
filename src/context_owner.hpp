#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "context_analysis.hpp"

namespace clang {
class ASTUnit;
}

namespace tracer {
struct ContextTypeProvenance final {
  std::string canonical_type;
  std::array<std::uint8_t, 16> source_lineage_id{};

  bool operator==(const ContextTypeProvenance&) const = default;
};

struct GeneratedOwnerState {
  ContextDependency type;
  std::string construct;
  std::string destroy;
  std::string address;
  std::string serialize;
  std::uint64_t initializer_identity = 0;
  std::string lifecycle_fingerprint;
  // The state family follows the package that defines the state type, while
  // scope registration identity follows the package that defines its tag.
  std::array<std::uint8_t, 16> state_lineage_id{};
  std::array<std::uint8_t, 16> scope_lineage_id{};
  // Every user-defined record reachable from a generic payload participates
  // in owner reuse compatibility, even when its root is a system container.
  std::vector<ContextTypeProvenance> payload_provenance;
};

struct GeneratedContextOwner {
  std::string compilation_copy;
  std::vector<GeneratedOwnerState> states;
  std::string error;
};

// Compiles state factories/erasure managers once in the owner generation.
// This only renders a compilation copy; its real emitted support closure must
// still pass private linking/ABI/native-boundary/unwind admission before use.
GeneratedContextOwner generate_context_owner(clang::ASTUnit&, bool serializers = false);
} // namespace tracer

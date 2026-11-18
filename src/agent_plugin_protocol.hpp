#pragma once

#include <cstddef>
#include <cstdint>

namespace tracer {
// Protobuf parsing accepts a signed int byte count.
inline constexpr std::size_t kMaxAgentPluginSource = INT32_MAX;
inline constexpr std::size_t kMaxAgentPluginOffer = kMaxAgentPluginSource;

enum class AgentPluginOperation : std::uint32_t { Load, Reload, Run, Clear, LoadPackage, ReloadPackage };
inline constexpr int kAgentPluginReusedReplObject = 1;

struct AgentPluginCall {
  std::uint32_t version = 1;
  AgentPluginOperation operation{};
  std::uint64_t source_size = 0;
  char session_id[64]{};
  char plugin_id[128]{};
  char command[128]{};
  std::uint64_t generation = 0;
  std::uint64_t value = 0;
  bool cache_hit = false;
  char error[2048]{};
};
} // namespace tracer

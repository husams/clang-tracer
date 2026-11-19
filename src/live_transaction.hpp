#pragma once

#include "live_update.hpp"
#include "live_agent_protocol.hpp"

#include <map>
#include <optional>
#include <set>

namespace tracer::live {

struct JitEntryAddress {
  std::uintptr_t destination = 0;
  std::uintptr_t mapping_begin = 0;
  std::size_t mapping_size = 0;
};

struct ActiveEntry {
  Entry binding;
  std::string installed_bytes;
  std::uintptr_t destination = 0;
  std::uintptr_t mapping_begin = 0;
  std::size_t mapping_size = 0;
  bool requires_drain = false;
  std::uintptr_t mock_original = 0;
  std::optional<JitEntryAddress> native_mock_alternate;
};

struct Revision {
  std::uint64_t parent = 0;
  std::uint64_t managed = 0;
  bool managed_native = false;
  std::map<std::uintptr_t, ActiveEntry> entries;
  std::map<std::string, SourceFile> sources;
  std::map<std::string, std::set<std::string>> private_closures;
};

struct ManagedTransition {
  std::uint64_t before;
  std::uint64_t after;
  std::uint64_t limit;
  bool reverting;
  std::uint64_t recovery_revision = 0;
  std::optional<AgentRequest> acknowledgement{};
};

struct Transaction {
  std::uint64_t active = 0;
  std::uint64_t pending_extension = 0;
  std::optional<ManagedTransition> managed_transition;
  std::uint64_t last_committed = 0;
  std::uint64_t retained = 0;
  std::uint64_t limit = 16;
  std::map<std::uint64_t, Revision> revisions{{0, {}}};
};

} // namespace tracer::live

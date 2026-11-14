#pragma once

#include "live_update.hpp"

#include <map>

namespace tracer::live {

struct ActiveEntry {
  Entry binding;
  std::string installed_bytes;
  std::uintptr_t destination = 0;
  std::uintptr_t mapping_begin = 0;
  std::size_t mapping_size = 0;
};

struct Revision {
  std::uint64_t parent = 0;
  std::uint64_t managed = 0;
  bool managed_native = false;
  std::map<std::uintptr_t, ActiveEntry> entries;
  std::map<std::string, SourceFile> sources;
  SafePoint safe_point = SafePoint::independent_entry;
};

struct Transaction {
  std::uint64_t active = 0;
  std::uint64_t pending_extension = 0;
  std::uint64_t last_committed = 0;
  std::uint64_t retained = 0;
  std::uint64_t limit = 16;
  std::map<std::uint64_t, Revision> revisions{{0, {}}};
};

} // namespace tracer::live

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "live_update.hpp"

namespace tracer::live {
struct LiveDataSelection {
  std::string module;
  std::optional<std::uintptr_t> address;
  std::vector<std::string> scan_ids;
};

struct LiveDataUpdateCommand {
  std::string file;
  std::string discover_file, discover_function;
  LiveDataSelection selection;
};

struct LiveHeapScanCommand {
  std::vector<std::string> types;
  std::string module, discover_file, discover_function;
};

// Arguments after the %live operation name, with REPL quoting already decoded.
LiveDataUpdateCommand parse_live_data_update(const std::vector<std::string>&);
LiveHeapScanCommand parse_live_heap_scan(const std::vector<std::string>&);

struct LiveDataTarget {
  std::uint64_t process_start = 0;
  std::string module;
  std::string build_id;
};

struct LiveDataDiscovery {
  JitRequest source;
  std::string function;
};

enum class LiveObjectOutcome { updated, skipped, failed };

struct LiveObjectResult {
  LiveObjectOutcome outcome = LiveObjectOutcome::skipped;
  std::string id, type, root, detail;
  std::uintptr_t address = 0;
};

struct LiveDataReport {
  std::vector<std::string> roots;
  std::vector<LiveObjectResult> objects;
  std::vector<std::string> limitations;
  std::size_t duplicates = 0;
};

struct LiveHeapCandidate {
  std::string id, type, module, build_id, root, evidence;
  std::uintptr_t address = 0;
};

struct LiveHeapReport {
  std::vector<LiveHeapCandidate> candidates;
  std::vector<std::string> limitations;
};

// Run on the target agent's control thread, without suspending peer threads.
// Keep one instance per attached target session. The transport supplies
// build-matched compile arguments and exact imports via JitRequest.
class LiveDataSession {
public:
  LiveDataSession();
  ~LiveDataSession();
  LiveDataSession(const LiveDataSession&) = delete;
  LiveDataSession& operator=(const LiveDataSession&) = delete;

  LiveDataReport update_data(const LiveDataTarget&, const JitRequest& update,
                             const std::optional<LiveDataDiscovery>&,
                             const LiveDataSelection& = {});
  LiveHeapReport scan_heap(const LiveDataTarget&, const std::vector<std::string>& types,
                           const std::optional<LiveDataDiscovery>& = std::nullopt);

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace tracer::live

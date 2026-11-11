#pragma once

#include "snapshot_runtime_limits.hpp"
#include "tracer.pb.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>

// Every limit of followed-member capture, in one place. The defaults here are
// the only place they are written; every other layer asks for the value.
namespace tracer {

struct SnapshotLimits {
  // Prepare time.
  std::uint32_t follow_depth = kDefaultFollowDepth;  // Levels of pointees followed below a captured record.
  std::uint32_t follow_members = 48;  // Followed members per probe (about 160 bytes of stack each).
  std::uint32_t record_members = 32;  // Automatic members discovered per record.
  std::uint32_t nested_depth = 2;     // Nested by-value records and smart pointers searched.
  // Run time.
  std::uint32_t string_bytes = 4000;  // Bytes of one captured C string.
  std::uint32_t vector_elements = 4096;  // Elements of a vector in a followed member.
  std::uint32_t checked_reads = 256;  // Checked reads one followed member may spend.
  std::uint32_t member_reserve = 8192;  // Arena bytes entry members leave for later roots.
  std::uint32_t arena_bytes = kDefaultArenaBytes;  // Per-thread snapshot storage.
  std::uint32_t slice_bytes = kDefaultSliceBytes;  // Bytes of one snapshot payload.
};

struct LimitSpec {
  const char *name;      // Option name without the leading dashes.
  const char *env;       // Environment variable read by the prepare tool.
  bool prepare_time;
  std::uint32_t minimum, maximum;  // Inclusive; `unlimited` selects the maximum.
  const char *meaning;
};

// Range of each option. A maximum is either the largest value the 32-bit option
// holds (`unlimited`) or a constraint that something else in the tracer imposes:
//   follow-depth    kMaxFollowDepth: followed members nest two protobuf messages per
//                   level on top of kEventBaseDepth, the event writer holds
//                   kMaxMessageDepth of them (bounded_proto.hpp) and the protobuf
//                   parser refuses more than 100 levels (see snapshot_runtime_limits.hpp).
//   snapshot-slice  one payload is a single string field of the event, which the
//                   event writer bounds at 64 KiB (context_capture.cpp limits.string_bytes).
//   string-bytes    a C string is carried in one payload, so it follows the slice.
// The floors of snapshot-arena and snapshot-slice are the built-in sizes: the
// per-thread storage is shared by every probe of the process and only grows.
inline constexpr LimitSpec kLimitSpecs[] = {
    {"follow-depth", "TRACER_PREPARE_FOLLOW_DEPTH", true, 0, kMaxFollowDepth,
     "levels of pointees followed below a captured record (0 follows none)"},
    {"follow-members", "TRACER_PREPARE_FOLLOW_MEMBERS", true, 0, kUnlimited,
     "followed members per probe, policy-named ones first (about 160 bytes of wrapper stack each)"},
    {"record-members", "TRACER_PREPARE_RECORD_MEMBERS", true, 0, kUnlimited,
     "automatically discovered pointer members per record"},
    {"nested-depth", "TRACER_PREPARE_NESTED_DEPTH", true, 0, kUnlimited,
     "levels of by-value records and smart pointers searched for pointer members"},
    {"string-bytes", "TRACER_PREPARE_STRING_BYTES", false, 0, kMaxSliceBytes,
     "bytes kept of one string in a followed member; bounded by the slice"},
    {"vector-elements", "TRACER_PREPARE_VECTOR_ELEMENTS", false, 0, kUnlimited,
     "elements of a vector in a followed member; a larger vector is not followed"},
    {"checked-reads", "TRACER_PREPARE_CHECKED_READS", false, 1, kUnlimited,
     "checked memory reads one followed member may spend (each is a system call)"},
    {"snapshot-reserve", "TRACER_PREPARE_SNAPSHOT_RESERVE", false, 0, kUnlimited,
     "bytes of the per-thread storage that members captured at entry leave for later roots"},
    {"snapshot-arena", "TRACER_PREPARE_SNAPSHOT_ARENA", false, kDefaultArenaBytes, kUnlimited,
     "per-thread storage for snapshots; only grows, the largest request of any probe wins"},
    {"snapshot-slice", "TRACER_PREPARE_SNAPSHOT_SLICE", false, kDefaultSliceBytes, kMaxSliceBytes,
     "bytes of one snapshot payload; only grows, the largest request of any probe wins"},
};

inline const LimitSpec *find_limit(std::string_view name) {
  for (const auto &spec : kLimitSpecs)
    if (name == spec.name)
      return &spec;
  return nullptr;
}

// "off" is 0 where 0 is allowed; "unlimited" is the largest safe value.
inline std::uint32_t parse_limit(const LimitSpec &spec, const std::string &text) {
  if (text == "unlimited")
    return spec.maximum;
  if (text == "off" && spec.minimum == 0)
    return 0;
  std::uint64_t value = 0;
  if (text.empty() || text.size() > 10 ||
      !std::all_of(text.begin(), text.end(), [](unsigned char c) { return c >= '0' && c <= '9'; }))
    throw std::invalid_argument(std::string("invalid --") + spec.name + ": " + text);
  for (const char c : text)
    value = value * 10 + static_cast<std::uint64_t>(c - '0');
  if (value < spec.minimum || value > spec.maximum)
    throw std::invalid_argument(std::string("--") + spec.name + " must be between " +
                                std::to_string(spec.minimum) + " and " +
                                std::to_string(spec.maximum) + " (or unlimited): " + text);
  return static_cast<std::uint32_t>(value);
}

#define TRACER_SNAPSHOT_LIMIT_FIELDS(X)                                                      \
  X(follow_depth, "follow-depth") X(follow_members, "follow-members")                       \
  X(record_members, "record-members") X(nested_depth, "nested-depth")                       \
  X(string_bytes, "string-bytes") X(vector_elements, "vector-elements")                     \
  X(checked_reads, "checked-reads") X(member_reserve, "snapshot-reserve")                   \
  X(arena_bytes, "snapshot-arena") X(slice_bytes, "snapshot-slice")

// Effective limits: defaults, then the prepared package, then the probe.
inline SnapshotLimits resolve_limits(const clangtracer::v1::SnapshotLimits *prepared,
                                     const clangtracer::v1::SnapshotLimits *probe) {
  SnapshotLimits result;
#define TRACER_APPLY(field, name)                                                            \
  if (prepared && prepared->has_##field()) result.field = prepared->field();                 \
  if (probe && probe->has_##field()) result.field = probe->field();
  TRACER_SNAPSHOT_LIMIT_FIELDS(TRACER_APPLY)
#undef TRACER_APPLY
  // The arena holds at least one slice, and a string must fit its payload (the
  // payload adds a tag, a length and the record framing: well under 96 bytes).
  result.arena_bytes = std::max(result.arena_bytes, result.slice_bytes);
  constexpr std::uint32_t overhead = 96;
  const std::uint32_t room = result.slice_bytes > overhead ? result.slice_bytes - overhead : 0;
  // The cut note of a shortened string names both options, since either may bind.
  result.string_bytes = std::min(result.string_bytes, room);
  return result;
}

// The built-in value of an option, by its dashed name.
inline std::uint32_t default_limit(std::string_view name) {
  const SnapshotLimits defaults;
#define TRACER_DEFAULT(field, option) if (name == option) return defaults.field;
  TRACER_SNAPSHOT_LIMIT_FIELDS(TRACER_DEFAULT)
#undef TRACER_DEFAULT
  return 0;
}

// Sets one option by its dashed name; false if the name is not a limit.
inline bool set_limit(clangtracer::v1::SnapshotLimits &limits, const std::string &name,
                      const std::string &text) {
  const auto *spec = find_limit(name);
  if (!spec)
    return false;
  const auto value = parse_limit(*spec, text);
#define TRACER_SET(field, option) if (name == option) limits.set_##field(value);
  TRACER_SNAPSHOT_LIMIT_FIELDS(TRACER_SET)
#undef TRACER_SET
  return true;
}

} // namespace tracer

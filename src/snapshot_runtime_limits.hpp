#pragma once
#include <atomic>
#include <cstdint>

// Constants and process-wide requests shared by the generated capture code, the
// per-thread storage and the event writer. The defaults of the user-visible
// options live in snapshot_limits.hpp; the values here are what the largest
// installed probe asked for, and the sizes that other parts of the tracer fix.
namespace tracer {

inline constexpr std::uint32_t kUnlimited = 0xFFFFFFFFu;
inline constexpr std::uint32_t kDefaultArenaBytes = 16 * 1024;
inline constexpr std::uint32_t kDefaultSliceBytes = 4096;
inline constexpr std::uint32_t kDefaultMemberLimit = 128;
inline constexpr std::uint32_t kDefaultFollowDepth = 3;
// One payload is a single string field of the event, which the event writer
// bounds at 64 KiB (context_capture.cpp, limits.string_bytes).
inline constexpr std::uint32_t kMaxSliceBytes = 64 * 1024;
// A followed member adds two nested messages (member, snapshot) to the event.
// The event writer allows kEventBaseDepth levels for the envelope and the record
// and two more per followed level (bounded_proto.hpp kMaxMessageDepth holds them).
// The binding constraint is the protobuf parser, which refuses a message nested
// more than 100 levels when the server and the REPL read the event: measured end
// to end (agent, server, REPL), a chain of followed members arrives up to 49
// levels in a result and 48 in an argument and the event is lost beyond that.
// kMaxFollowDepth keeps eight levels of slack below the measured 48 for the
// receiver and state roots and for wrapper messages that were not measured.
inline constexpr std::uint32_t kEventBaseDepth = 32;
inline constexpr std::uint32_t kMaxFollowDepth = 40;

// The storage grows to the arena size; a snapshot payload may use the slice.
inline std::atomic<std::uint32_t> &snapshot_arena_request() {
  static std::atomic<std::uint32_t> value{kDefaultArenaBytes};
  return value;
}
inline std::atomic<std::uint32_t> &snapshot_slice_request() {
  static std::atomic<std::uint32_t> value{kDefaultSliceBytes};
  return value;
}
inline std::atomic<std::uint32_t> &snapshot_text_request() {
  static std::atomic<std::uint32_t> value{kDefaultSliceBytes};
  return value;
}
// Followed members one snapshot may carry (--follow-members), and the levels of
// followed members an event may nest (--follow-depth, plus the root).
inline std::atomic<std::uint32_t> &snapshot_member_request() {
  static std::atomic<std::uint32_t> value{kDefaultMemberLimit};
  return value;
}
inline std::atomic<std::uint32_t> &snapshot_depth_request() {
  static std::atomic<std::uint32_t> value{kDefaultFollowDepth + 1};
  return value;
}
} // namespace tracer

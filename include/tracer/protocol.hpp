#pragma once
#include <cstdint>

namespace tracer {
inline constexpr std::uint32_t kProtocolVersion = 2;
// Version 13 adds asynchronous delivery accounting to the stop handshake.
// It reads a binary ProbeSpec at source_path, including optional
// Clang receiver context. Preparation and all-thread patch commits remain
// separate. Older agents reject the version before interpreting the new payload
// format.
inline constexpr std::uint32_t kBootstrapVersion = 13;
inline constexpr unsigned kMaxPatchBytes = 64;
inline constexpr unsigned kMaxArguments = 8;
inline constexpr unsigned kMaxProbes = 32;
inline constexpr unsigned kMaxStackDepth = 64;

enum class StackStatus : std::uint32_t {
  Disabled = 0,
  Complete = 1,
  Truncated = 2,
  Partial = 3
};
enum class FrameKind : std::uint32_t {
  Native = 0,
  TargetEntry = 1,
  JitBridge = 2
};
struct StackFrame {
  std::uint64_t pc = 0;
  std::uint64_t stack_pointer = 0;
  FrameKind kind = FrameKind::Native;
  // Agent-owned immutable strings (at most 256 bytes); empty means unknown.
  // Read these within the invocation, like the rest of the Call view.
  const char *function = "";
  const char *module = "";
  const char *file = "";
  std::uint32_t line = 0;          // Zero means no source line is available.
  std::uint64_t module_offset = 0; // ELF symbol lookup address before ASLR.
  bool application_frame =
      false; // Classified ELF owner/declaration, not arguments.
};
struct StackTrace {
  StackStatus status = StackStatus::Disabled;
  std::uint32_t count = 0;
  std::uint32_t requested_depth = 0;
  bool application_only = true;
  std::uint32_t hidden_frames = 0;
  std::uint32_t captured_depth =
      0; // Before filtering; depth/status cover this unwind.
  char detail[160]{};
  StackFrame frames[kMaxStackDepth]{};
};

// Numeric values match proto/tracer.proto; this is the native bootstrap ABI.
enum class ValueType : std::uint32_t {
  Unspecified = 0,
  Void = 1,
  Int32 = 2,
  Int64 = 3,
  UInt64 = 4,
  Float32 = 5,
  Float64 = 6,
  Pointer = 7,
  Object = 8
};
enum class CallableKind : std::uint32_t {
  Unspecified = 0,
  Function = 1,
  Method = 2
};
enum class TracePhase : std::uint32_t { Pre = 1, Post = 2 };

struct TraceValue {
  ValueType type{};
  std::uint32_t reserved = 0;
  std::int64_t signed_value = 0;
  std::uint64_t unsigned_value = 0;
  double real_value = 0;
};

struct CallbackPrintOptions {
  bool arguments = false;
  bool result = false;
};

struct TraceContext {
  std::uint64_t probe_id = 0;
  std::uint64_t object_address = 0;
  std::uint32_t argument_count = 0;
  std::uint32_t reserved = 0;
  TraceValue arguments[kMaxArguments]{};
  const char *argument_names[kMaxArguments]{};
  CallbackPrintOptions printing{};
  std::uint64_t call_id = 0;
  TracePhase phase = TracePhase::Pre;
  TraceValue result{};
  struct NamedValue {
    std::uint32_t kind = 0; // 1 = explicit argument, 2 = object field.
    std::uint32_t argument_index = 0;
    char name[64]{};
    TraceValue value{};
  };
  mutable std::uint32_t named_count = 0;
  mutable NamedValue named_values[16]{};
  // Entry caller state, recovered from Clang's forced wrapper frame pointer.
  // Only RBP/RSP/RIP are known initially; unsupported CFI stops explicitly.
  std::uint64_t wrapper_frame = 0;
  std::uint64_t caller_pc = 0;
  std::uint64_t caller_sp = 0;
  std::uint64_t caller_bp = 0;
  std::uint64_t target_entry = 0;
  std::uint64_t bridge_begin = 0;
  std::uint64_t bridge_end = 0;
  const TraceContext *stack_previous = nullptr;
  bool stack_application_only = true;
  void (*stack_capture)(const TraceContext *, unsigned,
                        bool) noexcept = nullptr;
  mutable StackTrace stack{};
};

struct Request {
  std::uint32_t version = kBootstrapVersion;
  CallableKind kind = CallableKind::Unspecified;
  ValueType result = ValueType::Unspecified;
  std::uint32_t argument_count = 0;
  ValueType arguments[kMaxArguments]{};
  std::uint64_t probe_id = 0;
  char session_id[64]{};
  char symbol[512]{};
  char source_path[1024]{};
  char event_socket[108]{};
  std::uint64_t original_entry = 0;
  std::uint64_t callback_address = 0;
  std::uint64_t wrapper_address = 0;
  std::uint64_t event_drops = 0;
  std::uint64_t callback_errors = 0;
  std::uint64_t events_sent = 0;
  std::uint32_t patch_size = 0;
  std::uint8_t original[kMaxPatchBytes]{};
  std::uint8_t installed[kMaxPatchBytes]{};
  char error[2048]{};
};
} // namespace tracer

extern "C" int tracer_agent_start(tracer::Request *request);
extern "C" int tracer_agent_stop(tracer::Request *request);

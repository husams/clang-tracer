#pragma once
#include <cstddef>
#include <cstdint>

namespace tracer {
inline constexpr std::uint32_t kProtocolVersion = 2;
// Version 16 replaces caller-provided callback origin text with an
// engine-assigned package lineage UUID. It reads a binary ProbeSpec at
// source_path, including optional Clang receiver context. Older agents reject
// the version before interpreting the expanded bootstrap request.
// Version 19 reports a prepared replacement's previous probe ID.
// Version 20 adds capture-time owned value snapshots to TraceValue.
// Version 21 verifies header-prepared scalar contracts without rescanning DWARF.
inline constexpr std::uint32_t kBootstrapVersion = 22;
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
  Object = 8,
  Bool = 9
};
enum class CallableKind : std::uint32_t {
  Unspecified = 0,
  Function = 1,
  Method = 2
};
enum class TracePhase : std::uint32_t { Pre = 1, Post = 2 };

struct ValueSnapshot {
  enum Status : std::uint32_t {
    Complete = 1,
    Unsupported = 2,
    LimitExceeded = 3,
    Failed = 4
  };
  const char *cpp_type = nullptr;
  const char *root_type = nullptr;
  const char *descriptor = nullptr;
  std::size_t descriptor_size = 0;
  const std::byte *payload = nullptr;
  std::size_t payload_size = 0;
  Status status = Unsupported;
  const char *detail = nullptr;
  const char *const *container_roles = nullptr;
  std::size_t container_role_count = 0;
  const char *const *record_types = nullptr;
  std::size_t record_type_count = 0;
};

struct TraceValue {
  ValueType type{};
  std::uint32_t reserved = 0;
  std::int64_t signed_value = 0;
  std::uint64_t unsigned_value = 0;
  double real_value = 0;
  const ValueSnapshot *snapshot = nullptr;
};

struct CallbackPrintOptions {
  bool arguments = false;
  bool result = false;
};

struct TraceContext {
  std::uint64_t probe_id = 0;
  std::uint64_t object_address = 0;
  const ValueSnapshot *receiver_snapshot = nullptr;
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
  // Shared-context delivery configuration. Kept in the bootstrap request so
  // the injected native agent uses the exact endpoint/profile selected by the
  // owning server session.
  char context_endpoint[256]{};
  char context_journal_dir[512]{};
  char context_recording_id[64]{};
  char context_sink_id[64]{};
  std::uint8_t source_lineage_id[16]{};
  std::uint32_t context_delivery_profile = 1; // 1=best-effort, 2=durable.
  std::uint64_t original_entry = 0;
  std::uint64_t callback_address = 0;
  std::uint64_t wrapper_address = 0;
  std::uint64_t event_drops = 0;
  std::uint64_t callback_errors = 0;
  std::uint64_t events_sent = 0;
  std::uint32_t patch_size = 0;
  std::uint8_t original[kMaxPatchBytes]{};
  std::uint8_t installed[kMaxPatchBytes]{};
  // Second native patch transaction for the supported pthread_create adapter.
  // Zero size means the agent could not prove the runtime's ABI/module bytes.
  std::uint64_t pthread_create_entry = 0;
  std::uint64_t pthread_create_trampoline = 0;
  std::uint64_t pthread_create_wrapper = 0;
  std::uint32_t pthread_create_patch_size = 0;
  std::uint32_t pthread_create_active_tasks = 0;
  bool context_required = false;
  std::uint8_t pthread_create_original[kMaxPatchBytes]{};
  std::uint8_t pthread_create_installed[kMaxPatchBytes]{};
  char error[2048]{};
  // Native PID identity for restart-safe shared-context event routing.
  // These fields are engine-populated from the attached Session.
  std::uint32_t context_native_pid = 0;
  std::uint64_t context_process_start_time = 0;
  // Agent-selected same-session probe to drain after replacement preparation.
  std::uint64_t replaced_probe_id = 0;
};

// Side-effect-free callback preparation request. The agent reads a serialized
// ProbeSpec from spec_path and writes a serialized ValidationReport to
// report_path. Level values match clangtracer::v1::ValidateProbeRequest.
struct ValidationRequest {
  std::uint32_t version = kBootstrapVersion;
  std::uint32_t level = 0; // 0=full compilation, 1=syntax only.
  char spec_path[1024]{};
  char report_path[1024]{};
  char error[2048]{};
};
} // namespace tracer

extern "C" int tracer_agent_start(tracer::Request *request);
extern "C" int tracer_agent_stop(tracer::Request *request);
extern "C" int tracer_agent_validate(tracer::ValidationRequest *request);

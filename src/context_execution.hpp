#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tracer::context_execution {

enum class Mode : std::uint8_t {
  Application,
  Tracking,
  Support,
  LifecycleNoTrace,
  TransportNoTrace
};

enum class Callee : std::uint8_t {
  ApplicationEndpoint,
  AdmittedSupport,
  AdmittedLifecycleBoundary,
  TransportWorker
};

enum class EntryDecision : std::uint8_t { Trace, Bypass, Reject };
enum class Severity : std::uint8_t { info, warning, error, critical };
enum class Phase : std::uint8_t {
  registration,
  construction,
  pre,
  target,
  post,
  access,
  capture,
  exporting,
  lifecycle,
  transport,
  count
};
enum class Action : std::uint8_t {
  continued,
  callback_skipped,
  event_dropped,
  scope_unchanged,
  call_rejected,
  lifecycle_ignored,
  transport_paused
};
enum class Reason : std::uint8_t {
  no_active_hook,
  missing_state,
  incompatible_state,
  invalid_ownership,
  access_failure,
  constructor_failure,
  hook_failure,
  capture_failure,
  export_queue_full,
  schema_failure,
  transport_failure,
  unsupported_effect,
  lifecycle_access,
  reentrant_application_call,
  invalid_transition,
  count
};

inline constexpr std::size_t kDiagnosticCapacity = 128;
inline constexpr std::size_t kEventNameCapacity = 48;

struct Diagnostic final {
  Severity severity = Severity::warning;
  Reason reason = Reason::no_active_hook;
  Phase phase = Phase::access;
  Action action = Action::continued;
  std::uint64_t occurrences = 1;
  std::uint64_t call_id = 0;
  std::uint64_t event_id = 0;
  std::array<char, kEventNameCapacity> event_name{};
  bool aggregate = false;
  bool has_correlation = false;
};

struct Snapshot final {
  Mode mode = Mode::Application;
  void *current_hook = nullptr;
  std::uint32_t access_depth = 0;
  std::uint32_t control_depth = 0;
  std::uint32_t suppression_depth = 0;
};

class Diagnostics;
Snapshot current() noexcept;
EntryDecision check_entry(Callee) noexcept;
bool context_access_allowed() noexcept;
Diagnostics &runtime_diagnostics() noexcept;
bool event_allowed(Diagnostics &, std::string_view event_name = {}) noexcept;

class Scope final {
public:
  // Scope transitions are fixed-size TLS bookkeeping. Invalid transitions do
  // not change mode and are reported through reserved counters.
  explicit Scope(Mode) noexcept;
  ~Scope() noexcept;
  Scope(const Scope &) = delete;
  Scope &operator=(const Scope &) = delete;
  Scope(Scope &&) = delete;
  Scope &operator=(Scope &&) = delete;
  bool entered() const noexcept { return entered_; }
private:
  Snapshot prior_{};
  bool entered_ = false;
  bool masked_hook_ = false;
};

// The only supported Tracking -> Application transition. It fails unless all
// payload/control guards are released before the original target call.
class OriginalDispatch final {
public:
  OriginalDispatch() noexcept;
  ~OriginalDispatch() noexcept;
  OriginalDispatch(const OriginalDispatch &) = delete;
  OriginalDispatch &operator=(const OriginalDispatch &) = delete;
  OriginalDispatch(OriginalDispatch &&) = delete;
  OriginalDispatch &operator=(OriginalDispatch &&) = delete;
  bool entered() const noexcept { return entered_; }
private:
  Snapshot prior_{};
  bool entered_ = false;
};

class CurrentHook final {
public:
  explicit CurrentHook(void *hook) noexcept;
  ~CurrentHook() noexcept;
  CurrentHook(const CurrentHook &) = delete;
  CurrentHook &operator=(const CurrentHook &) = delete;
  CurrentHook(CurrentHook &&) = delete;
  CurrentHook &operator=(CurrentHook &&) = delete;
  bool installed() const noexcept { return installed_; }
private:
  void *prior_ = nullptr;
  bool installed_ = false;
};

class AccessLease final {
public:
  AccessLease() noexcept;
  ~AccessLease() noexcept;
  AccessLease(const AccessLease &) = delete;
  AccessLease &operator=(const AccessLease &) = delete;
  AccessLease(AccessLease &&) = delete;
  AccessLease &operator=(AccessLease &&) = delete;
  bool entered() const noexcept { return entered_; }
private:
  bool entered_ = false;
};

class ControlLease final {
public:
  ControlLease() noexcept;
  ~ControlLease() noexcept;
  ControlLease(const ControlLease &) = delete;
  ControlLease &operator=(const ControlLease &) = delete;
  ControlLease(ControlLease &&) = delete;
  ControlLease &operator=(ControlLease &&) = delete;
  bool entered() const noexcept { return entered_; }
private:
  bool entered_ = false;
};

using DiagnosticSink = void (*)(void *, const Diagnostic &) noexcept;

// Shared native diagnostic storage. record() never allocates or waits; the
// bounded sample ring may drop samples while fixed counters continue counting.
class Diagnostics final {
public:
  Diagnostics() noexcept;
  bool record(Severity, Reason, Phase, Action, std::uint64_t call_id = 0,
              std::uint64_t event_id = 0,
              std::string_view event_name = {}) noexcept;
  void no_active_hook(std::string_view event_name = {}) noexcept;
  std::uint64_t count(Reason, Phase) const noexcept;

  // Call from the engine's native reporting worker after guards are released.
  // Reports fixed-category/phase aggregates first, then at most one bounded
  // correlation sample per category/phase. The sink executes without locks.
  std::size_t drain_coalesced(DiagnosticSink, void *,
                              std::size_t max_samples = kDiagnosticCapacity) noexcept;
private:
  struct Cell {
    std::atomic<std::size_t> sequence{0};
    Diagnostic value{};
  };
  static constexpr std::size_t kCounterCount =
      static_cast<std::size_t>(Reason::count) *
      static_cast<std::size_t>(Phase::count);
  std::array<Cell, kDiagnosticCapacity> ring_{};
  std::array<std::atomic<std::uint64_t>, kCounterCount> counters_{};
  std::atomic<std::size_t> enqueue_{0};
  std::atomic<std::size_t> dequeue_{0};
};

} // namespace tracer::context_execution

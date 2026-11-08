#pragma once

// Native engine internals. Generated adapters use these identities; callback
// authors never register factories, store handles or provide access plans.
#include <any>
#include <array>
#include <atomic>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <vector>

#include "tracer/compat.hpp"

namespace tracer::context_engine {

inline constexpr std::size_t kMaxSlots = 64;
inline constexpr std::size_t kMaxBindings = 256;
using Identity = std::uint64_t;
// Native dispatch calls this after releasing its execution-mode guard leases.
void drain_lifecycle() noexcept;
// Initialize the engine's per-thread retirement state before a native TLS
// workspace anchor is registered, so its thread-exit cleanup runs first.
void prepare_thread_context() noexcept;

enum class ErrorCode : unsigned {
  ok,
  missing_state,
  incompatible_registration,
  wrong_domain,
  capacity,
  invalid_frame,
  active_access,
  unplanned_access,
  borrowed_end,
  repeated_end,
  end_order,
  construction_failed,
  no_active_hook,
  lifecycle_access,
  count
};

class Error final : public std::exception {
public:
  explicit Error(ErrorCode code) noexcept : code_(code) {}

  ErrorCode code() const noexcept { return code_; }

  const char *what() const noexcept override;

private:
  ErrorCode code_;
};

class Diagnostics {
public:
  void count(ErrorCode code) noexcept;
  std::uint64_t count_for(ErrorCode code) const noexcept;

private:
  std::array<std::atomic<std::uint64_t>,
             static_cast<unsigned>(ErrorCode::count)>
      counts_{};
};

struct ScopeInit {
  const void *root_call = nullptr; // Valid only during the owner factory call.
  Identity payload_identity = 0;
  const void *payload_source = nullptr;
};

enum class Policy { confined, shared };

struct StateRegistration {
  Identity identity = 0;
  Policy policy = Policy::confined;
  // All bridge calls contain their own user exceptions. Payloads and erased
  // manager functions stay owner-side and never cross into callback JITs.
  ErrorCode (*construct)(const ScopeInit &, std::any &) noexcept = nullptr;
  void (*destroy)(std::any &) noexcept = nullptr;
  void *(*address)(std::any &) noexcept = nullptr;
};

struct ScopeRegistration {
  // Identity denotes the scope family; the registration object denotes the
  // exact frozen generation. A same-family mismatch is an error, not shadowing.
  Identity identity = 0;
  std::shared_ptr<const void> owner_code;
  std::vector<StateRegistration> states;
  // Native boundary retains the exact owner/support generation through the
  // final return. It is optional for statically compiled engine fixtures.
  const void *invocation_context = nullptr;
  bool (*invoke)(const void *, void (*)(void *), void *) noexcept = nullptr;
  // Final destruction may run after the ordinary support exception lanes are
  // occupied by an application call. This boundary must admit only the
  // validated nonthrowing cleanup entry and does not reserve an exception lane.
  bool (*invoke_cleanup)(const void *, void (*)(void *), void *) noexcept = nullptr;
  // Immutable engine-owned schema/serializer metadata for any state in this
  // registration. The context is pinned by owner_code; lookup is lock-free.
  const void *state_metadata_context = nullptr;
  const void *(*state_metadata)(const void *, Identity) noexcept = nullptr;
};

struct SlotRequest {
  std::shared_ptr<const ScopeRegistration> registration;
  Identity state = 0;
};

class ScopeRecord;
class InvocationFrame;
class HookAccess;
class HookExit;
class TaskCapture;
class TaskAttachment;

class Context final {
public:
  explicit Context(Diagnostics &diagnostics) noexcept;
  ~Context();
  Context(const Context &) = delete;
  Context &operator=(const Context &) = delete;
  Context(Context &&) = delete;
  Context &operator=(Context &&) = delete;

  std::size_t binding_count() const noexcept { return size_; }
  bool permits_cross_domain_capture() const noexcept;

  void *get(const SlotRequest &request);
  // Type-erased owner-generated visitor; exceptions are contained by the
  // generated owner bridge. This operation never constructs missing state.
  void with(const SlotRequest &request, void (*visitor)(void *, void *),
            void *argument);
  void init_scope(std::shared_ptr<const ScopeRegistration>,
                  const ScopeInit & = {});
  void end_scope(Identity scope);
  // Native-guard-safe status surface. Does not allocate or throw; the caller
  // may materialize its private error only after releasing payload guards.
  ErrorCode try_end_scope(Identity scope) noexcept;
  void *try_get(const SlotRequest &request, ErrorCode &status) noexcept;

private:
  friend class InvocationFrame;
  friend class HookAccess;
  friend class HookExit;
  friend class TaskCapture;
  friend class TaskAttachment;

  struct Binding {
    Identity identity = 0;
    Identity scope = 0;
    InvocationFrame *frame = nullptr;
    std::shared_ptr<ScopeRecord> record;
    bool owned = false;
    bool pending = false;
    bool retired = false;
  };

  void check_domain() const;
  void check_lifecycle() const;
  void push(InvocationFrame &, std::shared_ptr<const ScopeRegistration>,
            const ScopeInit &, bool fresh);
  bool restore(InvocationFrame &) noexcept;
  void commit_ends() noexcept;
  std::shared_ptr<ScopeRecord> resolve(const SlotRequest &) const;

  Diagnostics &diagnostics_;
  Identity domain_;
  std::array<Binding, kMaxBindings> bindings_{};
  std::size_t size_ = 0;
  InvocationFrame *frame_ = nullptr;
  HookAccess *access_ = nullptr;
  HookExit *hook_ = nullptr;
};

class InvocationFrame final {
public:
  explicit InvocationFrame(Context &context);
  ~InvocationFrame() noexcept;
  InvocationFrame(const InvocationFrame &) = delete;
  InvocationFrame &operator=(const InvocationFrame &) = delete;
  InvocationFrame(InvocationFrame &&) = delete;
  InvocationFrame &operator=(InvocationFrame &&) = delete;
  void create_or_attach(std::shared_ptr<const ScopeRegistration>,
                        const ScopeInit & = {});
  void pop_all_and_restore() noexcept;

private:
  friend class Context;
  Context &context_;
  InvocationFrame *parent_;
  std::size_t saved_;
  bool restored_ = false;
};

class HookAccess final {
public:
  enum class Kind { direct, independent };
  // The complete set is resolved, checked and pinned before the first wait.
  // all_visible is for generated direct hooks with reachable event emission.
  HookAccess(Context&, ::tracer::compat::span<const SlotRequest>, bool all_visible = false,
             Kind = Kind::direct);
  ~HookAccess() noexcept;
  HookAccess(const HookAccess &) = delete;
  HookAccess &operator=(const HookAccess &) = delete;
  HookAccess(HookAccess &&) = delete;
  HookAccess &operator=(HookAccess &&) = delete;
  void *ref(const SlotRequest &) const;
  void *try_ref(const SlotRequest &, ErrorCode &status) const noexcept;
  using Visit = void (*)(void *, Identity, Identity, const ScopeRegistration &,
                         void *) noexcept;
  bool visit_all(Visit, void *) const noexcept;

private:
  friend class Context;

  struct Entry {
    std::shared_ptr<ScopeRecord> record;
    std::size_t slot = 0;
    std::unique_lock<std::mutex> lock;
  };

  void add(std::shared_ptr<ScopeRecord>, Identity);
  Context &context_;
  std::array<Entry, kMaxSlots> entries_{};
  std::size_t size_ = 0;
  Kind kind_;
};

class HookExit final {
public:
  // Native bounded publication after access release and pending ends.
  using Publish = void (*)(void *, bool failed) noexcept;
  explicit HookExit(Context &, Publish = nullptr, void * = nullptr) noexcept;
  ~HookExit() noexcept;
  HookExit(const HookExit &) = delete;
  HookExit &operator=(const HookExit &) = delete;
  HookExit(HookExit &&) = delete;
  HookExit &operator=(HookExit &&) = delete;

  void capture_failure() noexcept { failed_ = true; }

private:
  Context &context_;
  Publish publish_;
  void *argument_;
  bool failed_ = false;
  bool installed_ = false;
};

// Engine scheduler primitives. No scheduler is declared supported merely by
// using this runtime: a generated adapter must prove acceptance/cancellation.
struct PropagationLimits {
  std::size_t per_scope = 64;
  std::size_t tasks = 4096;
  std::size_t bytes = 4 * 1024 * 1024;
  std::size_t ancestry = 64;
};

class PropagationLedger final {
public:
  explicit PropagationLedger(PropagationLimits limits = {}) : limits_(limits) {}

  std::size_t outstanding() const noexcept;

private:
  friend class TaskCapture;
  const PropagationLimits limits_;
  std::size_t tasks_ = 0, bytes_ = 0;
};

class TaskCapture final {
public:
  explicit TaskCapture(std::shared_ptr<PropagationLedger> ledger)
      : ledger_(std::move(ledger)) {}

  ~TaskCapture() noexcept;
  TaskCapture(const TaskCapture &) = delete;
  TaskCapture &operator=(const TaskCapture &) = delete;
  TaskCapture(TaskCapture &&) = delete;
  TaskCapture &operator=(TaskCapture &&) = delete;
  static std::shared_ptr<TaskCapture>
  capture(Context &, std::shared_ptr<PropagationLedger>) noexcept;
  // Only a confirmed pre-start cancellation consumes a queued capability.
  // A cancellation request during execution never releases live ownership.
  bool cancel_before_start() noexcept;

private:
  friend class TaskAttachment;
  enum class Phase { prepared, queued, running, terminal };
  void release() noexcept;
  void finish() noexcept;
  const std::shared_ptr<PropagationLedger> ledger_;
  std::atomic<Phase> phase_{Phase::prepared};
  std::array<std::shared_ptr<ScopeRecord>, kMaxSlots> records_{};
  std::size_t size_ = 0;
};

class TaskAttachment final {
public:
  TaskAttachment(Context &, std::shared_ptr<TaskCapture>);
  ~TaskAttachment() noexcept;
  TaskAttachment(const TaskAttachment &) = delete;
  TaskAttachment &operator=(const TaskAttachment &) = delete;
  TaskAttachment(TaskAttachment &&) = delete;
  TaskAttachment &operator=(TaskAttachment &&) = delete;

private:
  Context &context_;
  std::shared_ptr<TaskCapture> task_;
  std::array<Context::Binding, kMaxBindings> saved_{};
  std::size_t saved_size_ = 0;
  InvocationFrame *saved_frame_ = nullptr;
};

} // namespace tracer::context_engine

#pragma once

#include <functional>
#include <span>
#include <vector>

#include "tracer/live_migration.hpp"

namespace tracer::live {
struct JitRequest;
class JitImage;
} // namespace tracer::live

namespace tracer::live::managed {

struct HookOptions {
  std::string file;
  std::string function;
  std::string type;
};

struct UpdateOptions {
  std::vector<std::string> paths;
  HookOptions hook;
};

UpdateOptions parse_update(std::span<const std::string> arguments);

// The code callback is the coordinator's existing checked publication path.
// It runs with cooperative access gated. It must return after any OS thread
// freeze is released, before managed construction or hooks can continue.
// For an unsuccessful operation, code_unchanged means its entry state is
// exactly what it was on entry. Any ambiguous write requires recovery.
enum class CodeState { installed, unchanged, stale, restoration_required };

struct CodeResult {
  CodeState state;
  std::string detail;
};

using CodeAction = std::function<CodeResult()>;

struct Publication {
  CodeAction install;
  CodeAction restore;
};

enum class Status { applied, blocked, stale, rebuild_required, restoration_required };

struct Result {
  Status status;
  std::string detail;
  std::size_t instances = 0;
};

class Revision {
public:
  ~Revision();
  Revision(const Revision&) = delete;
  Revision& operator=(const Revision&) = delete;

  // Give the common transaction the already-materialized native entries
  // and their image to pin for as long as a native address may escape.
  const std::shared_ptr<JitImage>& image() const noexcept { return image_; }

  using Deadline = std::chrono::steady_clock::time_point;
  // Hooks must limit writes to the staged state. Application-owned
  // compensation is required for any effects outside the managed group.
  Result apply(Deadline deadline, const Publication& code = {});
  // A post-publication revert requires a declared reverse transform and
  // converts the current live states, including instances created since apply.
  Result revert(Deadline deadline, const Publication& code = {});
  // After uncertain publication the group stays gated until the coordinator
  // proves recovery to the code that preceded the failed operation.
  Result recover(const CodeAction& restore_code);

private:
  friend std::unique_ptr<Revision> stage(Registry&, const JitRequest&, const HookOptions&);

  struct PreparedType {
    std::string name;

    struct ExpectedLayout {
      std::string id;
      std::string cpp_type;
      std::size_t size;
      std::size_t alignment;
    } previous;

    std::shared_ptr<const Registry::Binding> before;
    std::shared_ptr<const Registry::Binding> after;
    Transform forward;
    Transform reverse;
  };

  using Hook = bool (*)(void*, Diagnostic&) noexcept;

  struct Pending {
    Registry::Type* live;
    Registry::Type staged;
  };

  Revision(Registry& registry, std::uint64_t generation);
  void add_change(const Change& change, const std::shared_ptr<void>& image);
  Result transition(bool reverse, Deadline deadline, const Publication& code);
  void prepare(bool reverse);
  void swap_states() noexcept;
  void release_gate() noexcept;
  Result seal(std::string detail);

  Registry& registry_;
  const std::uint64_t generation_;
  std::shared_ptr<JitImage> image_;
  std::shared_ptr<JitImage> hook_image_;
  std::vector<PreparedType> types_;
  std::string hook_type_;
  Hook hook_ = nullptr;
  std::mutex operation_;
  std::vector<Pending> pending_;
  bool applied_ = false;
  bool has_native_code_ = false;
  bool used_ = false;
};

// Call before compilation or process freezing with the application registry
// resolved from the same verified target as the native revision. Request.files
// contains the update and optional hook snapshots. The managed recipe is an
// explicit application opt-in. Lookup of a missing managed type fails closed.
// A header-only recipe is exposed by a compiled dependent TU, or by a TU
// supplied by the coordinator that includes the snapshotted header.
std::unique_ptr<Revision> stage(Registry& registry, const JitRequest& request,
                                const HookOptions& hook = {});

// Command diagnostics for missing application-owned storage.
class RebuildRequired : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

} // namespace tracer::live::managed

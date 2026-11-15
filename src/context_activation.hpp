#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "context_binding.hpp"
#include "context_prepared_adapter.hpp"
#include "context_registry.hpp"
#include "tracer/protocol.hpp"

namespace tracer {

struct ContextActivationRequest final {
  std::string source;
  std::vector<std::string> compiler_arguments;
  std::array<std::uint8_t, 16> source_lineage_id{};
  std::string interceptor_class;
  PreparedAdapterRegion prepared_adapter;
  std::string file_name = "context-activation-input.cc";
  std::size_t maximum_object_bytes = 4096;
  std::size_t maximum_object_alignment = 64;
  decltype(&detail::tracer_enqueue_log) callback_log = nullptr;
};

struct ContextActivationError final {
  std::string message;

  explicit operator bool() const noexcept { return !message.empty(); }
};

// One Clang error from a callback source that failed to parse.
struct ContextSourceError final {
  std::string file;
  unsigned line = 0;
  unsigned column = 0;
  std::string message;
};

struct ContextActivationResult;
ContextActivationResult
activate_context_source(const ContextActivationRequest &, ContextRegistry &);

struct ContextPreflightResult final {
  std::string error;
  // Set when the callback source itself failed to parse.
  std::vector<ContextSourceError> source_errors;

  explicit operator bool() const noexcept { return error.empty(); }
};

// Performs the same source lowering and wrapper checks as activation without
// publishing schemas, registering owners, constructing user state, or running
// any generated entrypoint. Full mode also materializes and resolves generated
// symbols; syntax-only mode stops after parsing the lowered wrapper.
ContextPreflightResult
preflight_context_source(const ContextActivationRequest &, bool syntax_only);

// A fully admitted generation. All generated code, the exact owner imported by
// the callback, immutable schemas and registry leases remain pinned together.
class ContextActivationGeneration final {
public:
  ContextActivationGeneration(const ContextActivationGeneration &) = delete;
  ContextActivationGeneration &
  operator=(const ContextActivationGeneration &) = delete;

  const std::shared_ptr<const PrivateContextCode> &owner_code() const noexcept {
    return owner_code_;
  }

  const std::shared_ptr<const PrivateContextCode> &
  callback_code() const noexcept {
    return callback_code_;
  }

  const std::vector<std::shared_ptr<const RegisteredContextScope>> &
  scopes() const noexcept {
    return scopes_;
  }

  const std::vector<std::shared_ptr<const context_engine::SchemaRecord>> &
  schemas() const noexcept {
    return schemas_;
  }

  const std::string &interceptor_class() const noexcept { return class_; }

  const std::vector<std::string> &lineage_diagnostics() const noexcept {
    return lineage_diagnostics_;
  }

  std::size_t object_bytes() const noexcept { return object_bytes_; }

  std::size_t object_alignment() const noexcept { return object_alignment_; }

  std::uintptr_t construct_entry() const noexcept { return construct_; }

  std::uintptr_t pre_entry() const noexcept { return pre_; }

  std::uintptr_t post_entry() const noexcept { return post_; }

  std::uintptr_t destroy_entry() const noexcept { return destroy_; }

  bool requires_stack() const noexcept { return requires_stack_; }

  // Whether the interceptor defines the hook dispatched for this phase.
  bool has_hook(TracePhase phase) const noexcept {
    return phase == TracePhase::Pre ? has_pre_ : has_post_;
  }

private:
  friend ContextActivationResult prepare_context_source(
      const ContextActivationRequest &, ContextRegistry *, bool, bool);
  friend ContextActivationResult
  activate_context_source(const ContextActivationRequest &, ContextRegistry &);
  ContextActivationGeneration(
      std::shared_ptr<const PrivateContextCode>,
      std::shared_ptr<const PrivateContextCode>,
      std::vector<std::shared_ptr<const RegisteredContextScope>>,
      std::vector<std::shared_ptr<const context_engine::SchemaRecord>>,
      std::string, std::size_t, std::size_t, std::uintptr_t, std::uintptr_t,
      std::uintptr_t, std::uintptr_t, std::vector<std::string>, bool, bool,
      bool);
  const std::shared_ptr<const PrivateContextCode> owner_code_;
  const std::shared_ptr<const PrivateContextCode> callback_code_;
  const std::vector<std::shared_ptr<const RegisteredContextScope>> scopes_;
  const std::vector<std::shared_ptr<const context_engine::SchemaRecord>>
      schemas_;
  const std::string class_;
  const std::size_t object_bytes_, object_alignment_;
  const std::uintptr_t construct_, pre_, post_, destroy_;
  const std::vector<std::string> lineage_diagnostics_;
  const bool requires_stack_;
  const bool has_pre_, has_post_;
};

struct ContextActivationResult final {
  std::shared_ptr<const ContextActivationGeneration> generation;
  std::string error;
  // Set when the callback source itself failed to parse.
  std::vector<ContextSourceError> source_errors;
  bool preflight_success = false;

  explicit operator bool() const noexcept {
    return bool(generation) || preflight_success;
  }
};

// Parses and validates one source generation, compiles state owners and hook
// callbacks into separate sealed modules, and publishes complete scope groups
// only after both modules have passed admission.
ContextActivationResult
activate_context_source(const ContextActivationRequest &, ContextRegistry &);

} // namespace tracer

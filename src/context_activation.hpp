#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "context_binding.hpp"
#include "context_registry.hpp"
#include "tracer/protocol.hpp"

namespace tracer {

struct ContextActivationRequest final {
  std::string source;
  std::vector<std::string> compiler_arguments;
  std::array<std::uint8_t, 16> source_lineage_id{};
  std::string interceptor_class;
  std::string file_name = "context-activation-input.cc";
  std::size_t maximum_object_bytes = 4096;
  std::size_t maximum_object_alignment = 64;
  decltype(&detail::tracer_enqueue_log) callback_log = nullptr;
};

struct ContextActivationError final {
  std::string message;

  explicit operator bool() const noexcept { return !message.empty(); }
};

struct ContextActivationResult;
ContextActivationResult
activate_context_source(const ContextActivationRequest &, ContextRegistry &);

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

private:
  friend ContextActivationResult
  activate_context_source(const ContextActivationRequest &, ContextRegistry &);
  ContextActivationGeneration(
      std::shared_ptr<const PrivateContextCode>,
      std::shared_ptr<const PrivateContextCode>,
      std::vector<std::shared_ptr<const RegisteredContextScope>>,
      std::vector<std::shared_ptr<const context_engine::SchemaRecord>>,
      std::string, std::size_t, std::size_t, std::uintptr_t, std::uintptr_t,
      std::uintptr_t, std::uintptr_t, std::vector<std::string>, bool);
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
};

struct ContextActivationResult final {
  std::shared_ptr<const ContextActivationGeneration> generation;
  std::string error;

  explicit operator bool() const noexcept { return bool(generation); }
};

// Parses and validates one source generation, compiles state owners and hook
// callbacks into separate sealed modules, and publishes complete scope groups
// only after both modules have passed admission.
ContextActivationResult
activate_context_source(const ContextActivationRequest &, ContextRegistry &);

} // namespace tracer

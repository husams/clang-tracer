#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "context_binding.hpp"
#include "context_capture.hpp"
#include "context_engine.hpp"
#include "context_owner.hpp"
#include "context_schema.hpp"
#include "tracer/compat.hpp"

namespace tracer {

context_engine::Identity context_scope_identity(const std::array<std::uint8_t, 16>& lineage,
                                                std::string_view canonical_scope) noexcept;
context_engine::Identity
context_state_identity(const std::array<std::uint8_t, 16>& lineage,
                       std::string_view canonical_scope,
                       std::string_view canonical_state_or_payload) noexcept;

struct RegisteredContextState final {
  using OwnerSerialize = bool (*)(context_engine::BoundedProtoWriter&, const void*,
                                  context_engine::EncodeBudget&) noexcept;
  const context_engine::Identity identity;
  const context_engine::Identity scope_identity;
  const ContextDependency dependency;
  const std::uint64_t initializer_identity;
  const std::string lifecycle_fingerprint;
  const std::shared_ptr<const context_engine::SchemaRecord> schema;
  const OwnerSerialize serialize;
  const void* owner_invocation_context;
  const std::vector<ContextTypeProvenance> payload_provenance;

  RegisteredContextState(context_engine::Identity, context_engine::Identity, ContextDependency,
                         std::uint64_t, std::string,
                         std::shared_ptr<const context_engine::SchemaRecord>,
                         OwnerSerialize, const void*, std::vector<ContextTypeProvenance> = {});
  context_engine::CaptureSchemaView schema_view(std::uint64_t epoch_schema_id) const noexcept;
  context_engine::CaptureStateView capture_view(std::uint64_t runtime_scope_id,
                                                std::uint64_t epoch_schema_id,
                                                const void* payload) const noexcept;

private:
  static bool encode_payload(const void*, context_engine::BoundedProtoWriter&, const void*,
                             context_engine::EncodeBudget&) noexcept;
};

const RegisteredContextState*
registered_context_state(const context_engine::ScopeRegistration& registration,
                         context_engine::Identity identity) noexcept;

struct RegisteredContextScope final {
  const std::array<std::uint8_t, 16> source_lineage_id;
  const std::string canonical_scope_type;
  const context_engine::Identity identity;
  const std::shared_ptr<const PrivateContextCode> owner_generation;
  const std::shared_ptr<const context_engine::ScopeRegistration> registration;
  const std::vector<RegisteredContextState> states;
  const ContextCompilationContract type_contract;

  RegisteredContextScope(std::array<std::uint8_t, 16>, std::string,
                         std::shared_ptr<const PrivateContextCode>,
                         std::shared_ptr<const context_engine::ScopeRegistration>,
                         std::vector<RegisteredContextState>, ContextCompilationContract);
};

struct ContextRegistryResult final {
  std::vector<std::shared_ptr<const RegisteredContextScope>> scopes;
  std::string error;

  explicit operator bool() const noexcept { return error.empty(); }
};

struct RegisteredContextStateRef final {
  std::shared_ptr<const RegisteredContextScope> owner;
  const RegisteredContextState* state = nullptr;

  explicit operator bool() const noexcept { return owner && state; }
};

// Freezes owner exports, schemas and complete per-scope state groups before
// activation. The registry retains exact owner code generations in each
// ScopeRegistration; queued schemas remain independent immutable records.
class ContextRegistry final {
public:
  ContextRegistry() = default;
  ContextRegistry(const ContextRegistry&) = delete;
  ContextRegistry& operator=(const ContextRegistry&) = delete;

  ContextRegistryResult register_owner(
      const std::array<std::uint8_t, 16>& source_lineage_id,
      std::shared_ptr<const PrivateContextCode> owner_code, const GeneratedContextOwner& generated,
      ::tracer::compat::span<const std::shared_ptr<const context_engine::SchemaRecord>> schemas);

  std::shared_ptr<const RegisteredContextScope>
  find_scope(const std::array<std::uint8_t, 16>& source_lineage_id,
             std::string_view canonical_scope_type) const;
  std::shared_ptr<const RegisteredContextScope> find_scope(context_engine::Identity identity) const;
  // Returns every still-live immutable generation of one stable scope family.
  // The single-result find_scope overloads return null when this is ambiguous.
  std::vector<std::shared_ptr<const RegisteredContextScope>>
  find_scope_generations(const std::array<std::uint8_t, 16>& source_lineage_id,
                         std::string_view canonical_scope_type) const;
  RegisteredContextStateRef find_state(context_engine::Identity scope_identity,
                                       context_engine::Identity state_identity) const;
  RegisteredContextStateRef find_state(std::shared_ptr<const RegisteredContextScope> generation,
                                       context_engine::Identity state_identity) const;
  std::size_t scope_count() const;

private:
  mutable std::mutex mutex_;
  std::vector<std::weak_ptr<const RegisteredContextScope>> scopes_;
};

} // namespace tracer

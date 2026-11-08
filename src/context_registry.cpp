#include "context_registry.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>

#include <llvm/Support/Error.h>

#include "context_stdlib.hpp"
#include "tracer/compat.hpp"

namespace tracer {
namespace {
using namespace context_engine;
using Construct = ErrorCode (*)(const ScopeInit &, std::any &) noexcept;
using Destroy = void (*)(std::any &) noexcept;
using Address = void *(*)(std::any &) noexcept;
using Serialize = RegisteredContextState::OwnerSerialize;
constexpr std::size_t kNativeSchemaMetadataBytes = 64ull * 1024 * 1024;

struct NativeSchemaAdmission {
  std::mutex mutex;
  std::unordered_map<const SchemaRecord *, std::weak_ptr<const SchemaRecord>>
      records;
};

NativeSchemaAdmission &native_schema_admission() {
  static NativeSchemaAdmission admission;
  return admission;
}

bool admit_native_schemas(::tracer::compat::span<const std::shared_ptr<const SchemaRecord>> schemas,
                          std::string& error) {
  auto &admission = native_schema_admission();
  std::lock_guard lock(admission.mutex);
  std::size_t used = 0;
  for (auto it = admission.records.begin(); it != admission.records.end();) {
    if (const auto record = it->second.lock()) {
      if (record->accounted_bytes >
          kNativeSchemaMetadataBytes -
              std::min(used, kNativeSchemaMetadataBytes)) {
        used = kNativeSchemaMetadataBytes;
      } else {
        used += record->accounted_bytes;
      }
      ++it;
    } else {
      it = admission.records.erase(it);
    }
  }
  std::set<const SchemaRecord *> pending;
  std::size_t additional = 0;
  for (const auto &record : schemas) {
    if (!record || ::tracer::compat::contains(admission.records, record.get()) ||
        !pending.insert(record.get()).second) {
      continue;
    }
    if (record->accounted_bytes >
        kNativeSchemaMetadataBytes -
            std::min(used + additional, kNativeSchemaMetadataBytes)) {
      error = "incompatible_registration: native schema metadata capacity "
              "exceeded";
      return false;
    }
    additional += record->accounted_bytes;
  }
  for (const auto *raw : pending) {
    const auto found =
        std::find_if(schemas.begin(), schemas.end(),
                     [&](const auto &record) { return record.get() == raw; });
    if (found != schemas.end())
      admission.records.emplace(raw, *found);
  }
  return true;
}

struct OwnerInvocationContext {
  std::shared_ptr<const PrivateContextCode> code;
  std::shared_ptr<const ContextStdlibImage> standard_library;
  std::vector<std::unique_ptr<RegisteredContextState>> state_metadata;
};

const void *lookup_state_metadata(const void *opaque,
                                  Identity identity) noexcept {
  const auto *owner = static_cast<const OwnerInvocationContext *>(opaque);
  if (!owner || !identity) {
    return nullptr;
  }
  const auto found = std::lower_bound(
      owner->state_metadata.begin(), owner->state_metadata.end(), identity,
      [](const auto &state, Identity requested) {
        return state->identity < requested;
      });
  return found != owner->state_metadata.end() && (*found)->identity == identity
             ? found->get()
             : nullptr;
}

bool invoke_owner(const void *opaque, void (*function)(void *),
                  void *argument) noexcept {
  if (!function) {
    return false;
  }
  const auto *context = static_cast<const OwnerInvocationContext *>(opaque);
  if (!context || !context->standard_library) {
    function(argument);
    return true;
  }
  ContextStdlibImage::Frame frame(context->standard_library,
                                  context->code.get(),
                                  &PrivateContextCode::unwind_lookup);
  return frame.entered() &&
         context->standard_library->invoke(function, argument);
}

bool invoke_owner_cleanup(const void *opaque, void (*function)(void *),
                          void *argument) noexcept {
  if (!function) {
    return false;
  }
  const auto *context = static_cast<const OwnerInvocationContext *>(opaque);
  if (!context || !context->standard_library) {
    function(argument);
    return true;
  }
  ContextStdlibImage::Frame frame(context->standard_library,
                                  context->code.get(),
                                  &PrivateContextCode::unwind_lookup,
                                  ContextStdlibImage::Frame::Purpose::cleanup);
  return frame.entered() &&
         context->standard_library->invoke(function, argument);
}

std::string scope_key(const std::array<std::uint8_t, 16> &lineage,
                      std::string_view scope) {
  std::string key(reinterpret_cast<const char *>(lineage.data()),
                  lineage.size());
  key.push_back('\0');
  key.append(scope);
  return key;
}

std::string state_key(const std::array<std::uint8_t, 16> &lineage,
                      std::string_view scope, std::string_view state) {
  auto key = scope_key(lineage, scope);
  key.push_back('\0');
  key.append(state);
  return key;
}

bool empty_lineage(const std::array<std::uint8_t, 16> &lineage) noexcept {
  return std::all_of(lineage.begin(), lineage.end(),
                     [](std::uint8_t byte) { return byte == 0; });
}

bool same_contract(const ContextCompilationContract &left,
                   const ContextCompilationContract &right) {
  return left.target == right.target && left.data_layout == right.data_layout &&
         left.compiler == right.compiler && left.language == right.language &&
         left.standard_library == right.standard_library &&
         left.types == right.types;
}

bool same_dependency(const ContextDependency &left,
                     const ContextDependency &right) noexcept {
  return left.state_type == right.state_type &&
         left.scope_type == right.scope_type && left.shared == right.shared &&
         left.generic == right.generic &&
         left.payload_type == right.payload_type;
}

bool same_schema(const context_engine::SchemaRecord &left,
                 const context_engine::SchemaRecord &right) noexcept {
  return left.family == right.family && left.root_type == right.root_type &&
         left.revision_id == right.revision_id &&
         left.descriptor_set == right.descriptor_set &&
         left.container_roles == right.container_roles;
}

struct CandidateState {
  ContextDependency dependency;
  std::uint64_t initializer_identity = 0;
  std::string lifecycle_fingerprint;
  std::shared_ptr<const SchemaRecord> schema;
  Identity identity = 0;
  Serialize serialize = nullptr;
  Construct construct = nullptr;
  Destroy destroy = nullptr;
  Address address = nullptr;
  std::vector<ContextTypeProvenance> payload_provenance;
};

struct CandidateScope {
  std::array<std::uint8_t, 16> lineage{};
  std::string scope_type;
  Identity identity = 0;
  std::vector<CandidateState> states;
};

bool same_scope(const RegisteredContextScope &existing,
                const CandidateScope &candidate,
                const std::array<std::uint8_t, 16> &lineage,
                const ContextCompilationContract &contract) {
  if (existing.source_lineage_id != lineage ||
      existing.canonical_scope_type != candidate.scope_type ||
      existing.identity != candidate.identity ||
      !same_contract(existing.type_contract, contract) ||
      existing.states.size() != candidate.states.size()) {
    return false;
  }
  for (std::size_t i = 0; i < candidate.states.size(); ++i) {
    const auto &a = existing.states[i];
    const auto &b = candidate.states[i];
    if (a.identity != b.identity ||
        !same_dependency(a.dependency, b.dependency) || !a.schema ||
        !b.schema || !same_schema(*a.schema, *b.schema) ||
        a.payload_provenance != b.payload_provenance ||
        a.lifecycle_fingerprint != b.lifecycle_fingerprint) {
      return false;
    }
  }
  return true;
}

std::string symbol_error(llvm::Expected<ContextSymbol> &symbol) {
  return llvm::toString(symbol.takeError());
}

template <class Function>
bool resolve_function(const PrivateContextCode &owner,
                      const std::string &source_name, Function &function,
                      std::string &error) {
  std::string name = source_name;
  const auto &contract = owner.contract();
  const auto bridge =
      std::find_if(contract.bridges.begin(), contract.bridges.end(),
                   [&](const auto &candidate) {
                     return candidate.source_name == source_name;
                   });
  if (bridge != contract.bridges.end()) {
    name = bridge->symbol;
  }
  if (name.empty()) {
    error = "generated owner export name is empty";
    return false;
  }
  auto symbol = owner.symbol(name);
  if (!symbol) {
    error = symbol_error(symbol);
    return false;
  }
  if (!symbol->address()) {
    error = "generated owner export has no materialized address: " + name;
    return false;
  }
  function = reinterpret_cast<Function>(symbol->address());
  return true;
}
} // namespace

context_engine::Identity
context_scope_identity(const std::array<std::uint8_t, 16> &lineage,
                       std::string_view canonical_scope) noexcept {
  std::uint64_t hash = 14695981039346656037ULL;
  constexpr std::string_view tag = "clangtracer.context.scope.v1";
  auto mix = [&](unsigned char byte) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  };
  for (const unsigned char byte : tag) {
    mix(byte);
  }
  mix(0);
  for (const auto byte : lineage) {
    mix(byte);
  }
  mix(0);
  for (const unsigned char byte : canonical_scope) {
    mix(byte);
  }
  return hash ? hash : 1;
}

context_engine::Identity
context_state_identity(const std::array<std::uint8_t, 16> &lineage,
                       std::string_view canonical_scope,
                       std::string_view canonical_state_or_payload) noexcept {
  std::uint64_t hash = 14695981039346656037ULL;
  constexpr std::string_view tag = "clangtracer.context.state.v1";
  auto mix = [&](unsigned char byte) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  };
  for (const unsigned char byte : tag) {
    mix(byte);
  }
  mix(0);
  for (const auto byte : lineage) {
    mix(byte);
  }
  mix(0);
  for (const unsigned char byte : canonical_scope) {
    mix(byte);
  }
  mix(0);
  for (const unsigned char byte : canonical_state_or_payload) {
    mix(byte);
  }
  return hash ? hash : 1;
}

RegisteredContextState::RegisteredContextState(
    context_engine::Identity state_identity,
    context_engine::Identity owner_scope_identity,
    ContextDependency state_dependency,
    std::uint64_t owner_initializer_identity,
    std::string owner_lifecycle_fingerprint,
    std::shared_ptr<const context_engine::SchemaRecord> state_schema,
    OwnerSerialize state_serializer, const void *invocation_context,
    std::vector<ContextTypeProvenance> provenance)
    : identity(state_identity), scope_identity(owner_scope_identity),
      dependency(std::move(state_dependency)),
      initializer_identity(owner_initializer_identity),
      lifecycle_fingerprint(std::move(owner_lifecycle_fingerprint)),
      schema(std::move(state_schema)), serialize(state_serializer),
      owner_invocation_context(invocation_context),
      payload_provenance(std::move(provenance)) {}

const RegisteredContextState *
registered_context_state(const context_engine::ScopeRegistration &registration,
                         context_engine::Identity identity) noexcept {
  if (!registration.state_metadata || !registration.state_metadata_context) {
    return nullptr;
  }
  return static_cast<const RegisteredContextState *>(
      registration.state_metadata(registration.state_metadata_context,
                                  identity));
}

context_engine::CaptureSchemaView RegisteredContextState::schema_view(
    std::uint64_t epoch_schema_id) const noexcept {
  const auto &family = schema->family;
  const auto lineage = std::string_view(
      reinterpret_cast<const char *>(family.source_lineage_id.data()),
      family.source_lineage_id.size());
  return {schema->revision_id,    schema->root_type,
          schema->descriptor_set, lineage,
          family.cpp_state_type,  family.cpp_scope_type,
          schema->revision_id,    schema->container_roles,
          epoch_schema_id,        schema};
}

context_engine::CaptureStateView
RegisteredContextState::capture_view(std::uint64_t runtime_scope_id,
                                     std::uint64_t epoch_schema_id,
                                     const void *payload) const noexcept {
  return {runtime_scope_id,
          schema->revision_id,
          epoch_schema_id,
          payload,
          this,
          &RegisteredContextState::encode_payload};
}

bool RegisteredContextState::encode_payload(
    const void *opaque, context_engine::BoundedProtoWriter &writer,
    const void *payload, context_engine::EncodeBudget &budget) noexcept {
  const auto *state = static_cast<const RegisteredContextState *>(opaque);
  if (!state || !state->serialize || !state->owner_invocation_context) {
    return false;
  }

  struct Args {
    OwnerSerialize serialize;
    context_engine::BoundedProtoWriter *writer;
    const void *payload;
    context_engine::EncodeBudget *budget;
    bool result = false;
  } args{state->serialize, &writer, payload, &budget};

  const auto call_serializer = [](void *context) noexcept {
    auto &call = *static_cast<Args *>(context);
    call.result = call.serialize(*call.writer, call.payload, *call.budget);
  };
  return invoke_owner(state->owner_invocation_context, call_serializer,
                      &args) &&
         args.result;
}

RegisteredContextScope::RegisteredContextScope(
    std::array<std::uint8_t, 16> lineage, std::string scope,
    std::shared_ptr<const PrivateContextCode> owner_generation,
    std::shared_ptr<const context_engine::ScopeRegistration> scope_registration,
    std::vector<RegisteredContextState> registered_states,
    ContextCompilationContract contract)
    : source_lineage_id(lineage), canonical_scope_type(std::move(scope)),
      identity(scope_registration ? scope_registration->identity : 0),
      owner_generation(std::move(owner_generation)),
      registration(std::move(scope_registration)),
      states(std::move(registered_states)), type_contract(std::move(contract)) {
}

ContextRegistryResult ContextRegistry::register_owner(
    const std::array<std::uint8_t, 16>& source_lineage_id,
    std::shared_ptr<const PrivateContextCode> owner_code, const GeneratedContextOwner& generated,
    ::tracer::compat::span<const std::shared_ptr<const context_engine::SchemaRecord>> schemas) {
  ContextRegistryResult result;
  if (empty_lineage(source_lineage_id) || !owner_code ||
      !generated.error.empty() || generated.states.empty()) {
    result.error = generated.error.empty()
                       ? "incompatible_registration: missing lineage, owner or "
                         "generated state exports"
                       : generated.error;
    return result;
  }
  if (owner_code->contract().compiler.empty() ||
      owner_code->contract().target.empty()) {
    result.error =
        "incompatible_registration: owner has no frozen type contract";
    return result;
  }

  std::map<std::string, CandidateScope> groups;
  std::map<std::string, std::set<std::uint64_t>> initializer_ids;
  std::set<const SchemaRecord *> matched_schemas;
  for (const auto &entry : generated.states) {
    const auto &dependency = entry.type;
    const auto state_type =
        dependency.generic ? dependency.payload_type : dependency.state_type;
    const auto scope_lineage = empty_lineage(entry.scope_lineage_id)
                                   ? source_lineage_id
                                   : entry.scope_lineage_id;
    const auto initializer_group =
        scope_key(scope_lineage, dependency.scope_type);
    if (dependency.scope_type.empty() || state_type.empty() ||
        (dependency.generic && !dependency.state_type.empty()) ||
        (!dependency.generic && !dependency.payload_type.empty()) ||
        (dependency.generic && dependency.shared) ||
        !entry.initializer_identity ||
        !initializer_ids[initializer_group]
             .insert(entry.initializer_identity)
             .second) {
      result.error =
          "incompatible_registration: invalid or duplicate owner state "
          "identity";
      return result;
    }
    std::shared_ptr<const SchemaRecord> schema;
    const auto state_lineage = empty_lineage(entry.state_lineage_id)
                                   ? source_lineage_id
                                   : entry.state_lineage_id;
    for (const auto &candidate : schemas) {
      if (!candidate) {
        continue;
      }
      const auto &family = candidate->family;
      if (family.cpp_scope_type == dependency.scope_type &&
          family.cpp_state_type == state_type &&
          family.source_lineage_id == state_lineage) {
        if (schema) {
          result.error =
              "incompatible_registration: duplicate schema for state " +
              state_type;
          return result;
        }
        schema = candidate;
      }
    }
    if (!schema || schema->revision_id.empty() || schema->root_type.empty() ||
        schema->descriptor_set.empty() || !schema->root) {
      result.error =
          "incompatible_registration: missing immutable schema for " +
          dependency.scope_type + ":" + state_type;
      return result;
    }
    matched_schemas.insert(schema.get());
    CandidateState state;
    state.dependency = dependency;
    state.initializer_identity = entry.initializer_identity;
    state.lifecycle_fingerprint = entry.lifecycle_fingerprint;
    state.payload_provenance = entry.payload_provenance;
    state.schema = std::move(schema);
    state.identity =
        context_state_identity(state.schema->family.source_lineage_id,
                               dependency.scope_type, state_type);
    if (!resolve_function(*owner_code, entry.construct, state.construct,
                          result.error) ||
        !resolve_function(*owner_code, entry.destroy, state.destroy,
                          result.error) ||
        !resolve_function(*owner_code, entry.address, state.address,
                          result.error) ||
        !resolve_function(*owner_code, entry.serialize, state.serialize,
                          result.error)) {
      return result;
    }
    const auto key = scope_key(scope_lineage, dependency.scope_type);
    auto &group = groups[key];
    group.lineage = scope_lineage;
    group.scope_type = dependency.scope_type;
    group.identity =
        context_scope_identity(scope_lineage, dependency.scope_type);
    group.states.push_back(std::move(state));
  }

  for (const auto &schema : schemas) {
    if (schema && !::tracer::compat::contains(matched_schemas, schema.get())) {
      result.error = "incompatible_registration: schema has no generated owner "
                     "state serializer";
      return result;
    }
  }

  auto invocation = std::make_shared<OwnerInvocationContext>();
  invocation->code = owner_code;
  invocation->standard_library = owner_code->standard_library();
  const auto pinned_owner = std::shared_ptr<const void>(invocation);
  std::vector<std::shared_ptr<const RegisteredContextScope>> candidates;
  candidates.reserve(groups.size());
  for (auto &[key, group] : groups) {
    (void)key;
    std::sort(group.states.begin(), group.states.end(),
              [](const CandidateState &a, const CandidateState &b) {
                return a.identity < b.identity;
              });
    if (group.states.size() > context_engine::kMaxSlots) {
      result.error = "incompatible_registration: scope state group exceeds "
                     "engine slot capacity";
      return result;
    }
    for (std::size_t i = 1; i < group.states.size(); ++i) {
      if (group.states[i - 1].identity == group.states[i].identity) {
        result.error =
            "incompatible_registration: state identity hash collision";
        return result;
      }
    }
    auto registration = std::make_shared<context_engine::ScopeRegistration>();
    registration->identity = group.identity;
    registration->owner_code = pinned_owner;
    registration->invocation_context = invocation.get();
    registration->invoke = invoke_owner;
    registration->invoke_cleanup = invoke_owner_cleanup;
    registration->state_metadata_context = invocation.get();
    registration->state_metadata = lookup_state_metadata;
    registration->states.reserve(group.states.size());
    std::vector<RegisteredContextState> registered_states;
    registered_states.reserve(group.states.size());
    for (const auto &state : group.states) {
      context_engine::StateRegistration native_state;
      native_state.identity = state.identity;
      native_state.policy = state.dependency.shared
                                ? context_engine::Policy::shared
                                : context_engine::Policy::confined;
      native_state.construct = state.construct;
      native_state.destroy = state.destroy;
      native_state.address = state.address;
      registration->states.push_back(native_state);
      registered_states.emplace_back(
          state.identity, group.identity, state.dependency,
          state.initializer_identity, state.lifecycle_fingerprint, state.schema,
          state.serialize, invocation.get(), state.payload_provenance);
    }
    candidates.push_back(std::make_shared<const RegisteredContextScope>(
        group.lineage, group.scope_type, owner_code, std::move(registration),
        std::move(registered_states), owner_code->contract()));
  }

  std::size_t metadata_count = 0;
  for (const auto &candidate : candidates) {
    metadata_count += candidate->states.size();
  }
  invocation->state_metadata.reserve(metadata_count);
  for (const auto &candidate : candidates) {
    for (const auto &state : candidate->states) {
      invocation->state_metadata.push_back(
          std::make_unique<RegisteredContextState>(state));
    }
  }
  std::sort(invocation->state_metadata.begin(),
            invocation->state_metadata.end(),
            [](const auto &left, const auto &right) {
              return left->identity < right->identity;
            });

  std::lock_guard lock(mutex_);
  ::tracer::compat::erase_if(scopes_, [](const auto& scope) { return scope.expired(); });
  // Hash collisions are rejected across the complete transaction before any
  // new group is visible.
  for (std::size_t i = 0; i < candidates.size(); ++i) {
    const auto &candidate = candidates[i];
    const auto full_scope = scope_key(candidate->source_lineage_id,
                                      candidate->canonical_scope_type);
    for (std::size_t j = 0; j < i; ++j) {
      if (candidates[j]->identity == candidate->identity &&
          scope_key(candidates[j]->source_lineage_id,
                    candidates[j]->canonical_scope_type) != full_scope) {
        result.error =
            "incompatible_registration: scope identity hash collision";
        return result;
      }
    }
    for (std::size_t j = 0; j < i; ++j) {
      for (const auto &old_state : candidates[j]->states) {
        for (const auto &new_state : candidate->states) {
          const auto old_key = state_key(
              old_state.schema->family.source_lineage_id,
              candidates[j]->canonical_scope_type,
              old_state.dependency.generic ? old_state.dependency.payload_type
                                           : old_state.dependency.state_type);
          const auto new_key = state_key(
              new_state.schema->family.source_lineage_id,
              candidate->canonical_scope_type,
              new_state.dependency.generic ? new_state.dependency.payload_type
                                           : new_state.dependency.state_type);
          if (old_state.identity == new_state.identity && old_key == new_key &&
              candidates[j]->source_lineage_id !=
                  candidate->source_lineage_id) {
            result.error = "incompatible_scope_owner: one source generation "
                           "binds a StateFamily to multiple scope owners";
            return result;
          }
          if (old_state.identity == new_state.identity && old_key != new_key) {
            result.error = "incompatible_registration: state identity hash "
                           "collision";
            return result;
          }
        }
      }
    }
    for (const auto &existing_weak : scopes_) {
      const auto existing = existing_weak.lock();
      if (!existing) {
        continue;
      }
      if (existing->identity == candidate->identity &&
          scope_key(existing->source_lineage_id,
                    existing->canonical_scope_type) != full_scope) {
        result.error =
            "incompatible_registration: scope identity hash collision";
        return result;
      }
      for (const auto &old_state : existing->states) {
        for (const auto &new_state : candidate->states) {
          if (old_state.identity == new_state.identity &&
              state_key(old_state.schema->family.source_lineage_id,
                        existing->canonical_scope_type,
                        old_state.dependency.generic
                            ? old_state.dependency.payload_type
                            : old_state.dependency.state_type) !=
                  state_key(new_state.schema->family.source_lineage_id,
                            candidate->canonical_scope_type,
                            new_state.dependency.generic
                                ? new_state.dependency.payload_type
                                : new_state.dependency.state_type)) {
            result.error = "incompatible_registration: state identity hash "
                           "collision";
            return result;
          }
        }
      }
    }
  }

  const auto compatible_generation =
      [&](const std::shared_ptr<const RegisteredContextScope> &candidate) {
        const auto group = groups.find(scope_key(
            candidate->source_lineage_id, candidate->canonical_scope_type));
        if (group == groups.end()) {
          return std::shared_ptr<const RegisteredContextScope>{};
        }
        for (const auto &existing_weak : scopes_) {
          const auto existing = existing_weak.lock();
          if (existing &&
              same_scope(*existing, group->second, candidate->source_lineage_id,
                         candidate->type_contract)) {
            return existing;
          }
        }
        return std::shared_ptr<const RegisteredContextScope>{};
      };
  std::vector<std::shared_ptr<const RegisteredContextScope>> publish;
  publish.reserve(candidates.size());
  for (const auto &candidate : candidates) {
    if (const auto existing = compatible_generation(candidate)) {
      result.scopes.push_back(existing);
    } else {
      result.scopes.push_back(candidate);
      publish.push_back(candidate);
    }
  }
  std::vector<std::shared_ptr<const SchemaRecord>> native_schema_refs;
  for (const auto &candidate : publish)
    for (const auto &state : candidate->states)
      native_schema_refs.push_back(state.schema);
  if (!admit_native_schemas(native_schema_refs, result.error))
    return result;
  // New revisions and layouts are separate immutable generations. Existing
  // live scope records retain the registration they already pinned.
  scopes_.reserve(scopes_.size() + publish.size());
  for (const auto &candidate : publish) {
    scopes_.push_back(candidate);
  }
  return result;
}

std::shared_ptr<const RegisteredContextScope> ContextRegistry::find_scope(
    const std::array<std::uint8_t, 16> &source_lineage_id,
    std::string_view canonical_scope_type) const {
  std::shared_ptr<const RegisteredContextScope> result;
  std::lock_guard lock(mutex_);
  for (const auto &scope_weak : scopes_) {
    const auto scope = scope_weak.lock();
    if (!scope || scope->source_lineage_id != source_lineage_id ||
        scope->canonical_scope_type != canonical_scope_type) {
      continue;
    }
    if (result && result != scope) {
      return nullptr;
    }
    result = scope;
  }
  return result;
}

std::shared_ptr<const RegisteredContextScope>
ContextRegistry::find_scope(context_engine::Identity identity) const {
  std::shared_ptr<const RegisteredContextScope> result;
  std::lock_guard lock(mutex_);
  for (const auto &scope_weak : scopes_) {
    const auto scope = scope_weak.lock();
    if (!scope || scope->identity != identity) {
      continue;
    }
    if (result && result != scope) {
      return nullptr;
    }
    result = scope;
  }
  return result;
}

std::vector<std::shared_ptr<const RegisteredContextScope>>
ContextRegistry::find_scope_generations(
    const std::array<std::uint8_t, 16> &source_lineage_id,
    std::string_view canonical_scope_type) const {
  std::vector<std::shared_ptr<const RegisteredContextScope>> result;
  std::lock_guard lock(mutex_);
  for (const auto &scope_weak : scopes_) {
    auto scope = scope_weak.lock();
    if (scope && scope->source_lineage_id == source_lineage_id &&
        scope->canonical_scope_type == canonical_scope_type) {
      result.push_back(std::move(scope));
    }
  }
  return result;
}

RegisteredContextStateRef
ContextRegistry::find_state(context_engine::Identity scope_identity,
                            context_engine::Identity state_identity) const {
  auto scope = find_scope(scope_identity);
  return find_state(std::move(scope), state_identity);
}

RegisteredContextStateRef
ContextRegistry::find_state(std::shared_ptr<const RegisteredContextScope> scope,
                            context_engine::Identity state_identity) const {
  if (!scope) {
    return {};
  }
  const auto state = std::find_if(scope->states.begin(), scope->states.end(),
                                  [&](const auto &candidate) {
                                    return candidate.identity == state_identity;
                                  });
  return state == scope->states.end()
             ? RegisteredContextStateRef{}
             : RegisteredContextStateRef{std::move(scope), &*state};
}

std::size_t ContextRegistry::scope_count() const {
  std::lock_guard lock(mutex_);
  return static_cast<std::size_t>(
      std::count_if(scopes_.begin(), scopes_.end(),
                    [](const auto &scope) { return !scope.expired(); }));
}

} // namespace tracer

#include <any>
#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <llvm/AsmParser/Parser.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/SourceMgr.h>

#include "context_registry.hpp"
#include "context_test_config.hpp"

namespace tracer::test {
namespace {
void require(bool value, const char* message) {
  if (!value) {
    throw std::runtime_error(message);
  }
}

std::shared_ptr<const context_engine::SchemaRecord>
make_schema(const std::array<std::uint8_t, 16>& lineage, std::string state_type,
            std::string scope_type, std::string revision) {
  context_engine::StateFamilyKey family;
  family.source_lineage_id = lineage;
  family.cpp_state_type = std::move(state_type);
  family.cpp_scope_type = std::move(scope_type);
  auto root = std::make_shared<context_engine::SchemaMessage>();
  root->cpp_type = "test::Payload";
  root->full_name = "test.Payload";
  return std::make_shared<const context_engine::SchemaRecord>(
      std::move(family), "test.Payload", std::move(revision), "opaque-fds", std::move(root),
      std::vector<std::string>{"1=vector"});
}

std::shared_ptr<const PrivateContextCode> make_owner(bool extra_function = false,
                                                     bool changed_layout = false) {
  std::vector<std::string> arguments{"-std=c++20", "-fPIC", "-O0",
                                     "-resource-dir=" TRACER_TEST_RESOURCE_DIR};
  arguments.insert(arguments.end(), std::begin(kContextSystemArguments),
                   std::end(kContextSystemArguments));
  const std::string contract_source =
      "#include <string>\n"
      "namespace registry { struct Scope {}; struct First { int value; " +
      std::string(changed_layout ? "long extra; " : "") + "}; struct Second { int value; }; }\n" +
      "extern \"C\" int registry_contract() { return 0; }\n" +
      (extra_function ? "std::string unrelated_callback_helper() { return {}; }\n" : "");
  auto compiled =
      compile_context_module(contract_source, arguments, {"registry::First", "registry::Second"});
  if (!compiled) {
    throw std::runtime_error(llvm::toString(compiled.takeError()));
  }
  auto context = std::make_unique<llvm::LLVMContext>();
  llvm::SMDiagnostic diagnostic;
  auto module = llvm::parseAssemblyString("define void @make_one() { ret void }\n"
                                          "define void @drop_one() { ret void }\n"
                                          "define void @ptr_one() { ret void }\n"
                                          "define void @write_one() { ret void }\n"
                                          "define void @make_two() { ret void }\n"
                                          "define void @drop_two() { ret void }\n"
                                          "define void @ptr_two() { ret void }\n"
                                          "define void @write_two() { ret void }\n",
                                          diagnostic, *context);
  require(static_cast<bool>(module), "registry test IR did not parse");
  module->setTargetTriple(llvm::Triple(compiled->contract.target));
  module->setDataLayout(compiled->contract.data_layout);
  auto owner = PrivateContextCode::load(
      llvm::orc::ThreadSafeModule(std::move(module), std::move(context)), compiled->contract,
      {"make_one", "drop_one", "ptr_one", "write_one", "make_two", "drop_two", "ptr_two",
       "write_two"});
  if (!owner) {
    throw std::runtime_error(llvm::toString(owner.takeError()));
  }
  return std::move(*owner);
}

GeneratedOwnerState state(std::string type, std::uint64_t initializer, std::string prefix) {
  GeneratedOwnerState result;
  result.type.state_type = std::move(type);
  result.type.scope_type = "registry::Scope";
  result.type.shared = true;
  result.construct = "make_" + prefix;
  result.destroy = "drop_" + prefix;
  result.address = "ptr_" + prefix;
  result.serialize = "write_" + prefix;
  result.initializer_identity = initializer;
  return result;
}

context_engine::ErrorCode context_test_construct(const context_engine::ScopeInit&,
                                                 std::any& payload) noexcept {
  try {
    payload = std::make_shared<int>(7);
    return context_engine::ErrorCode::ok;
  } catch (...) {
    return context_engine::ErrorCode::construction_failed;
  }
}

void context_test_destroy(std::any& payload) noexcept {
  payload.reset();
}

void* context_test_address(std::any& payload) noexcept {
  const auto* value = std::any_cast<std::shared_ptr<int>>(&payload);
  return value ? value->get() : nullptr;
}

std::shared_ptr<const context_engine::ScopeRegistration>
engine_registration(const RegisteredContextScope& scope) {
  auto registration = std::make_shared<context_engine::ScopeRegistration>(*scope.registration);
  registration->owner_code = std::make_shared<int>(1);
  registration->invocation_context = nullptr;
  registration->invoke = nullptr;
  for (auto& state : registration->states) {
    state.construct = context_test_construct;
    state.destroy = context_test_destroy;
    state.address = context_test_address;
  }
  return registration;
}
} // namespace

void verify_context_registry() {
  std::array<std::uint8_t, 16> lineage{};
  lineage[0] = 0x31;
  std::array<std::uint8_t, 16> other_lineage = lineage;
  other_lineage[0] = 0x32;
  const auto scope_id = context_scope_identity(lineage, "registry::Scope");
  require(scope_id && scope_id == context_scope_identity(lineage, "registry::Scope") &&
              scope_id != context_scope_identity(other_lineage, "registry::Scope"),
          "scope family identity is not stable or lineage-specific");
  const auto state_id = context_state_identity(lineage, "registry::Scope", "registry::First");
  require(state_id && state_id != scope_id &&
              state_id != context_state_identity(lineage, "registry::Scope", "registry::Second"),
          "state identity is not type-specific");

  ContextRegistry registry;
  GeneratedContextOwner invalid;
  const auto rejected = registry.register_owner(lineage, nullptr, invalid, {});
  require(!rejected && registry.scope_count() == 0,
          "registry admitted an owner without code or generated exports");

  auto owner = make_owner();
  GeneratedContextOwner generated;
  generated.states.push_back(state("registry::First", 1, "one"));
  generated.states.push_back(state("registry::Second", 2, "two"));
  const auto first_schema = make_schema(lineage, "registry::First", "registry::Scope", "rev-A");
  const auto second_schema = make_schema(lineage, "registry::Second", "registry::Scope", "rev-B");
  const std::array<std::shared_ptr<const context_engine::SchemaRecord>, 2> schemas{first_schema,
                                                                                   second_schema};
  const auto registered = registry.register_owner(lineage, owner, generated, schemas);
  require(registered && registered.scopes.size() == 1 && registered.scopes[0]->states.size() == 2 &&
              registered.scopes[0]->identity == scope_id &&
              registered.scopes[0]->registration->states.size() == 2 && registry.scope_count() == 1,
          "registry did not freeze the complete scope state group");
  const auto found = registry.find_scope(scope_id);
  require(found == registered.scopes[0] && registry.find_scope(lineage, "registry::Scope") == found,
          "registry did not return the immutable scope generation");
  const auto state_ref = registry.find_state(scope_id, state_id);
  require(state_ref && state_ref.owner == found && state_ref.state->schema == first_schema,
          "stable state identity did not resolve its original schema");
  const auto* pinned_metadata = registered_context_state(*found->registration, state_id);
  require(pinned_metadata && pinned_metadata->schema == first_schema &&
              pinned_metadata->serialize && pinned_metadata->owner_invocation_context,
          "scope registration did not pin state capture metadata");
  const auto capture_state = state_ref.state->capture_view(41, 9, reinterpret_cast<void*>(1));
  require(capture_state.scope_id == 41 && capture_state.epoch_schema_id == 9 &&
              capture_state.schema_identity == "rev-A" &&
              capture_state.payload == reinterpret_cast<void*>(1) &&
              capture_state.encode_context == state_ref.state && capture_state.encode,
          "registered state did not provide capture metadata");

  const auto second_owner = make_owner(true);
  auto reordered = generated;
  reordered.states[0].initializer_identity = 19;
  reordered.states[1].initializer_identity = 23;
  std::swap(reordered.states[0], reordered.states[1]);
  const auto reused = registry.register_owner(lineage, second_owner, reordered, schemas);
  require(reused && reused.scopes.size() == 1 && reused.scopes[0] == registered.scopes[0] &&
              reused.scopes[0]->owner_generation == owner && registry.scope_count() == 1,
          "compatible independent interceptor did not reuse its scope group");
  require(reused.scopes[0]->states[0].initializer_identity ==
              registered.scopes[0]->states[0].initializer_identity,
          "registry replaced owner-local initializer identities on reuse");

  const auto changed_owner = make_owner(false, true);
  const auto changed_first_schema =
      make_schema(lineage, "registry::First", "registry::Scope", "rev-A2");
  const std::array<std::shared_ptr<const context_engine::SchemaRecord>, 2> changed_layout_schemas{
      changed_first_schema, second_schema};
  const auto changed_layout =
      registry.register_owner(lineage, changed_owner, generated, changed_layout_schemas);
  require(changed_layout && changed_layout.scopes.size() == 1 &&
              changed_layout.scopes[0] != registered.scopes[0] &&
              changed_layout.scopes[0]->owner_generation == changed_owner &&
              registry.scope_count() == 2,
          "changed layout did not create a separately pinned generation");
  require(!registry.find_scope(scope_id) && !registry.find_scope(lineage, "registry::Scope") &&
              registry.find_scope_generations(lineage, "registry::Scope").size() == 2 &&
              !registry.find_state(scope_id, state_id),
          "ambiguous family lookup guessed a schema generation");
  const auto old_generation_state = registry.find_state(registered.scopes[0], state_id);
  const auto new_generation_state = registry.find_state(changed_layout.scopes[0], state_id);
  require(old_generation_state && new_generation_state &&
              old_generation_state.state->schema == first_schema &&
              new_generation_state.state->schema == changed_first_schema &&
              old_generation_state.state->serialize != new_generation_state.state->serialize,
          "generation-specific lookup did not retain original schemas");

  const auto old_native_registration = engine_registration(*registered.scopes[0]);
  const auto new_native_registration = engine_registration(*changed_layout.scopes[0]);
  context_engine::Diagnostics old_diagnostics;
  context_engine::Context old_context(old_diagnostics);
  {
    context_engine::InvocationFrame old_frame(old_context);
    old_frame.create_or_attach(old_native_registration);
    context_engine::InvocationFrame new_frame(old_context);
    bool rejected_live_rebind = false;
    try {
      new_frame.create_or_attach(new_native_registration);
    } catch (const context_engine::Error& error) {
      rejected_live_rebind = error.code() == context_engine::ErrorCode::incompatible_registration;
    }
    require(rejected_live_rebind && old_context.binding_count() == 1,
            "new generation reinterpreted an existing live scope record");
  }
  context_engine::Diagnostics new_diagnostics;
  context_engine::Context new_context(new_diagnostics);
  {
    context_engine::InvocationFrame new_frame(new_context);
    new_frame.create_or_attach(new_native_registration);
    context_engine::HookExit hook(new_context);
    const context_engine::SlotRequest new_slot{new_native_registration, state_id};
    context_engine::HookAccess access(new_context, std::span(&new_slot, std::size_t{1}));
    require(access.ref(new_slot) != nullptr,
            "fresh context could not use the new state generation");
  }

  auto incomplete = generated;
  incomplete.states.pop_back();
  const std::array<std::shared_ptr<const context_engine::SchemaRecord>, 1> incomplete_schemas{
      first_schema};
  const auto shortened_group =
      registry.register_owner(lineage, second_owner, incomplete, incomplete_schemas);
  require(shortened_group && shortened_group.scopes.size() == 1 &&
              shortened_group.scopes[0]->states.size() == 1 && registry.scope_count() == 3,
          "changed complete state group did not create a new generation");

  auto changed_schema = make_schema(lineage, "registry::First", "registry::Scope", "rev-changed");
  const std::array<std::shared_ptr<const context_engine::SchemaRecord>, 2> changed_schemas{
      changed_schema, second_schema};
  const auto rejected_revision =
      registry.register_owner(lineage, second_owner, generated, changed_schemas);
  require(rejected_revision && rejected_revision.scopes.size() == 1 &&
              rejected_revision.scopes[0] != registered.scopes[0] &&
              rejected_revision.scopes[0] != changed_layout.scopes[0] &&
              registry.scope_count() == 4,
          "schema revision did not create an independent immutable generation");

  const auto missing_schema =
      make_schema(lineage, "registry::First", "registry::Scope", "rev-new-lineage");
  const std::array<std::shared_ptr<const context_engine::SchemaRecord>, 1> incomplete_schema_set{
      missing_schema};
  auto second_scope_state = state("registry::Third", 3, "two");
  second_scope_state.type.scope_type = "registry::OtherScope";
  GeneratedContextOwner atomic;
  atomic.states.push_back(state("registry::First", 1, "one"));
  atomic.states.push_back(std::move(second_scope_state));
  const auto rejected_atomic =
      registry.register_owner(lineage, second_owner, atomic, incomplete_schema_set);
  require(!rejected_atomic && registry.scope_count() == 4 &&
              !registry.find_scope(lineage, "registry::OtherScope"),
          "multi-scope owner registration published a partial transaction");

  GeneratedContextOwner alternate_scope_owner;
  auto alternate_scope_state = state("registry::First", 1, "one");
  alternate_scope_state.scope_lineage_id = other_lineage;
  alternate_scope_owner.states.push_back(std::move(alternate_scope_state));
  const std::array<std::shared_ptr<const context_engine::SchemaRecord>, 1> alternate_scope_schemas{
      first_schema};
  const auto alternate_scope = registry.register_owner(lineage, second_owner, alternate_scope_owner,
                                                       alternate_scope_schemas);
  require(alternate_scope && alternate_scope.scopes.size() == 1 &&
              alternate_scope.scopes[0]->identity != scope_id &&
              alternate_scope.scopes[0]->states[0].identity == state_id,
          "separate scope-owner generation did not retain the stable state family");

  GeneratedContextOwner ambiguous_scope_owner;
  auto first_binding = state("registry::First", 11, "one");
  auto second_binding = state("registry::First", 12, "two");
  first_binding.scope_lineage_id = lineage;
  second_binding.scope_lineage_id = other_lineage;
  ambiguous_scope_owner.states.push_back(std::move(first_binding));
  ambiguous_scope_owner.states.push_back(std::move(second_binding));
  const auto rejected_ambiguous_scope = registry.register_owner(
      lineage, second_owner, ambiguous_scope_owner, alternate_scope_schemas);
  require(!rejected_ambiguous_scope &&
              rejected_ambiguous_scope.error.find("incompatible_scope_owner") !=
                  std::string::npos &&
              registry.scope_count() == 5,
          "one source generation bound a state family to multiple scope owners");

  auto payload_owner_lineage = lineage;
  payload_owner_lineage[0] = 0x51;
  auto payload_scope_lineage = lineage;
  payload_scope_lineage[0] = 0x52;
  const std::string payload_type = "std::vector<lineage_fixture::UserRecord>";
  const auto payload_schema =
      make_schema(payload_owner_lineage, payload_type, "registry::GenericScope", "payload-rev");
  const std::array<std::shared_ptr<const context_engine::SchemaRecord>, 1> payload_schemas{
      payload_schema};
  GeneratedContextOwner payload_generation;
  auto payload_state = state(payload_type, 21, "one");
  payload_state.type.state_type.clear();
  payload_state.type.generic = true;
  payload_state.type.shared = false;
  payload_state.type.scope_type = "registry::GenericScope";
  payload_state.type.payload_type = payload_type;
  payload_state.state_lineage_id = payload_owner_lineage;
  payload_state.scope_lineage_id = payload_scope_lineage;
  payload_state.payload_provenance.push_back({"lineage_fixture::UserRecord", other_lineage});
  payload_generation.states.push_back(payload_state);
  const auto first_payload =
      registry.register_owner(lineage, second_owner, payload_generation, payload_schemas);
  require(first_payload && first_payload.scopes.size() == 1 &&
              first_payload.scopes[0]->states[0].payload_provenance ==
                  payload_state.payload_provenance,
          "generic payload package provenance was not frozen in the scope record");

  auto other_payload_generation = payload_generation;
  other_payload_generation.states[0].payload_provenance[0].source_lineage_id = lineage;
  const auto changed_payload_provenance =
      registry.register_owner(lineage, second_owner, other_payload_generation, payload_schemas);
  require(changed_payload_provenance &&
              changed_payload_provenance.scopes[0] != first_payload.scopes[0] &&
              changed_payload_provenance.scopes[0]->states[0].identity ==
                  first_payload.scopes[0]->states[0].identity &&
              registry.scope_count() == 7,
          "generic payload provenance change incorrectly reused owner code");
}

} // namespace tracer::test

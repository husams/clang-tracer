#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>

#include "context_codec.hpp"
#include "context_receiver.hpp"
#include "context_test_config.hpp"
#include "tracer/compat.hpp"

void verify_context_schema_roundtrip();

namespace {
struct SchemaRecordFinder final
    : clang::RecursiveASTVisitor<SchemaRecordFinder> {
  std::string name;
  const clang::CXXRecordDecl *record = nullptr;
  bool VisitCXXRecordDecl(clang::CXXRecordDecl *r) {
    if (r->isThisDeclarationADefinition() &&
        r->getQualifiedNameAsString() == name)
      record = r;
    return true;
  }
};
struct SchemaAliasFinder final : clang::RecursiveASTVisitor<SchemaAliasFinder> {
  std::string name;
  const clang::TypeAliasDecl *alias = nullptr;
  bool VisitTypeAliasDecl(clang::TypeAliasDecl *candidate) {
    if (candidate->getNameAsString() == name)
      alias = candidate;
    return true;
  }
};
} // namespace

namespace tracer::context_engine {
void verify_context_schema() {
  std::ifstream fixture(TRACER_TEST_SOURCE_DIR
                        "/tests/fixtures/shared_context_nested.cpp");
  const std::string source{std::istreambuf_iterator<char>(fixture), {}};
  std::vector<std::string> arguments{"-std=c++20",
                                     "-I" TRACER_TEST_SOURCE_DIR "/include",
                                     "-I" TRACER_TEST_SOURCE_DIR "/src",
                                     "-resource-dir=" TRACER_TEST_RESOURCE_DIR};
  arguments.insert(arguments.end(), std::begin(kContextSystemArguments),
                   std::end(kContextSystemArguments));
  auto ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n" + source, arguments,
      "context-schema-fixture.cc");
  if (!ast || ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("shared context schema fixture failed to parse");
  SchemaRecordFinder state_finder;
  state_finder.name = "demo::RequestState";
  state_finder.TraverseDecl(ast->getASTContext().getTranslationUnitDecl());
  const auto *state = state_finder.record;
  if (!state)
    throw std::runtime_error("shared state fixture declaration not found");
  const auto generated_codec = render_state_codec_source(*state);
  if (!generated_codec)
    throw std::runtime_error("automatic C++ codec generation failed: " +
                             generated_codec.error);
  auto codec_ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n#include \"context_codec.hpp\"\n" +
          source + generated_codec.source,
      arguments, "context-codec-fixture.cc");
  if (!codec_ast || codec_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error(
        "generated recursive C++ codec did not compile in the fixture AST");
  const std::string template_source = R"cpp(
namespace schema_template {
template<class T> struct Box { T value{}; };
struct Outer { Box<int> count; Box<double> ratio; };
}
)cpp";
  auto template_ast = clang::tooling::buildASTFromCodeWithArgs(
      template_source, arguments, "template-schema-fixture.cc");
  if (!template_ast || template_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("template schema fixture failed to parse");
  SchemaRecordFinder template_finder;
  template_finder.name = "schema_template::Outer";
  template_finder.TraverseDecl(
      template_ast->getASTContext().getTranslationUnitDecl());
  if (!template_finder.record)
    throw std::runtime_error("template outer record not found");
  const auto template_type = clang::QualType(
      template_finder.record->getTypeForDecl(), 0);
  StateFamilyKey template_family;
  template_family.source_lineage_id[0] = 0x74;
  template_family.cpp_state_type = "schema_template::Outer";
  template_family.cpp_scope_type = "schema_template::Scope";
  const auto template_schema = build_payload_schema(
      template_ast->getASTContext(), template_type, template_family);
  if (!template_schema)
    throw std::runtime_error("template schema failed: " + template_schema.error);
  assert(template_schema.record->root->fields.size() == 1);
  assert(template_schema.record->root->fields[0].message);
  const auto &template_fields =
      template_schema.record->root->fields[0].message->fields;
  assert(template_fields.size() == 2);
  assert(template_fields[0].cpp_type == "schema_template::Box<int>");
  assert(template_fields[1].cpp_type == "schema_template::Box<double>");
  assert(template_fields[0].message && template_fields[1].message);
  assert(template_fields[0].message->cpp_type == template_fields[0].cpp_type);
  assert(template_fields[1].message->cpp_type == template_fields[1].cpp_type);
  assert(template_fields[0].message->full_name !=
         template_fields[1].message->full_name);
  const auto template_codec = render_payload_codec_source(
      template_ast->getASTContext(), template_type);
  if (!template_codec)
    throw std::runtime_error("template codec failed: " + template_codec.error);
  auto template_codec_ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"context_codec.hpp\"\n" + template_source +
          template_codec.source,
      arguments, "template-codec-fixture.cc");
  if (!template_codec_ast ||
      template_codec_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("generated template codec did not compile");
  StateFamilyKey family;
  family.source_lineage_id[0] = 1;
  family.cpp_state_type = "demo::RequestState";
  family.cpp_scope_type = "demo::RequestScope";
  auto schema = build_state_schema(ast->getASTContext(), *state, family);
  if (!schema)
    throw std::runtime_error("automatic schema discovery failed: " +
                             schema.error);
  if (std::find(schema.record->container_roles.begin(),
                schema.record->container_roles.end(),
                "2=vector") == schema.record->container_roles.end() ||
      std::find(schema.record->container_roles.begin(),
                schema.record->container_roles.end(),
                "3=map") == schema.record->container_roles.end())
    throw std::runtime_error(
        "canonical container role metadata was not retained");
  auto user_hash_arguments = arguments;
  user_hash_arguments.emplace_back("-isystem");
  user_hash_arguments.emplace_back(TRACER_TEST_SOURCE_DIR "/tests/fixtures");
  auto user_hash_ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"context_user_hash_specialization.hpp\"\n",
      user_hash_arguments, "context-user-hash-fixture.cc");
  if (!user_hash_ast || user_hash_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error(
        "user specialization -isystem fixture failed to parse");
  SchemaAliasFinder user_hash_alias;
  user_hash_alias.name = "ContextSchemaUserHashPayload";
  user_hash_alias.TraverseDecl(
      user_hash_ast->getASTContext().getTranslationUnitDecl());
  if (!user_hash_alias.alias)
    throw std::runtime_error("user hash specialization alias is absent");
  const auto user_hash_type = user_hash_alias.alias->getUnderlyingType();
  StateFamilyKey user_hash_family;
  user_hash_family.source_lineage_id[0] = 0x73;
  user_hash_family.cpp_state_type =
      user_hash_type.getNonReferenceType().getCanonicalType().getAsString();
  user_hash_family.cpp_scope_type = "demo::RequestScope";
  const auto user_hash_schema = build_payload_schema(
      user_hash_ast->getASTContext(), user_hash_type, user_hash_family);
  if (user_hash_schema ||
      user_hash_schema.error.find("hash") == std::string::npos ||
      user_hash_schema.error.find("specialization") == std::string::npos) {
    throw std::runtime_error(
        "user std::hash specialization in a -isystem project header was "
        "admitted or rejected without a hash-specialization diagnostic: " +
        user_hash_schema.error);
  }
  google::protobuf::DescriptorPool pool;
  google::protobuf::FileDescriptorSet definitions;
  if (!definitions.ParseFromString(schema.record->descriptor_set))
    throw std::runtime_error("descriptor set did not parse");
  for (const auto &f : definitions.file())
    if (!pool.BuildFile(f))
      throw std::runtime_error(
          "generated recursive descriptors failed validation");
  const auto *root = pool.FindMessageTypeByName(schema.record->root_type);
  if (!root || root->field_count() != 4)
    throw std::runtime_error("root descriptor lost fixture fields");
  std::shared_ptr<const SchemaRecord> schema_ref = schema.record;
  const std::array<std::string_view, 2> nested_path{"items", "sku"};
  const auto query = query_field_path(
      ::tracer::compat::span<const std::shared_ptr<const SchemaRecord>>(&schema_ref, 1),
      nested_path);
  if (query.size() != 1 || !query[0].present ||
      query[0].kind != SchemaKind::string)
    throw std::runtime_error("recursive cross-revision field query failed");
  StateFamilyKey int_family;
  int_family.source_lineage_id[0] = 0x71;
  int_family.cpp_state_type = "int";
  int_family.cpp_scope_type = "demo::RequestScope";
  auto int_payload = build_payload_schema(
      ast->getASTContext(), ast->getASTContext().IntTy, int_family);
  if (!int_payload || int_payload.record->root->fields.size() != 1 ||
      int_payload.record->root->fields[0].name != "value" ||
      int_payload.record->root->fields[0].kind != SchemaKind::signed_integer)
    throw std::runtime_error("generic scalar payload wrapper schema failed");
  auto generic_payload_ref =
      std::shared_ptr<const SchemaRecord>(int_payload.record);
  const std::array<std::string_view, 1> generic_value_path{"value"};
  const auto generic_query = query_field_path(
      ::tracer::compat::span<const std::shared_ptr<const SchemaRecord>>(&generic_payload_ref, 1),
      generic_value_path);
  if (generic_query.size() != 1 || !generic_query[0].present ||
      generic_query[0].cpp_type != "int")
    throw std::runtime_error("generic payload original-type query failed");
  const auto request_id_type = state->field_begin()->getType();
  StateFamilyKey string_family;
  string_family.source_lineage_id[0] = 0x72;
  string_family.cpp_state_type =
      request_id_type.getNonReferenceType().getCanonicalType().getAsString();
  string_family.cpp_scope_type = "demo::RequestScope";
  auto string_payload = build_payload_schema(ast->getASTContext(),
                                             request_id_type, string_family);
  if (!string_payload ||
      string_payload.record->root->fields[0].kind != SchemaKind::string)
    throw std::runtime_error("generic string payload wrapper schema failed: " +
                             string_payload.error);

  const std::string extended_source =
      "#include <array>\n#include <cstddef>\n#include <deque>\n"
      "#include <functional>\n#include <list>\n#include <optional>\n"
      "#include <set>\n#include <string>\n#include <unordered_map>\n"
      "#include <unordered_set>\n#include <vector>\n" +
      source + R"cpp(
namespace extended {
struct Scope {};
enum class Mode { zero=0, active=1 };
struct Nested { Mode mode; std::array<double,2> fixed; };
struct ContainerState : shared_state<Scope> {
  std::deque<std::list<std::string>> queues;
  std::set<int> ids;
  std::unordered_set<bool> flags;
  std::unordered_map<std::string,std::unordered_map<unsigned,std::optional<Nested>>> values;
};
}
namespace unsupported_schema {
struct Scope {};
struct ReverseLess { bool operator()(int,int) const; };
struct ReverseState : shared_state<Scope> { std::set<int,ReverseLess> values; };
template<class T> struct Allocator { using value_type=T; Allocator()=default; template<class U> Allocator(const Allocator<U>&){} T* allocate(std::size_t); void deallocate(T*,std::size_t); };
struct AllocatorState : shared_state<Scope> { std::vector<int,Allocator<int>> values; };
struct HashRecord {};
struct LessRecord {};
enum class EqualRecord { value };
}
namespace std {
template<> struct hash<unsupported_schema::HashRecord> { size_t operator()(const unsupported_schema::HashRecord&) const noexcept { return 1; } };
template<> struct less<unsupported_schema::LessRecord> { bool operator()(const unsupported_schema::LessRecord&,const unsupported_schema::LessRecord&) const noexcept { return false; } };
template<> struct equal_to<unsupported_schema::EqualRecord> { bool operator()(const unsupported_schema::EqualRecord&,const unsupported_schema::EqualRecord&) const noexcept { return true; } };
}
namespace unsupported_schema {
struct HashState : shared_state<Scope> { std::unordered_set<HashRecord> values; };
struct LessState : shared_state<Scope> { std::set<LessRecord> values; };
struct EqualState : shared_state<Scope> { std::unordered_map<EqualRecord,int> values; };
}
)cpp";
  auto extended_ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n" + extended_source, arguments,
      "context-schema-extended.cc");
  if (!extended_ast || extended_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("extended container fixture failed to parse");
  SchemaRecordFinder container_finder;
  container_finder.name = "extended::ContainerState";
  container_finder.TraverseDecl(
      extended_ast->getASTContext().getTranslationUnitDecl());
  const auto *container_state = container_finder.record;
  if (!container_state)
    throw std::runtime_error("extended container state declaration not found");
  StateFamilyKey extended_family;
  extended_family.source_lineage_id[0] = 2;
  extended_family.cpp_state_type = "extended::ContainerState";
  extended_family.cpp_scope_type = "extended::Scope";
  auto extended_schema = build_state_schema(extended_ast->getASTContext(),
                                            *container_state, extended_family);
  if (!extended_schema)
    throw std::runtime_error("extended standard container schema rejected: " +
                             extended_schema.error);
  auto extended_codec = render_state_codec_source(*container_state);
  if (!extended_codec)
    throw std::runtime_error("extended recursive codec generation failed: " +
                             extended_codec.error);
  auto extended_codec_ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n#include \"context_codec.hpp\"\n" +
          extended_source + extended_codec.source,
      arguments, "context-codec-extended.cc");
  if (!extended_codec_ast ||
      extended_codec_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("extended generated codec did not parse");
  google::protobuf::FileDescriptorSet extended_set;
  if (!extended_set.ParseFromString(extended_schema.record->descriptor_set))
    throw std::runtime_error("extended descriptor bytes malformed");
  google::protobuf::DescriptorPool extended_pool;
  for (const auto &f : extended_set.file())
    if (!extended_pool.BuildFile(f))
      throw std::runtime_error("nested container descriptors invalid");
  for (const auto *name :
       {"unsupported_schema::ReverseState",
        "unsupported_schema::AllocatorState", "unsupported_schema::HashState",
        "unsupported_schema::LessState", "unsupported_schema::EqualState"}) {
    SchemaRecordFinder bad_finder;
    bad_finder.name = name;
    bad_finder.TraverseDecl(
        extended_ast->getASTContext().getTranslationUnitDecl());
    if (!bad_finder.record)
      throw std::runtime_error(
          "unsupported-specialization fixture declaration missing");
    StateFamilyKey bad_family;
    bad_family.source_lineage_id[0] = 9;
    bad_family.cpp_state_type = name;
    bad_family.cpp_scope_type = "unsupported_schema::Scope";
    auto rejected = build_state_schema(extended_ast->getASTContext(),
                                       *bad_finder.record, bad_family);
    if (rejected)
      throw std::runtime_error(
          "custom standard-library specialization was accepted: " +
          std::string(name));
  }

  std::array<std::byte, 1024> storage{};
  BoundedProtoWriter writer(storage);
  EncodeBudget budget;
  struct TruncationCheck {
    bool called = false, has_key = false;
    std::uint64_t total = 0, captured = 0;
    std::uint32_t field = 0;
  } trunc;
  budget.max_items = 1;
  budget.truncation = [](void* opaque, ::tracer::compat::span<const EncodeBudget::PathStep> path,
                         std::uint64_t total, std::uint64_t captured) noexcept {
    auto &c = *static_cast<TruncationCheck *>(opaque);
    c.called = true;
    c.total = total;
    c.captured = captured;
    if (!path.empty() &&
        path.back().selector == EncodeBudget::Selector::field_number)
      c.field = static_cast<std::uint32_t>(path.back().number);
    for (const auto &step : path)
      if (step.selector == EncodeBudget::Selector::string_key &&
          step.string_value == "billing")
        c.has_key = true;
    return true;
  };
  budget.truncation_context = &trunc;
  std::vector<std::string> names{"a", "b"};
  assert(encode_value(writer, 1, std::uint32_t{17}, budget));
  assert(encode_value(writer, 2, names, budget));
  assert(trunc.called && trunc.total == 2 && trunc.captured == 1 &&
         trunc.field == 2);
  std::map<std::string, std::vector<int>> nested{{"billing", {3, 4}}};
  assert(encode_value(writer, 3, nested, budget));
  assert(trunc.has_key && trunc.total == 2 && trunc.captured == 1);
  auto wire = writer.finish();
  assert(!wire.empty());

  google::protobuf::FileDescriptorSet set;
  auto *file = set.add_file();
  file->set_name("fixture.proto");
  file->set_package("fixture");
  file->set_syntax("proto3");
  auto *message = file->add_message_type();
  message->set_name("State");
  auto *field = message->add_field();
  field->set_name("value");
  field->set_number(1);
  field->set_label(google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL);
  field->set_type(google::protobuf::FieldDescriptorProto::TYPE_UINT32);
  auto ledger = std::make_shared<ReceiverLedger>();
  StateFamilyKey receiver_family;
  receiver_family.source_lineage_id[0] = 7;
  receiver_family.cpp_state_type = "fixture::State";
  receiver_family.cpp_scope_type = "fixture::Scope";
  ContextReceiver receiver(ledger);
  assert(receiver.register_schema(1, set, "fixture.State", receiver_family));
  auto changed = set;
  changed.mutable_file(0)->mutable_message_type(0)->mutable_field(0)->set_type(
      google::protobuf::FieldDescriptorProto::TYPE_STRING);
  assert(
      !receiver.register_schema(1, changed, "fixture.State", receiver_family));
  auto role_ledger = std::make_shared<ReceiverLedger>();
  ContextReceiver invalid_roles(role_ledger);
  const std::array<std::string, 1> scalar_role{"1=map"};
  assert(!invalid_roles.register_schema(1, set, "fixture.State",
                                        receiver_family, nullptr, scalar_role));
  auto repeated_set = set;
  auto *repeated_root = repeated_set.mutable_file(0)->mutable_message_type(0);
  repeated_root->mutable_field(0)->set_label(
      google::protobuf::FieldDescriptorProto::LABEL_REPEATED);
  auto *second_repeated = repeated_root->add_field();
  second_repeated->set_name("names");
  second_repeated->set_number(2);
  second_repeated->set_label(
      google::protobuf::FieldDescriptorProto::LABEL_REPEATED);
  second_repeated->set_type(
      google::protobuf::FieldDescriptorProto::TYPE_STRING);
  ContextReceiver role_validation(std::make_shared<ReceiverLedger>());
  assert(!role_validation.register_schema(20, repeated_set, "fixture.State",
                                          receiver_family));
  const std::array<std::string, 2> ordered_roles{"1=vector", "2=list"};
  assert(role_validation.register_schema(21, repeated_set, "fixture.State",
                                         receiver_family, nullptr,
                                         ordered_roles));
  ContextReceiver bad_role_validation(std::make_shared<ReceiverLedger>());
  const std::array<std::string, 1> incomplete_roles{"1=vector"};
  assert(!bad_role_validation.register_schema(22, repeated_set, "fixture.State",
                                              receiver_family, nullptr,
                                              incomplete_roles));
  const std::array<std::string, 2> unordered_roles{"2=list", "1=vector"};
  assert(!bad_role_validation.register_schema(23, repeated_set, "fixture.State",
                                              receiver_family, nullptr,
                                              unordered_roles));
  const std::array<std::string, 2> duplicate_target_roles{"1=list", "1=vector"};
  assert(!bad_role_validation.register_schema(24, repeated_set, "fixture.State",
                                              receiver_family, nullptr,
                                              duplicate_target_roles));
  StateFamilyKey boundary_a = receiver_family;
  boundary_a.cpp_state_type = "ab";
  boundary_a.cpp_scope_type = "c";
  StateFamilyKey boundary_b = receiver_family;
  boundary_b.cpp_state_type = "a";
  boundary_b.cpp_scope_type = "bc";
  assert(compute_schema_revision_id("descriptor", boundary_a, "root", {}) !=
         compute_schema_revision_id("descriptor", boundary_b, "root", {}));
  const auto schema_charge = ledger->retained_schema_bytes();
  std::byte payload[]{std::byte{0x08}, std::byte{0x2a}};
  auto decoded = receiver.decode(1, payload);
  assert(decoded && decoded->schema());
  std::byte unknown_field[]{std::byte{0x10}, std::byte{0x01}};
  assert(!receiver.decode(1, unknown_field));
  receiver.close_epoch();
  assert(decoded->message()->GetDescriptor()->full_name() == "fixture.State");
  assert(ledger->retained_schema_bytes() == schema_charge);
  decoded.reset();
  assert(ledger->retained_schema_bytes() == 0);
  auto legacy_ledger = std::make_shared<ReceiverLedger>();
  ContextReceiver legacy_a(legacy_ledger), legacy_b(legacy_ledger);
  assert(legacy_a.register_legacy_schema(1, set, "fixture.State"));
  assert(legacy_b.register_legacy_schema(1, set, "fixture.State"));
  auto legacy_state_a = legacy_a.decode(1, payload);
  auto legacy_state_b = legacy_b.decode(1, payload);
  assert(legacy_state_a && legacy_state_b &&
         legacy_state_a->schema()->family != legacy_state_b->schema()->family);
  auto batch_ledger = std::make_shared<ReceiverLedger>();
  ContextReceiver batch_receiver(batch_ledger);
  std::array<SchemaRegistration, 2> batch;
  batch[0].id = 10;
  batch[0].descriptor_set = set;
  batch[0].root = "fixture.State";
  batch[0].family = receiver_family;
  batch[1] = batch[0];
  batch[1].id = 11;
  batch[1].container_roles = {"1=map"};
  assert(!batch_receiver.register_schema_set(batch));
  assert(batch_receiver.schema_count() == 0 &&
         batch_ledger->retained_schema_bytes() == 0);
  batch[1].container_roles.clear();
  assert(batch_receiver.register_schema_set(batch));
  assert(batch_receiver.schema_count() == 2);
  batch_receiver.close_epoch();
  assert(batch_ledger->retained_schema_bytes() == 0);
  verify_context_schema_roundtrip();
}
} // namespace tracer::context_engine

void verify_context_schema() {
  tracer::context_engine::verify_context_schema();
}

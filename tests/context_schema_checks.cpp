#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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
// The name the schema emitter gives the message of a C++ type.
std::string ident_for_test(std::string_view text) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (unsigned char c : text) {
    hash ^= c;
    hash *= 1099511628211ULL;
  }
  std::ostringstream out;
  out << "M_" << std::hex << hash;
  return out.str();
}
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
  auto view_ast = clang::tooling::buildASTFromCodeWithArgs(R"cpp(
#include "tracer/context.hpp"
#include <string_view>
#include <vector>
struct ViewScope {};
struct DirectView : tracer::shared_state<ViewScope> { std::string_view text; };
struct Nested { std::string_view text; };
struct NestedView : tracer::shared_state<ViewScope> { Nested nested; };
struct ViewList : tracer::shared_state<ViewScope> { std::vector<std::string_view> texts; };
)cpp", arguments, "string-view-state.cc");
  if (!view_ast || view_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("string_view fixture failed to parse");
  auto &view_context = view_ast->getASTContext();
  for (const char *name : {"DirectView", "NestedView", "ViewList"}) {
    SchemaRecordFinder finder;
    finder.name = name;
    finder.TraverseDecl(view_context.getTranslationUnitDecl());
    assert(finder.record);
    StateFamilyKey view_family{family.source_lineage_id, name, "ViewScope"};
    const auto rejected = build_state_schema(view_context, *finder.record, view_family);
    if (rejected || rejected.error.find("std::string_view") == std::string::npos)
      throw std::runtime_error("expected borrowed state diagnostic: " + rejected.error);
    const auto snapshot = build_payload_schema(view_context,
        clang::QualType(finder.record->getTypeForDecl(), 0), view_family, {},
        CodecAccess::snapshot_members);
    if (!snapshot)
      throw std::runtime_error("string_view snapshot failed: " + snapshot.error);
  }
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

  // Values the wire format has no scalar for (issue #181): a payload keeps them in a
  // defined form on both codec access policies, and a retained state still refuses the
  // ones it must not hold.
  const std::string abi_source = R"cpp(
#include "tracer/context.hpp"
#include <cstddef>
#include <mutex>
#include <vector>
namespace abi_schema {
struct Scope {};
struct Key { int a; int b; int f(int); };
union Number { int i; float f; };
enum class Big : unsigned long long { small = 1, also_small = 1, large = 1ull << 40, top = ~0ull };
enum class Low : long long { low = -9223372036854775807LL - 1 };
enum class Top32 : unsigned { top = 0xffffffffu };
enum class Wide : long { a = 1 };
enum Small { one = 1 };
struct All {
  int tag;
  Number named;
  union { int i; float f; };
  union { long l; char c[8]; } unnamed;
  long double extended;
  __int128 wide;
  unsigned __int128 uwide;
  Big big;
  Low low;
  Top32 top;
  Wide w;
  Small small;
  int Key::*data;
  int (Key::*function)(int);
  std::nullptr_t null;
  volatile int counter;
  std::vector<Number> numbers;
};
struct Locked { std::mutex lock; };
struct UnionState : tracer::shared_state<Scope> { Number number; };
struct MemberPointerState : tracer::shared_state<Scope> { int Key::*data; };
struct LongDoubleState : tracer::shared_state<Scope> { long double value; };
struct WideState : tracer::shared_state<Scope> { __int128 wide; Big big; };
}
)cpp";
  auto abi_ast = clang::tooling::buildASTFromCodeWithArgs(abi_source, arguments,
                                                          "abi-schema-fixture.cc");
  if (!abi_ast || abi_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("ABI value fixture failed to parse");
  auto &abi_context = abi_ast->getASTContext();
  const auto abi_record = [&](const char *name) {
    SchemaRecordFinder finder;
    finder.name = name;
    finder.TraverseDecl(abi_context.getTranslationUnitDecl());
    if (!finder.record)
      throw std::runtime_error(std::string("ABI value fixture record missing: ") + name);
    return finder.record;
  };
  const auto abi_check = [](bool ok, const std::string &what) {
    if (!ok)
      throw std::runtime_error("ABI value schema: " + what);
  };
  const auto abi_family = [](const char *type) {
    StateFamilyKey key;
    key.source_lineage_id[0] = 0x18;
    key.cpp_state_type = type;
    key.cpp_scope_type = "abi_schema::Scope";
    return key;
  };
  const auto *all = abi_record("abi_schema::All");
  const auto all_type = clang::QualType(all->getTypeForDecl(), 0);
  for (const auto access : {CodecAccess::public_members, CodecAccess::snapshot_members}) {
    const auto built = build_payload_schema(abi_context, all_type,
                                            abi_family("abi_schema::All"), {}, access);
    abi_check(bool(built), "payload rejected: " + built.error);
    const auto &members = built.record->root->fields[0].message->fields;
    const auto member = [&](const char *name) -> const SchemaField & {
      const auto found = std::find_if(members.begin(), members.end(),
                                      [&](const auto &f) { return f.name == name; });
      abi_check(found != members.end(), std::string("field missing: ") + name);
      return *found;
    };
    const auto parts = [&](const char *name, const char *type,
                           std::initializer_list<std::pair<const char *, SchemaKind>> expected) {
      const auto &field = member(name);
      abi_check(field.kind == SchemaKind::message && field.message &&
                    field.message->cpp_type == type &&
                    field.message->fields.size() == expected.size(),
                std::string("wrong form of ") + name + ": " +
                    (field.message ? field.message->cpp_type : "not a message"));
      std::size_t index = 0;
      for (const auto &[part, kind] : expected) {
        const auto &actual = field.message->fields[index++];
        abi_check(actual.name == part && actual.kind == kind && actual.number == index,
                  std::string("wrong part of ") + name + ": " + part);
      }
    };
    const std::initializer_list<std::pair<const char *, SchemaKind>> opaque = {
        {"bytes", SchemaKind::bytes},
        {"size", SchemaKind::unsigned_integer},
        {"active_member", SchemaKind::string},
        {"alternatives", SchemaKind::string}};
    parts("named", "abi_schema::Number", opaque);
    parts("__anonymous_union_0", "abi_schema::All::(anonymous union)", opaque);
    parts("unnamed", "abi_schema::All::(unnamed union)", opaque);
    parts("extended", "long double",
          {{"format", SchemaKind::string}, {"bytes", SchemaKind::bytes}});
    parts("wide", "__int128",
          {{"low", SchemaKind::unsigned_integer}, {"high", SchemaKind::signed_integer}});
    parts("uwide", "unsigned __int128",
          {{"low", SchemaKind::unsigned_integer}, {"high", SchemaKind::unsigned_integer}});
    parts("data", "int abi_schema::Key::*",
          {{"abi", SchemaKind::string},
           {"null", SchemaKind::boolean},
           {"offset", SchemaKind::signed_integer}});
    parts("function", "int (abi_schema::Key::*)(int)",
          {{"abi", SchemaKind::string},
           {"null", SchemaKind::boolean},
           {"is_virtual", SchemaKind::boolean},
           {"address", SchemaKind::unsigned_integer},
           {"vtable_offset", SchemaKind::unsigned_integer},
           {"this_adjustment", SchemaKind::signed_integer}});
    parts("null", "std::nullptr_t", {{"null", SchemaKind::boolean}});
    abi_check(member("counter").kind == SchemaKind::signed_integer &&
                  member("counter").bit_width == 32,
              "volatile int is not a 32-bit integer");
    const auto wide_enum = [&](const char *name, const char *type, bool is_unsigned,
                               unsigned width) {
      const auto &field = member(name);
      abi_check(field.kind == SchemaKind::enumeration && field.enum_wide &&
                    field.enum_unsigned == is_unsigned && field.bit_width == width &&
                    field.enum_name == type,
                std::string("wrong wide enumeration: ") + name);
    };
    wide_enum("big", "abi_schema::Big", true, 64);
    wide_enum("low", "abi_schema::Low", false, 64);
    wide_enum("top", "abi_schema::Top32", true, 32);
    wide_enum("w", "abi_schema::Wide", false, 64);
    abi_check(!member("small").enum_wide, "a narrow enumeration became wide");
    google::protobuf::FileDescriptorSet abi_set;
    abi_check(abi_set.ParseFromString(built.record->descriptor_set), "descriptor malformed");
    google::protobuf::DescriptorPool abi_pool;
    for (const auto &f : abi_set.file())
      abi_check(abi_pool.BuildFile(f) != nullptr, "descriptor rejected by a pool");
    const auto *all_message = abi_pool.FindMessageTypeByName(
        "tracer.generated.state." + ident_for_test("abi_schema::All"));
    abi_check(all_message != nullptr, "record message missing from the descriptor");
    const auto wire = [&](const char *name) {
      for (int i = 0; i < all_message->field_count(); ++i) {
        const auto *f = all_message->field(i);
        if (f->name() == name || (f->has_json_name() && f->json_name() == name))
          return f;
      }
      throw std::runtime_error(std::string("ABI value schema: wire field missing: ") + name);
    };
    using Wire = google::protobuf::FieldDescriptor;
    abi_check(wire("big")->type() == Wire::TYPE_UINT64 &&
                  wire("low")->type() == Wire::TYPE_INT64 &&
                  wire("top")->type() == Wire::TYPE_UINT32 &&
                  wire("w")->type() == Wire::TYPE_INT64 &&
                  wire("small")->type() == Wire::TYPE_ENUM,
              "enumeration wire types");
    abi_check(wire("named")->message_type()->FindFieldByNumber(1)->type() == Wire::TYPE_BYTES,
              "union bytes are not a bytes field");
    std::vector<std::string> enumerators;
    std::string enum_type;
    const auto &options = wire("big")->options().unknown_fields();
    for (int i = 0; i < options.field_count(); ++i) {
      if (options.field(i).number() == kEnumTypeOption)
        enum_type = options.field(i).length_delimited();
      if (options.field(i).number() == kEnumeratorOption)
        enumerators.push_back(options.field(i).length_delimited());
    }
    abi_check(enum_type == "abi_schema::Big" &&
                  enumerators == std::vector<std::string>{"small=1", "also_small=1",
                                                          "large=1099511627776",
                                                          "top=18446744073709551615"},
              "wide enumeration metadata");
    abi_check(wire("small")->options().unknown_fields().empty(),
              "a narrow enumeration carries wide metadata");
    const auto codec = render_payload_codec_source(abi_context, all_type, access);
    abi_check(bool(codec), "codec generation failed: " + codec.error);
    abi_check(codec.source.find("UnionAlternatives<::abi_schema::Number>") != std::string::npos &&
                  codec.source.find("text = \"int i; float f\";") != std::string::npos &&
                  codec.source.find(", 8, \"long l; char[8] c\", budget)") != std::string::npos &&
                  codec.source.find("encode_union(") != std::string::npos,
              "codec does not encode the unions as their bytes");
    auto abi_codec_ast = clang::tooling::buildASTFromCodeWithArgs(
        "#include \"context_codec.hpp\"\n" + abi_source + codec.source, arguments,
        "abi-codec-fixture.cc");
    abi_check(abi_codec_ast && !abi_codec_ast->getDiagnostics().hasErrorOccurred(),
              "generated codec did not compile");
    // A class of the standard library without a codec is refused where the schema is
    // built, not when the generated code is compiled.
    const auto locked = build_payload_schema(
        abi_context, clang::QualType(abi_record("abi_schema::Locked")->getTypeForDecl(), 0),
        abi_family("abi_schema::Locked"), {}, access);
    abi_check(!locked &&
                  locked.error.find("unsupported standard type: std::mutex") != std::string::npos,
              "std::mutex member: " + locked.error);
  }
  const std::pair<const char *, const char *> refused_states[] = {
      {"abi_schema::UnionState", "union state layout is unsupported"},
      {"abi_schema::MemberPointerState", "unsupported member-pointer field: data"},
      {"abi_schema::LongDoubleState", "unsupported field type 'long double': value"}};
  for (const auto &[name, reason] : refused_states) {
    const auto rejected = build_state_schema(abi_context, *abi_record(name), abi_family(name));
    abi_check(!rejected && rejected.error.find(reason) != std::string::npos,
              std::string("state expected '") + reason + "', got '" + rejected.error + "'");
  }
  // A state keeps a 128-bit integer and a wide enumeration whole too.
  const auto wide_state = build_state_schema(abi_context, *abi_record("abi_schema::WideState"),
                                             abi_family("abi_schema::WideState"));
  abi_check(bool(wide_state), "wide state rejected: " + wide_state.error);
  abi_check(wide_state.record->root->fields[0].message &&
                wide_state.record->root->fields[0].message->cpp_type == "__int128" &&
                wide_state.record->root->fields[1].enum_wide,
            "wide state lost its exact forms");

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

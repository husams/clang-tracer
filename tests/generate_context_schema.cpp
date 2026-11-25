#include "context_schema.hpp"
#include "context_test_config.hpp"

#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>

namespace {
struct Fixture {
  const char *name;
  const char *path;
  const char *state;
  const char *scope;
  unsigned char lineage;
  bool emit_codec;
};
struct Found final : clang::RecursiveASTVisitor<Found> {
  std::string wanted;
  const clang::CXXRecordDecl *record = nullptr;
  bool VisitCXXRecordDecl(clang::CXXRecordDecl *r) {
    if (r->isThisDeclarationADefinition() &&
        r->getQualifiedNameAsString() == wanted)
      record = r;
    return true;
  }
};
struct FoundAlias final : clang::RecursiveASTVisitor<FoundAlias> {
  std::string wanted;
  clang::TypedefNameDecl *alias = nullptr;
  bool VisitTypedefNameDecl(clang::TypedefNameDecl *d) {
    if (d->getNameAsString() == wanted)
      alias = d;
    return true;
  }
};
std::string canonical(clang::QualType t) {
  t = t.getNonReferenceType().getCanonicalType();
  if (const auto *r = t->getAsCXXRecordDecl()) {
    if (llvm::isa<clang::ClassTemplateSpecializationDecl>(r))
      return t.getAsString();
    return r->getQualifiedNameAsString();
  }
  return t.getAsString();
}
std::string read(const std::string &path) {
  std::ifstream in(path);
  if (!in)
    throw std::runtime_error("cannot read fixture: " + path);
  return {std::istreambuf_iterator<char>(in), {}};
}
std::unique_ptr<clang::ASTUnit> parse(const std::string &source) {
  std::vector<std::string> args{"-std=c++20",
                                "-I" TRACER_TEST_SOURCE_DIR "/include",
                                "-I" TRACER_TEST_SOURCE_DIR "/src",
                                "-resource-dir=" TRACER_TEST_RESOURCE_DIR};
  args.insert(args.end(), std::begin(kContextSystemArguments),
              std::end(kContextSystemArguments));
  auto ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n" + source, args,
      "context-schema-generated.cc");
  if (!ast || ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("schema fixture failed to parse");
  return ast;
}
void emit_bytes(std::ostream &out, const char *name, const std::string &bytes) {
  out << "inline constexpr unsigned char " << name << "[] = {";
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    if (i % 16 == 0)
      out << "\n  ";
    out << "0x" << std::hex
        << static_cast<unsigned>(static_cast<unsigned char>(bytes[i])) << ",";
  }
  out << std::dec << "\n};\n";
}
void emit_schema(std::ostream &out, const char *prefix,
                 const tracer::context_engine::SchemaRecord &s) {
  emit_bytes(out, (std::string(prefix) + "_descriptor").c_str(),
             s.descriptor_set);
  out << "inline constexpr char " << prefix << "_revision[] = \""
      << s.revision_id << "\";\n";
  out << "inline constexpr char " << prefix << "_root[] = \"" << s.root_type
      << "\";\n";
  out << "inline constexpr char " << prefix << "_state[] = \""
      << s.family.cpp_state_type << "\";\n";
  out << "inline constexpr char " << prefix << "_scope[] = \""
      << s.family.cpp_scope_type << "\";\n";
  out << "inline constexpr unsigned char " << prefix << "_lineage[16] = {";
  for (auto byte : s.family.source_lineage_id)
    out << static_cast<unsigned>(byte) << ",";
  out << "};\n";
  out << "inline constexpr std::size_t " << prefix
      << "_role_count = " << s.container_roles.size()
      << ";\ninline constexpr const char* " << prefix << "_roles[] = {";
  if (s.container_roles.empty())
    out << "nullptr,";
  else
    for (const auto &role : s.container_roles)
      out << '"' << role << "\",";
  out << "};\n";
}
// The descriptor message that names a C++ type (context_schema.cpp `ident`).
std::string message_name(const std::string &cpp_type) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (unsigned char byte : cpp_type) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  std::ostringstream out;
  out << "tracer.generated.state.M_" << std::hex << hash;
  return out.str();
}

// Value snapshots of records that contain themselves: the schema of each root
// as the capture code generator builds it, and one codec for all of them.
void emit_recursive_snapshots(const char *path) {
  using namespace tracer::context_engine;
  auto ast = parse(read(std::string(TRACER_TEST_SOURCE_DIR) +
                        "/tests/fixtures/snapshot_recursive.hpp"));
  std::ofstream out(path);
  if (!out)
    throw std::runtime_error("cannot open recursive snapshot header output");
  out << "#pragma once\n#include <cstddef>\n#include \"context_codec.hpp\"\n"
         "namespace snapshot_recursive_generated {\n"
         "struct Schema {\n  const char *cpp_type, *root;\n"
         "  const unsigned char *descriptor;\n  std::size_t descriptor_size;\n"
         "  const char *const *roles;\n  std::size_t role_count;\n"
         "  const char *const *types;\n  std::size_t type_count;\n};\n";
  std::vector<clang::QualType> roots;
  for (const char *name :
       {"TreeNode", "ListNode", "SharedNode", "Folder", "Hidden", "Grid", "Holder"}) {
    Found finder;
    finder.wanted = std::string("recursive_fixture::") + name;
    finder.TraverseDecl(ast->getASTContext().getTranslationUnitDecl());
    if (!finder.record)
      throw std::runtime_error(std::string("recursive fixture record not found: ") + name);
    const clang::QualType type(finder.record->getTypeForDecl(), 0);
    roots.push_back(type);
    StateFamilyKey family;
    family.source_lineage_id[0] = 1;
    family.cpp_state_type = finder.wanted;
    family.cpp_scope_type = "tracer::EventValueSnapshot";
    // The limits of a value snapshot (value_snapshot_codegen.cpp).
    SchemaLimits limits;
    limits.descriptor_bytes = 4096;
    limits.fields = 128;
    limits.depth = 16;
    const auto schema =
        build_payload_schema(ast->getASTContext(), type, std::move(family), limits,
                             CodecAccess::snapshot_members, {}, SchemaRecursion::allow);
    if (!schema)
      throw std::runtime_error(std::string("recursive snapshot schema failed for ") + name +
                               ": " + schema.error);
    const auto &record = *schema.record;
    std::set<std::string> types;
    std::function<void(const SchemaField &)> field;
    std::function<void(const SchemaMessage &)> message;
    message = [&](const SchemaMessage &value) {
      types.insert(message_name(value.cpp_type) + "=" + value.cpp_type);
      for (const auto &item : value.fields)
        field(item);
    };
    field = [&](const SchemaField &value) {
      if (value.message)
        message(*value.message);
      if (value.value)
        field(*value.value);
      if (value.key)
        field(*value.key);
    };
    message(*record.root);
    emit_bytes(out, (std::string(name) + "_descriptor").c_str(), record.descriptor_set);
    out << "inline constexpr const char* " << name << "_roles[] = {";
    for (const auto &role : record.container_roles)
      out << '"' << role << "\",";
    out << "nullptr};\ninline constexpr const char* " << name << "_types[] = {";
    for (const auto &type_name : types)
      out << '"' << type_name << "\",";
    out << "nullptr};\ninline constexpr Schema " << name << "{\"" << finder.wanted << "\", \""
        << record.root_type << "\", " << name << "_descriptor, sizeof(" << name
        << "_descriptor), " << name << "_roles, " << record.container_roles.size() << ", "
        << name << "_types, " << types.size() << "};\n";
  }
  out << "}\n";
  const auto codec = render_context_codec_sources(ast->getASTContext(), {}, roots,
                                                  CodecAccess::snapshot_members, {},
                                                  SchemaRecursion::allow);
  if (!codec)
    throw std::runtime_error("recursive snapshot codec generation failed: " + codec.error);
  out << codec.source << "\n";
  if (!out)
    throw std::runtime_error("cannot write recursive snapshot header");
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 2 && argc != 3)
      throw std::runtime_error("usage: generate-context-schema OUTPUT [RECURSIVE_SNAPSHOT_OUTPUT]");
    if (argc == 3)
      emit_recursive_snapshots(argv[2]);
    const Fixture fixtures[] = {
        {"demo", "tests/fixtures/shared_context_nested.cpp",
         "demo::RequestState", "demo::RequestScope", 0x11, true},
        {"catalog", "tests/fixtures/context_schema_catalog.cpp",
         "catalog::CatalogState", "catalog::Scope", 0x22, true},
        {"history_v1", "tests/fixtures/context_schema_history_v1.cpp",
         "history::State", "history::Scope", 0x33, true},
        {"history_v2", "tests/fixtures/context_schema_history_v2.cpp",
         "history::State", "history::Scope", 0x33, false},
        {"query", "tests/fixtures/context_schema_query.cpp",
         "query_fixture::State", "query_fixture::Scope", 0x55, true},
    };
    std::ofstream out(argv[1]);
    if (!out)
      throw std::runtime_error("cannot open generated header output");
    out << "#pragma once\n#include <cstddef>\n#include <cstdint>\n#include "
           "\"context_codec.hpp\"\nnamespace context_schema_generated {\n";
    std::vector<std::string> codecs;
    for (const auto &fixture : fixtures) {
      const auto source =
          read(std::string(TRACER_TEST_SOURCE_DIR) + "/" + fixture.path);
      auto ast = parse(source);
      Found finder;
      finder.wanted = fixture.state;
      finder.TraverseDecl(ast->getASTContext().getTranslationUnitDecl());
      if (!finder.record)
        throw std::runtime_error(std::string("state declaration not found: ") +
                                 fixture.state);
      tracer::context_engine::StateFamilyKey family;
      family.source_lineage_id[0] = fixture.lineage;
      family.cpp_state_type = fixture.state;
      family.cpp_scope_type = fixture.scope;
      auto schema = tracer::context_engine::build_state_schema(
          ast->getASTContext(), *finder.record, std::move(family));
      if (!schema)
        throw std::runtime_error(std::string("schema generation failed: ") +
                                 schema.error);
      emit_schema(out, fixture.name, *schema.record);
      if (fixture.emit_codec) {
        auto codec =
            tracer::context_engine::render_state_codec_source(*finder.record);
        if (!codec)
          throw std::runtime_error(std::string("codec generation failed: ") +
                                   codec.error);
        codecs.push_back(std::move(codec.source));
      }
    }
    auto generic_ast =
        parse(read(std::string(TRACER_TEST_SOURCE_DIR) +
                   "/tests/fixtures/context_schema_generic.cpp"));
    const auto emit_payload = [&](const char *prefix, clang::QualType payload,
                                  unsigned char lineage) {
      tracer::context_engine::StateFamilyKey family;
      family.source_lineage_id[0] = lineage;
      family.cpp_state_type = canonical(payload);
      family.cpp_scope_type = "generic_payload::Scope";
      auto schema = tracer::context_engine::build_payload_schema(
          generic_ast->getASTContext(), payload, std::move(family));
      if (!schema)
        throw std::runtime_error(
            std::string("generic payload schema generation failed: ") +
            schema.error);
      emit_schema(out, prefix, *schema.record);
      auto codec = tracer::context_engine::render_payload_codec_source(
          generic_ast->getASTContext(), payload);
      if (!codec)
        throw std::runtime_error(
            std::string("generic payload codec generation failed: ") +
            codec.error);
      codecs.push_back(std::move(codec.source));
    };
    emit_payload("generic_int", generic_ast->getASTContext().IntTy, 0x44);
    FoundAlias text_alias;
    text_alias.wanted = "Text";
    text_alias.TraverseDecl(
        generic_ast->getASTContext().getTranslationUnitDecl());
    if (!text_alias.alias)
      throw std::runtime_error("generic string alias missing");
    emit_payload("generic_text", text_alias.alias->getUnderlyingType(), 0x45);
    FoundAlias values_alias;
    values_alias.wanted = "Values";
    values_alias.TraverseDecl(
        generic_ast->getASTContext().getTranslationUnitDecl());
    if (!values_alias.alias)
      throw std::runtime_error("generic vector alias missing");
    emit_payload("generic_values", values_alias.alias->getUnderlyingType(),
                 0x46);
    FoundAlias object_alias;
    object_alias.wanted = "Object";
    object_alias.TraverseDecl(
        generic_ast->getASTContext().getTranslationUnitDecl());
    if (!object_alias.alias)
      throw std::runtime_error("generic object alias missing");
    emit_payload("generic_object", object_alias.alias->getUnderlyingType(),
                 0x47);
    out << "}\n";
    for (const auto &codec : codecs)
      out << codec << "\n";
    return out ? 0 : 1;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}

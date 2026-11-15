#include "cpp_resolver.hpp"
#include "header_context.hpp"
#include "prepared_types.hpp"
#include "prepared_adapter.hpp"
#include "prepared_contract.hpp"
#include "prepared_header_search.hpp"
#include "tracer_runtime_config.hpp"

#include "../include/tracer/receiver_abi.hpp"
#include "tracer/compat.hpp"
#include "tracer/compiler_options.hpp"

#ifndef TRACER_CLANG_RESOURCE_DIR
#error                                                                                             \
    "clang-tracer must be built with TRACER_CLANG_RESOURCE_DIR set to its Clang resource directory"
#endif

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Attr.h>
#include <clang/AST/CXXInheritance.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Expr.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/ParentMapContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/Basic/Version.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/SmallString.h>
#if __has_include(<llvm/TargetParser/Triple.h>)
#include <llvm/TargetParser/Triple.h>
#else
#include <llvm/ADT/Triple.h>
#endif
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>

namespace tracer::client {
namespace {

namespace fs = std::filesystem;
namespace api = clangtracer::v1;

constexpr std::size_t kMaxTranslationUnits = 256;
constexpr std::size_t kMaxExplicitSources = 128;
constexpr std::size_t kMaxArguments = 8;
constexpr std::size_t kMaxReceiverFields = 128;
constexpr std::size_t kMaxReceiverSourcePath = 4096;
constexpr std::size_t kMaxReceiverArguments = 512;
constexpr std::size_t kMaxReceiverArgumentBytes = 65536;

std::string trim(std::string value) {
  const auto first =
      std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c); });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
                      return std::isspace(c);
                    }).base();
  return first < last ? std::string(first, last) : std::string();
}

std::string compact(std::string_view value) {
  std::string result;
  for (std::size_t i = 0; i < value.size(); ++i) {
    const auto c = static_cast<unsigned char>(value[i]);
    if (!std::isspace(c)) {
      result.push_back(static_cast<char>(c));
      continue;
    }
    std::size_t next = i + 1;
    while (next < value.size() && std::isspace(static_cast<unsigned char>(value[next]))) {
      ++next;
    }
    const auto is_word = [](unsigned char ch) { return std::isalnum(ch) || ch == '_'; };
    if (!result.empty() && next < value.size() &&
        is_word(static_cast<unsigned char>(result.back())) &&
        is_word(static_cast<unsigned char>(value[next]))) {
      result.push_back(' ');
    }
    i = next - 1;
  }
  return result;
}

class TypeAliasFinder final
    : public clang::RecursiveASTVisitor<TypeAliasFinder> {
public:
  explicit TypeAliasFinder(std::string qualified_name)
      : qualified_name_(std::move(qualified_name)) {}

  bool VisitTypeAliasDecl(clang::TypeAliasDecl *alias) {
    if (alias->getQualifiedNameAsString() == qualified_name_)
      result_ = alias;
    return true;
  }

  const clang::TypeAliasDecl *result() const { return result_; }

private:
  std::string qualified_name_;
  const clang::TypeAliasDecl *result_ = nullptr;
};

const clang::CXXRecordDecl *record_alias(clang::ASTContext &ast,
                                         std::string qualified_name) {
  TypeAliasFinder finder(std::move(qualified_name));
  finder.TraverseDecl(ast.getTranslationUnitDecl());
  const auto *alias = finder.result();
  const auto *record = alias ? alias->getUnderlyingType()->getAsCXXRecordDecl() : nullptr;
  return record ? record->getDefinition() : nullptr;
}

void validate_snapshot_environment(clang::ASTContext &ast) {
  if (!ast.getLangOpts().RTTI)
    throw std::invalid_argument("snapshot return type requires RTTI-enabled source flags");
  const auto *vtable = llvm::dyn_cast<clang::ItaniumVTableContext>(
      ast.getVTableContext());
  if (!vtable || vtable->isRelativeLayout())
    throw std::invalid_argument(
        "snapshot return type requires pointer-sized Itanium vtable entries");
}

void validate_snapshot_candidate(const clang::CXXRecordDecl *snapshot_record) {
  if (!snapshot_record || !snapshot_record->isCompleteDefinition() ||
      snapshot_record->isUnion() || snapshot_record->isDependentType() ||
      snapshot_record->isAbstract() || !snapshot_record->getIdentifier() ||
      snapshot_record->getQualifiedNameAsString().empty() ||
      snapshot_record->getFormalLinkage() != clang::Linkage::External)
    throw std::invalid_argument(
        "snapshot return type must name a complete, concrete, externally visible record");
  for (auto *record = snapshot_record; record;
       record = llvm::dyn_cast<clang::CXXRecordDecl>(record->getDeclContext())) {
    const auto access = record->getAccess();
    if (access != clang::AS_none && access != clang::AS_public)
      throw std::invalid_argument("snapshot return type must be publicly nameable");
  }
}

class RecordDefinitions final : public clang::RecursiveASTVisitor<RecordDefinitions> {
public:
  bool VisitCXXRecordDecl(clang::CXXRecordDecl *record) {
    if (record->isCompleteDefinition() && !record->isDependentType())
      records.push_back(record->getCanonicalDecl()->getDefinition());
    return true;
  }
  std::vector<const clang::CXXRecordDecl *> records;
};

struct SnapshotRoot {
  const clang::CXXRecordDecl *record = nullptr;
  std::vector<const clang::CXXRecordDecl *> dynamic_types;
  struct Member {
    std::string field;
    std::vector<const clang::CXXRecordDecl *> dynamic_types;
    bool automatic = false;
    bool erased_any = false;
  };
  std::vector<Member> members;
};

struct PreparedSnapshots {
  std::vector<const clang::CXXRecordDecl *> types;
  std::vector<api::PreparedSnapshotRecord> records;
};

const clang::CXXRecordDecl *snapshot_record_type(clang::QualType type) {
  type = type.getNonReferenceType();
  if (type->isPointerType())
    type = type->getPointeeType();
  const auto *record = type->getAsCXXRecordDecl();
  return record ? record->getDefinition() : nullptr;
}

std::vector<const clang::CXXRecordDecl *> discover_dynamic_types(
    clang::ASTContext &ast, const clang::CXXRecordDecl *base,
    const std::vector<const clang::CXXRecordDecl *> &definitions,
    bool include_base) {
  if (!base || !base->isPolymorphic())
    return {};
  try {
    validate_snapshot_environment(ast);
  } catch (const std::invalid_argument &) {
    return {};
  }
  std::map<std::string, const clang::CXXRecordDecl *> eligible;
  std::set<const clang::CXXRecordDecl *> seen;
  for (const auto *record : definitions) {
    if (!record || !seen.insert(record->getCanonicalDecl()).second)
      continue;
    if (!include_base && record->getCanonicalDecl() == base->getCanonicalDecl())
      continue;
    clang::CXXBasePaths paths;
    if (record != base &&
        (!record->isDerivedFrom(base, paths) ||
         paths.isAmbiguous(ast.getCanonicalType(tracer::prepared::record_type(ast, base))) ||
         std::none_of(paths.begin(), paths.end(), [](const auto &path) {
           return path.Access == clang::AS_public;
         })))
      continue;
    try {
      validate_snapshot_candidate(record);
    } catch (const std::invalid_argument &) {
      continue;
    }
    const auto identity = tracer::prepared::type_name(ast, tracer::prepared::record_type(ast, record));
    if (!eligible.emplace(identity, record).second)
      throw std::invalid_argument("ambiguous snapshot type identity: " + identity);
  }
  std::vector<const clang::CXXRecordDecl *> result;
  result.reserve(eligible.size());
  for (const auto &candidate : eligible)
    result.push_back(candidate.second);
  return result;
}

class SnapshotPolicyFinder final
    : public clang::RecursiveASTVisitor<SnapshotPolicyFinder> {
public:
  bool VisitClassTemplateSpecializationDecl(
      clang::ClassTemplateSpecializationDecl *specialization) {
    if (!specialization->isThisDeclarationADefinition() ||
        !visited.insert(specialization->getCanonicalDecl()).second)
      return true;
    const auto *templ = specialization->getSpecializedTemplate();
    if (!templ)
      return true;
    const auto name = templ->getQualifiedNameAsString();
    if (name != "tracer_snapshot::FollowPointers" && name != "tracer_snapshot::AnyPayloads")
      return true;
    const auto &args = specialization->getTemplateArgs();
    if (args.size() != 1 || args[0].getKind() != clang::TemplateArgument::Type)
      throw std::invalid_argument(name + " specialization requires one record type");
    const auto *record = args[0].getAsType()->getAsCXXRecordDecl();
    record = record ? record->getDefinition() : nullptr;
    if (!record)
      throw std::invalid_argument(name + " policy record must be complete in prepare inputs");
    if (name == "tracer_snapshot::AnyPayloads") {
      if (!specialization->getASTContext().getLangOpts().RTTI)
        throw std::invalid_argument("AnyPayloads requires RTTI-enabled source flags");
      auto &members = any_policies[record->getCanonicalDecl()];
      for (const auto *declaration : specialization->decls()) {
        const auto *alias = llvm::dyn_cast<clang::TypeAliasDecl>(declaration);
        if (!alias)
          continue;
        const auto *tuple = llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
            alias->getUnderlyingType()->getAsCXXRecordDecl());
        if (!tuple || tuple->getSpecializedTemplate()->getQualifiedNameAsString() != "std::tuple" ||
            tuple->getTemplateArgs().size() != 1 ||
            tuple->getTemplateArgs()[0].getKind() != clang::TemplateArgument::Pack)
          throw std::invalid_argument("AnyPayloads member aliases require std::tuple of record types");
        SnapshotRoot::Member member;
        member.field = alias->getNameAsString();
        member.erased_any = true;
        const auto fields = record->fields();
        const auto field = std::find_if(fields.begin(), fields.end(), [&](const auto *candidate) {
          return candidate->getNameAsString() == member.field;
        });
        const auto *erased = field == fields.end() ? nullptr : (*field)->getType()->getAsCXXRecordDecl();
        if (!erased || erased->getQualifiedNameAsString() != "std::any" ||
            (*field)->getType().isVolatileQualified())
          throw std::invalid_argument("AnyPayloads field must be a direct std::any member");
        std::set<const clang::CXXRecordDecl *> seen;
        for (const auto &argument : tuple->getTemplateArgs()[0].pack_elements()) {
          const auto *payload = argument.getAsType()->getAsCXXRecordDecl();
          payload = payload ? payload->getDefinition() : nullptr;
          validate_snapshot_candidate(payload);
          if (!seen.insert(payload->getCanonicalDecl()).second)
            throw std::invalid_argument("AnyPayloads contains a duplicate payload type");
          member.dynamic_types.push_back(payload);
        }
        if (member.dynamic_types.empty())
          throw std::invalid_argument("AnyPayloads requires at least one payload type per member");
        members.push_back(std::move(member));
      }
      if (members.empty())
        throw std::invalid_argument("AnyPayloads must name at least one std::any member");
      return true;
    }
    const clang::VarDecl *fields = nullptr;
    if (const auto *definition = specialization->getDefinition())
      for (const auto *decl : definition->decls())
        if (const auto *variable = llvm::dyn_cast<clang::VarDecl>(decl);
            variable && variable->getName() == "fields") {
          if (fields)
            throw std::invalid_argument("FollowPointers policy has multiple fields arrays");
          fields = variable;
        }
    if (!fields || !fields->isStaticDataMember() || !fields->isConstexpr() ||
        !fields->hasInit())
      throw std::invalid_argument(
          "FollowPointers policy must define static constexpr const char* fields[]");
    const auto *array_type = llvm::dyn_cast<clang::ConstantArrayType>(
        fields->getType().getTypePtr());
    const auto element_type = array_type
        ? array_type->getElementType().getUnqualifiedType() : clang::QualType{};
    if (element_type.isNull() || !element_type->isPointerType() ||
        !element_type->getPointeeType()->isCharType() ||
        !element_type->getPointeeType().isConstQualified())
      throw std::invalid_argument(
          "FollowPointers fields must have type static constexpr const char*[]");
    const auto *initializer = llvm::dyn_cast<clang::InitListExpr>(
        fields->getInit()->IgnoreParenImpCasts());
    if (!initializer)
      throw std::invalid_argument("FollowPointers fields must be a literal initializer list");
    std::vector<std::string> names;
    for (const auto *element : initializer->inits()) {
      const auto *literal = llvm::dyn_cast<clang::StringLiteral>(
          element->IgnoreParenImpCasts());
      if (!literal || literal->getString().empty())
        throw std::invalid_argument("FollowPointers fields must contain nonempty string literals");
      const auto name = literal->getString().str();
      if (!(std::isalpha(static_cast<unsigned char>(name.front())) || name.front() == '_') ||
          !std::all_of(name.begin() + 1, name.end(), [](unsigned char c) {
            return std::isalnum(c) || c == '_';
          }))
        throw std::invalid_argument("FollowPointers entry is not a field identifier: " + name);
      if (std::find(names.begin(), names.end(), name) != names.end())
        throw std::invalid_argument("FollowPointers field is duplicated: " + name);
      names.push_back(name);
    }
    if (names.empty())
      throw std::invalid_argument("FollowPointers fields array cannot be empty");
    const auto key = record->getCanonicalDecl();
    if (!policies.emplace(key, std::move(names)).second)
      throw std::invalid_argument("duplicate FollowPointers specialization for " +
                                  record->getQualifiedNameAsString());
    return true;
  }
  std::map<const clang::CXXRecordDecl *, std::vector<std::string>> policies;
  std::map<const clang::CXXRecordDecl *, std::vector<SnapshotRoot::Member>> any_policies;
  std::set<const clang::CXXRecordDecl *> visited;
};

std::vector<const clang::CXXRecordDecl *> discover_member_types(
    clang::ASTContext &ast, const clang::CXXRecordDecl *base,
    const std::vector<const clang::CXXRecordDecl *> &definitions) {
  if (!base || !base->isCompleteDefinition())
    throw std::invalid_argument("FollowPointers member pointee must be complete in prepare inputs");
  if (!base->isPolymorphic()) {
    validate_snapshot_candidate(base);
    return {base};
  }
  return discover_dynamic_types(ast, base, definitions, true);
}

PreparedSnapshots prepare_snapshot_roots(
    clang::ASTContext &ast, const clang::FunctionDecl &function,
    const clang::CXXMethodDecl *method) {
  std::vector<const clang::CXXRecordDecl *> roots;
  const auto add = [&](clang::QualType type) {
    if (const auto *record = snapshot_record_type(type))
      roots.push_back(record);
  };
  if (method && !method->isStatic())
    roots.push_back(method->getParent()->getDefinition());
  for (const auto *parameter : function.parameters())
    add(parameter->getType());
  add(function.getReturnType());
  const auto result_type = function.getReturnType();
  const bool result_root = result_type->isPointerType() || result_type->isReferenceType();
  const auto *result_record = result_root ? snapshot_record_type(result_type) : nullptr;
  const auto *dynamic_result_root = function.getReturnType()->isPointerType()
      ? snapshot_record_type(function.getReturnType()) : nullptr;
  SnapshotPolicyFinder policies;
  policies.TraverseDecl(ast.getTranslationUnitDecl());
  RecordDefinitions record_finder;
  record_finder.TraverseDecl(ast.getTranslationUnitDecl());
  const auto &definitions = record_finder.records;
  std::sort(roots.begin(), roots.end(), [&](const auto *left, const auto *right) {
    return tracer::prepared::type_name(ast, tracer::prepared::record_type(ast, left)) <
           tracer::prepared::type_name(ast, tracer::prepared::record_type(ast, right));
  });
  roots.erase(std::unique(roots.begin(), roots.end(), [](const auto *left, const auto *right) {
    return left->getCanonicalDecl() == right->getCanonicalDecl();
  }), roots.end());

  std::set<const clang::CXXRecordDecl *> roots_seen;
  std::vector<const clang::CXXRecordDecl *> pending = roots;
  std::vector<SnapshotRoot> root_policies;
  while (!pending.empty()) {
    const auto *root = pending.back();
    pending.pop_back();
    if (!root || !roots_seen.insert(root->getCanonicalDecl()).second)
      continue;
    SnapshotRoot policy;
    policy.record = root;
    if (dynamic_result_root &&
        root->getCanonicalDecl() == dynamic_result_root->getCanonicalDecl())
      policy.dynamic_types = discover_dynamic_types(ast, root, definitions, false);
    const auto policy_fields = policies.policies.find(root->getCanonicalDecl());
    if (policy_fields != policies.policies.end()) {
      for (const auto &field_name : policy_fields->second) {
        const clang::FieldDecl *field = nullptr;
        for (const auto *candidate : root->fields())
          if (candidate->getNameAsString() == field_name) {
            field = candidate;
            break;
          }
        if (!field)
          throw std::invalid_argument("FollowPointers field is not a direct member of " +
              root->getQualifiedNameAsString() + ": " + field_name);
        const auto field_type = field->getType();
        if (field_type.isVolatileQualified() || !field_type->isPointerType() ||
            field_type->getPointeeType().isVolatileQualified())
          throw std::invalid_argument("FollowPointers field must be a non-volatile raw record pointer: " +
                                      field_name);
        const auto *pointee = field_type->getPointeeType()->getAsCXXRecordDecl();
        pointee = pointee ? pointee->getDefinition() : nullptr;
        if (!pointee)
          throw std::invalid_argument("FollowPointers pointee is incomplete in prepare inputs: " +
                                      field_name);
        auto member_types = discover_member_types(ast, pointee, definitions);
        if (pointee->isPolymorphic() && member_types.empty())
          throw std::invalid_argument(
              "FollowPointers polymorphic pointee has no eligible concrete type in prepare inputs: " +
              field_name);
        policy.members.push_back({field_name, std::move(member_types), false});
      }
    } else if (!policies.any_policies.count(root->getCanonicalDecl()) && result_record &&
               result_record->getCanonicalDecl() == root->getCanonicalDecl()) {
      for (const auto *field : root->fields()) {
        const auto field_type = field->getType();
        if (field_type.isVolatileQualified() || !field_type->isPointerType() ||
            field_type->getPointeeType().isVolatileQualified())
          continue;
        const auto *pointee = field_type->getPointeeType()->getAsCXXRecordDecl();
        pointee = pointee ? pointee->getDefinition() : nullptr;
        if (!pointee || !pointee->isPolymorphic())
          continue;
        auto member_types = discover_dynamic_types(ast, pointee, definitions, false);
        if (!member_types.empty())
          policy.members.push_back(
              {field->getNameAsString(), std::move(member_types), true});
      }
    }
    if (const auto payloads = policies.any_policies.find(root->getCanonicalDecl());
        payloads != policies.any_policies.end())
      policy.members.insert(policy.members.end(), payloads->second.begin(), payloads->second.end());
    for (const auto *dynamic_type : policy.dynamic_types) {
      if (!roots_seen.count(dynamic_type->getCanonicalDecl()))
        pending.push_back(dynamic_type);
    }
    if (!policy.dynamic_types.empty() || !policy.members.empty())
      root_policies.push_back(std::move(policy));
  }
  std::vector<const clang::CXXRecordDecl *> policy_types;
  for (const auto &root : root_policies) {
    policy_types.push_back(root.record);
    policy_types.insert(policy_types.end(), root.dynamic_types.begin(),
                        root.dynamic_types.end());
    for (const auto &member : root.members)
      policy_types.insert(policy_types.end(), member.dynamic_types.begin(),
                          member.dynamic_types.end());
  }
  std::sort(policy_types.begin(), policy_types.end(), [&](const auto *left, const auto *right) {
    return tracer::prepared::type_name(ast, tracer::prepared::record_type(ast, left)) <
           tracer::prepared::type_name(ast, tracer::prepared::record_type(ast, right));
  });
  std::vector<const clang::CXXRecordDecl *> types;
  std::map<std::string, const clang::CXXRecordDecl *> identities;
  for (const auto *record : policy_types) {
    if (!types.empty() &&
        types.back()->getCanonicalDecl() == record->getCanonicalDecl())
      continue;
    const auto identity = tracer::prepared::type_name(ast, tracer::prepared::record_type(ast, record));
    if (!identities.emplace(identity, record->getCanonicalDecl()).second)
      throw std::invalid_argument("ambiguous snapshot type identity: " + identity);
    types.push_back(record);
  }
  const auto index_of = [&](const clang::CXXRecordDecl *record) -> std::uint32_t {
    const auto found = std::find_if(types.begin(), types.end(), [&](const auto *candidate) {
      return candidate->getCanonicalDecl() == record->getCanonicalDecl();
    });
    if (found == types.end())
      throw std::logic_error("prepared snapshot type registry is incomplete");
    return static_cast<std::uint32_t>(std::distance(types.begin(), found));
  };
  PreparedSnapshots prepared;
  std::sort(root_policies.begin(), root_policies.end(), [&](const auto &left, const auto &right) {
    return tracer::prepared::type_name(ast, tracer::prepared::record_type(ast, left.record)) <
           tracer::prepared::type_name(ast, tracer::prepared::record_type(ast, right.record));
  });
  for (const auto &root : root_policies) {
    api::PreparedSnapshotRecord record;
    record.set_type_index(index_of(root.record));
    for (const auto *dynamic_type : root.dynamic_types)
      record.add_dynamic_types(index_of(dynamic_type));
    for (const auto &member : root.members) {
      auto &prepared_member = member.automatic
          ? *record.add_automatic_members() : *record.add_members();
      prepared_member.set_field(member.field);
      prepared_member.set_erased_any(member.erased_any);
      for (const auto *dynamic_type : member.dynamic_types)
        prepared_member.add_dynamic_types(index_of(dynamic_type));
    }
    prepared.records.push_back(std::move(record));
  }
  prepared.types = std::move(types);
  return prepared;
}

std::string storage_name(const clang::NamedDecl &decl) {
  std::string name = decl.getNameAsString();
  for (const auto *parent = decl.getDeclContext(); parent && !parent->isTranslationUnit();
       parent = parent->getParent()) {
    if (const auto *scope = llvm::dyn_cast<clang::NamespaceDecl>(parent))
      name = scope->getNameAsString() + "::" + name;
    else if (const auto *record = llvm::dyn_cast<clang::CXXRecordDecl>(parent))
      name = record->getNameAsString() + "::" + name;
  }
  return name;
}

std::string storage_type(clang::ASTContext &ast, clang::QualType type, unsigned depth = 0) {
  if (type.isNull() || depth > 12)
    return "?";
  type = type.getCanonicalType().getUnqualifiedType();
  if (type->isPointerType() || type->isReferenceType()) {
    const auto kind = type->isPointerType() ? "*" : type->isLValueReferenceType() ? "&" : "&&";
    return kind + storage_type(ast, type->getPointeeType(), depth + 1);
  }
  if (const auto *record = type->getAsCXXRecordDecl()) {
    if (const auto *special = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record)) {
      std::string name = storage_name(*special->getSpecializedTemplate());
      const auto append = [&](auto &&self, const clang::TemplateArgument &arg) -> bool {
        if (arg.getKind() == clang::TemplateArgument::Type)
          name += "<" + storage_type(ast, arg.getAsType(), depth + 1) + ">";
        else if (arg.getKind() == clang::TemplateArgument::Integral) {
          llvm::SmallString<32> value;
          arg.getAsIntegral().toString(value, 10);
          name += "<#" + value.str().str() + ">";
        } else if (arg.getKind() == clang::TemplateArgument::Pack) {
          for (const auto &element : arg.pack_elements())
            if (!self(self, element))
              return false;
        } else {
          return false;
        }
        return true;
      };
      for (const auto &arg : special->getTemplateArgs().asArray())
        if (!append(append, arg))
          return "?";
      return name;
    }
    return storage_name(*record);
  }
  if (const auto *enumeration = type->getAs<clang::EnumType>())
    return storage_name(*enumeration->getDecl());
  if (type->isVoidType()) return "void";
  if (type->isBooleanType()) return "bool";
  if (type->isCharType())
    return ast.getTargetInfo().getTriple().isAArch64() ? "char" :
        type->isSignedIntegerType() ? "signed-char" : "unsigned-char";
  if (type->isSpecificBuiltinType(clang::BuiltinType::SChar)) return "signed-char";
  if (type->isSpecificBuiltinType(clang::BuiltinType::UChar)) return "unsigned-char";
  if (type->isSignedIntegerType()) return "signed" + std::to_string(ast.getTypeSize(type));
  if (type->isUnsignedIntegerType()) return "unsigned" + std::to_string(ast.getTypeSize(type));
  if (type->isRealFloatingType()) return "float" + std::to_string(ast.getTypeSize(type));
  if (const auto *array = llvm::dyn_cast<clang::ConstantArrayType>(type.getTypePtr()))
    return "[" + std::to_string(array->getSize().getLimitedValue()) + "]" +
        storage_type(ast, array->getElementType(), depth + 1);
  return "?";
}

void describe_receiver_storage(clang::ASTContext &ast, const clang::CXXRecordDecl *receiver,
                               const PreparedSnapshots &snapshots, api::PreparedInjector &package) {
  std::set<const clang::CXXRecordDecl *> seen;
  std::vector<const clang::CXXRecordDecl *> pending = snapshots.types;
  pending.push_back(receiver);
  while (!pending.empty()) {
    const auto *record = pending.back();
    pending.pop_back();
    if (!record || !(record = record->getDefinition()) || !seen.insert(record).second)
      continue;
    if (const auto *special = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record)) {
      const auto add = [&](auto &&self, const clang::TemplateArgument &arg) -> void {
        if (arg.getKind() == clang::TemplateArgument::Type && !arg.getAsType()->isPointerType())
          pending.push_back(snapshot_record_type(arg.getAsType()));
        else if (arg.getKind() == clang::TemplateArgument::Pack)
          for (const auto &element : arg.pack_elements())
            self(self, element);
      };
      for (const auto &arg : special->getTemplateArgs().asArray())
        add(add, arg);
    }
    if (record->isInStdNamespace())
      continue;
    const auto &layout = ast.getASTRecordLayout(record);
    auto &description = *package.add_receiver_storage();
    description.set_name(storage_type(ast, tracer::prepared::record_type(ast, record)));
    description.set_bytes(layout.getSize().getQuantity());
    for (const auto &base : record->bases()) {
      const auto *decl = base.getType()->getAsCXXRecordDecl()->getDefinition();
      auto &member = *description.add_members();
      member.set_base(true);
      member.set_type(storage_type(ast, base.getType()));
      member.set_needs_dynamic_location(base.isVirtual());
      if (!base.isVirtual())
        member.set_offset_bits(ast.toBits(layout.getBaseClassOffset(decl)));
      pending.push_back(decl);
    }
    for (const auto *field : record->fields()) {
      auto &member = *description.add_members();
      member.set_name(field->getNameAsString());
      member.set_offset_bits(layout.getFieldOffset(field->getFieldIndex()));
      member.set_type(storage_type(ast, field->getType()));
      member.set_needs_dynamic_location(field->isBitField() || field->getType()->isIncompleteType());
      if (!member.needs_dynamic_location())
        member.set_bytes(ast.getTypeSizeInChars(field->getType()).getQuantity());
      auto value = field->getType();
      while (const auto *array = ast.getAsArrayType(value))
        value = array->getElementType();
      if (!value->isPointerType() && !value->isReferenceType())
        pending.push_back(snapshot_record_type(value));
    }
  }
}

struct Selector {
  std::string qualified_name;
  bool has_parameters = false;
  std::vector<std::string> parameters;
  bool is_const = false;
  bool is_volatile = false;
  clang::RefQualifierKind ref_qualifier = clang::RQ_None;
};

std::vector<std::string> split_parameters(std::string_view text) {
  std::vector<std::string> result;
  if (trim(std::string(text)).empty()) {
    return result;
  }
  std::size_t start = 0;
  int angles = 0, parens = 0, brackets = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    switch (text[i]) {
    case '<':
      ++angles;
      break;
    case '>':
      if (angles > 0) {
        --angles;
      }
      break;
    case '(':
      ++parens;
      break;
    case ')':
      if (parens > 0) {
        --parens;
      }
      break;
    case '[':
      ++brackets;
      break;
    case ']':
      if (brackets > 0) {
        --brackets;
      }
      break;
    case ',':
      if (angles == 0 && parens == 0 && brackets == 0) {
        result.push_back(trim(std::string(text.substr(start, i - start))));
        start = i + 1;
      }
      break;
    default:
      break;
    }
  }
  result.push_back(trim(std::string(text.substr(start))));
  return result;
}

Selector parse_selector(const std::string& text) {
  Selector selector;
  auto open = std::string::npos;
  unsigned angles = 0, parentheses = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];
    if (ch == '(') {
      if (!angles) {
        open = i;
        break;
      }
      ++parentheses;
    } else if (ch == ')' && parentheses) {
      --parentheses;
    } else if (!parentheses && ch == '<') {
      ++angles;
    } else if (!parentheses && ch == '>' && angles) {
      --angles;
    }
  }
  if (open == std::string::npos)
    open = text.find('('); // Preserve legacy parsing for operator spellings.
  if (open == std::string::npos) {
    selector.qualified_name = trim(text);
    if (selector.qualified_name.empty()) {
      throw std::runtime_error("callable name is empty");
    }
    return selector;
  }
  const auto close = text.rfind(')');
  if (close == std::string::npos || close < open) {
    throw std::runtime_error("callable selector has an unmatched parameter list");
  }
  selector.qualified_name = trim(text.substr(0, open));
  if (selector.qualified_name.empty()) {
    throw std::runtime_error("callable name is empty");
  }
  selector.has_parameters = true;
  selector.parameters = split_parameters(std::string_view(text).substr(open + 1, close - open - 1));
  auto suffix = compact(trim(text.substr(close + 1)));
  while (!suffix.empty()) {
    suffix = trim(std::move(suffix));
    if (::tracer::compat::starts_with(suffix, "const")) {
      if (selector.is_const) {
        throw std::runtime_error("duplicate const qualifier in selector");
      }
      selector.is_const = true;
      suffix.erase(0, 5);
    } else if (::tracer::compat::starts_with(suffix, "volatile")) {
      if (selector.is_volatile) {
        throw std::runtime_error("duplicate volatile qualifier in selector");
      }
      selector.is_volatile = true;
      suffix.erase(0, 8);
    } else if (::tracer::compat::starts_with(suffix, "&&")) {
      if (selector.ref_qualifier != clang::RQ_None) {
        throw std::runtime_error("duplicate ref qualifier in selector");
      }
      selector.ref_qualifier = clang::RQ_RValue;
      suffix.erase(0, 2);
    } else if (::tracer::compat::starts_with(suffix, "&")) {
      if (selector.ref_qualifier != clang::RQ_None) {
        throw std::runtime_error("duplicate ref qualifier in selector");
      }
      selector.ref_qualifier = clang::RQ_LValue;
      suffix.erase(0, 1);
    } else {
      throw std::runtime_error("unsupported callable qualifier: " + suffix);
    }
  }
  return selector;
}

struct Candidate {
  std::string name;
  std::string symbol;
  std::string declaration;
  std::string source;
  std::string canonical_identity;
  std::vector<std::string> parameter_spellings;
  std::vector<std::string> canonical_parameter_spellings;
  bool is_const = false;
  bool is_volatile = false;
  clang::RefQualifierKind ref_qualifier = clang::RQ_None;
  std::string snapshot_result_root;
  api::ProbeSpec spec;
  std::string unsupported_reason;
  bool selected_by_type = false;
  bool requires_typed_adapter = false;
};

struct FrozenInvocation {
  std::string working_directory;
  std::string source_argument;
  std::vector<std::string> arguments;
  std::vector<std::string> prepared_headers;
  api::PreparedInjector prepared_profile;
  std::string adapter_symbol;
  bool select_template = false;
  bool pointer_layouts = true;
  bool receiver_storage = false;
};

class PrepareDiagnostics final : public clang::TextDiagnosticPrinter {
public:
  explicit PrepareDiagnostics(clang::DiagnosticOptions& options)
      : clang::TextDiagnosticPrinter(llvm::errs(), options) {}

  void HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                        const clang::Diagnostic& diagnostic) override {
    had_error_ |= level >= clang::DiagnosticsEngine::Error;
    clang::TextDiagnosticPrinter::HandleDiagnostic(level, diagnostic);
  }

  // A ToolInvocation can continue into the frontend after a driver error.
  bool had_error() const { return had_error_; }

private:
  bool had_error_ = false;
};

bool is_receiver_field_type(clang::QualType type) {
  type = type.getCanonicalType();
  if (const auto* array = llvm::dyn_cast<clang::ConstantArrayType>(type.getTypePtr())) {
    return is_receiver_field_type(array->getElementType());
  }
  if (type->isPointerType()) {
    auto pointee = type->getPointeeType().getCanonicalType();
    return pointee->isVoidType() || pointee->isBuiltinType();
  }
  return type->isBuiltinType() && type->isScalarType();
}

std::string receiver_layout_identity(const api::ReceiverContext& receiver) {
  std::string identity = "|receiver=" + receiver.record_name() +
                         "|size=" + std::to_string(receiver.size()) +
                         "|align=" + std::to_string(receiver.alignment()) +
                         "|volatile=" + (receiver.is_volatile() ? "1" : "0");
  for (const auto& field : receiver.fields()) {
    identity += "|field=" + field.name() + ":" + field.canonical_type() + ":" +
                std::to_string(field.bit_offset()) + ":" + std::to_string(field.bit_width()) + ":" +
                std::to_string(field.bit_size()) + ":" + std::to_string(field.bit_alignment()) +
                ":" + (field.is_mutable() ? "1" : "0") + ":" + field.access() + ":" +
                field.abi_identity();
  }
  return identity;
}

std::string type_spelling(clang::QualType type, const clang::PrintingPolicy& policy) {
  return type.getAsString(policy);
}

api::ValueType scalar_type(clang::QualType type, clang::ASTContext& context, bool is_result) {
  if (type->isReferenceType()) {
    throw std::runtime_error("reference parameters and results are unsupported");
  }
  type = type.getCanonicalType().getUnqualifiedType();
  if (type->isVoidType()) {
    if (is_result) {
      return api::VOID;
    }
    throw std::runtime_error("void parameter is invalid");
  }
  if (type->isPointerType()) {
    const auto pointer = type.getCanonicalType();
    const auto pointee = pointer->getPointeeType();
    if (pointer.getAddressSpace() != clang::LangAS::Default ||
        pointee.getAddressSpace() != clang::LangAS::Default || context.getTypeSize(pointer) != 64) {
      throw std::runtime_error("non-default-address-space or non-64-bit pointers are unsupported");
    }
    return api::POINTER;
  }
  if (type->isMemberPointerType() || type->isBlockPointerType()) {
    throw std::runtime_error("member and block pointers are unsupported");
  }
  if (type->isBooleanType() || type->isCharType() || type->isBitIntType()) {
    throw std::runtime_error("bool, character, and bit-precise integer types are unsupported");
  }
  if (type->isFloatingType()) {
    const auto bits = context.getTypeSize(type);
    if (bits == 32) {
      return api::FLOAT32;
    }
    if (bits == 64) {
      return api::FLOAT64;
    }
    throw std::runtime_error("floating type width is unsupported");
  }
  if (type->isIntegerType() || type->isEnumeralType()) {
    const auto bits = context.getTypeSize(type);
    const bool is_signed = type->isSignedIntegerOrEnumerationType();
    if (bits == 32 && is_signed) {
      return api::INT32;
    }
    if (bits == 64 && is_signed) {
      return api::INT64;
    }
    if (bits == 64 && !is_signed) {
      return api::UINT64;
    }
    throw std::runtime_error("integer width or signedness has no exact trace value type");
  }
  throw std::runtime_error("aggregate, vector, and other non-scalar types are unsupported");
}

class Collector : public clang::RecursiveASTVisitor<Collector> {
public:
  Collector(std::vector<Candidate>& candidates, const std::string& qualified_name,
            FrozenInvocation invocation, std::string translation_unit, std::string target_triple,
            clang::Sema *sema)
      : candidates_(candidates)
      , qualified_name_(qualified_name)
      , invocation_(std::move(invocation))
      , translation_unit_(std::move(translation_unit))
      , target_triple_(std::move(target_triple)), sema_(sema) {}

  bool VisitFunctionDecl(clang::FunctionDecl* declaration) {
    if (invocation_.select_template)
      return true;
    if (!invocation_.adapter_symbol.empty()) {
      if (!tracer::prepared::adapter_callable(*declaration, *context_, qualified_name_,
                                             invocation_.adapter_symbol))
        return true;
    } else if (declaration->getQualifiedNameAsString() != qualified_name_) {
      return true;
    }
    return collect(declaration, false);
  }

  bool VisitTypeAliasDecl(clang::TypeAliasDecl *alias) {
    if (!invocation_.select_template ||
        alias->getQualifiedNameAsString() != "tracer_prepared_selection::Selected")
      return true;
    const auto *type = llvm::dyn_cast<clang::DecltypeType>(alias->getUnderlyingType().getTypePtr());
    const auto *call = type ? llvm::dyn_cast<clang::CallExpr>(
        type->getUnderlyingExpr()->IgnoreParenImpCasts()) : nullptr;
    const auto *address = call && call->getNumArgs() == 1
        ? llvm::dyn_cast<clang::UnaryOperator>(call->getArg(0)->IgnoreParenImpCasts()) : nullptr;
    const auto *reference = address && address->getOpcode() == clang::UO_AddrOf
        ? llvm::dyn_cast<clang::DeclRefExpr>(address->getSubExpr()->IgnoreParenImpCasts()) : nullptr;
    auto *function = reference ? llvm::dyn_cast<clang::FunctionDecl>(reference->getDecl()) : nullptr;
    return function ? collect(function, true) : true;
  }

  bool collect(const clang::FunctionDecl *declaration, bool selected_by_type) {
    auto* prototype = declaration->getType()->getAs<clang::FunctionProtoType>();
    if (!prototype) {
      return true;
    }
    const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(declaration);

    Candidate candidate;
    if (!invocation_.adapter_symbol.empty()) {
      const auto result_type = declaration->getReturnType();
      if (result_type->isPointerType() || result_type->isReferenceType())
        if (const auto *record = snapshot_record_type(result_type))
          candidate.snapshot_result_root = tracer::prepared::type_name(
              *context_, tracer::prepared::record_type(*context_, record));
    }
    candidate.name = selected_by_type ? qualified_name_ : declaration->getQualifiedNameAsString();
    candidate.selected_by_type = selected_by_type;
    candidate.requires_typed_adapter = tracer::prepared::template_instance(*declaration) ||
                                       tracer::prepared::nested_static_method(method);
    candidate.is_const = method && method->isConst();
    candidate.is_volatile = method && method->isVolatile();
    candidate.ref_qualifier = method ? method->getRefQualifier() : clang::RQ_None;
    clang::PrintingPolicy policy(context_->getLangOpts());
    policy.SuppressScope = false;
    for (const auto* parameter : declaration->parameters()) {
      candidate.parameter_spellings.push_back(type_spelling(parameter->getType(), policy));
      candidate.spec.add_argument_names(parameter->getName().empty()
          ? "arg" + std::to_string(candidate.parameter_spellings.size() - 1)
          : parameter->getNameAsString());
    }
    for (unsigned i = 0; i < prototype->getNumParams(); ++i) {
      candidate.canonical_parameter_spellings.push_back(
          type_spelling(prototype->getParamType(i).getCanonicalType(), policy));
    }
    candidate.declaration =
        type_spelling(declaration->getReturnType(), policy) + " " + candidate.name + "(";
    for (std::size_t i = 0; i < candidate.parameter_spellings.size(); ++i) {
      if (i) {
        candidate.declaration += ", ";
      }
      candidate.declaration += candidate.parameter_spellings[i];
    }
    candidate.declaration += ")";
    if (candidate.is_const) {
      candidate.declaration += " const";
    }
    if (candidate.is_volatile) {
      candidate.declaration += " volatile";
    }
    if (candidate.ref_qualifier == clang::RQ_LValue) {
      candidate.declaration += " &";
    }
    if (candidate.ref_qualifier == clang::RQ_RValue) {
      candidate.declaration += " &&";
    }
    candidate.source = context_->getSourceManager().getFilename(declaration->getLocation()).str();
    candidate.canonical_identity =
        declaration->getType().getCanonicalType().getAsString(policy) + "|" +
        (method && method->isStatic() ? "static"
         : method                     ? "method"
                                      : "function") +
        "|const=" + (candidate.is_const ? "1" : "0") +
        "|volatile=" + (candidate.is_volatile ? "1" : "0") +
        "|ref=" + std::to_string(static_cast<int>(candidate.ref_qualifier));

    if (method && !method->isStatic() && !sema_) {
      set_receiver(candidate, method, policy);
      if (candidate.spec.has_receiver()) {
        candidate.canonical_identity += receiver_layout_identity(candidate.spec.receiver());
      }
    }

    try {
      if (declaration->getDeclContext()->isDependentContext() ||
          declaration->getType()->isDependentType()) {
        throw std::runtime_error("dependent callable types are unsupported");
      }
      if (!sema_ && method && method->isVirtual()) {
        throw std::runtime_error("virtual methods and their possible thunks are unsupported");
      }
      if (llvm::isa<clang::CXXConstructorDecl>(declaration) ||
          llvm::isa<clang::CXXDestructorDecl>(declaration)) {
        throw std::runtime_error("constructors and destructors are unsupported");
      }
      if (declaration->getFormalLinkage() != clang::Linkage::External) {
        throw std::runtime_error("callables with internal or non-external linkage are unsupported");
      }
      if (prototype->getCallConv() != clang::CC_C) {
        throw std::runtime_error("non-C calling convention is unsupported");
      }
      const auto function_info = prototype->getExtInfo();
      if (function_info.getHasRegParm() || function_info.getProducesResult() ||
          function_info.getNoCallerSavedRegs() || function_info.getCmseNSCall()) {
        throw std::runtime_error("ABI-changing function attributes are unsupported");
      }
      if (invocation_.adapter_symbol.empty() && declaration->hasAttr<clang::AsmLabelAttr>()) {
        throw std::runtime_error("asm-labelled declarations are unsupported");
      }
      if (prototype->isVariadic()) {
        throw std::runtime_error("variadic callables are unsupported");
      }
      if (declaration->getNumParams() > kMaxArguments) {
        throw std::runtime_error("callables with more than eight arguments are unsupported");
      }
      if (prototype->hasExtParameterInfos()) {
        for (const auto& info : prototype->getExtParameterInfos()) {
          if (info != clang::FunctionProtoType::ExtParameterInfo{}) {
            throw std::runtime_error("ABI-annotated parameters are unsupported");
          }
        }
      }
      if (method && !method->isStatic() && !method->getParent()->isCompleteDefinition()) {
        throw std::runtime_error("incomplete class method is unsupported");
      }
      candidate.spec.set_symbol(invocation_.adapter_symbol.empty()
          ? tracer::prepared::mangle(declaration, *context_) : invocation_.adapter_symbol);
      candidate.spec.mutable_signature()->set_kind(method && !method->isStatic() ? api::METHOD
                                                                                 : api::FUNCTION);
      if (sema_) {
        tracer::prepared::describe_prepared_callable(*context_, *declaration, method,
            candidate.spec, sema_, nullptr, invocation_.pointer_layouts);
        if (!invocation_.prepared_headers.empty()) {
          const auto snapshots = prepare_snapshot_roots(*context_, *declaration, method);
          auto adapter = tracer::prepared::AdapterEmitter::emit(*context_, *header_search_,
              *declaration, invocation_.prepared_headers, invocation_.pointer_layouts,
              snapshots.types);
          auto &package = *candidate.spec.mutable_prepared();
          package = invocation_.prepared_profile;
          package.set_target_triple(context_->getTargetInfo().getTriple().str());
          package.set_adapter_declarations(std::move(adapter.source));
          for (const auto *snapshot_type : snapshots.types)
            tracer::prepared::describe_record(*context_, *snapshot_type,
                *package.add_snapshot_types());
          if (invocation_.receiver_storage && method && !method->isStatic())
            describe_receiver_storage(*context_, method->getParent(), snapshots, package);
          for (const auto &record : snapshots.records)
            *package.add_snapshot_records() = record;
          for (const auto &header : adapter.standard_headers)
            package.add_adapter_standard_headers(header);
        }
      } else {
        auto *signature = candidate.spec.mutable_signature();
        signature->set_result(scalar_type(declaration->getReturnType(), *context_, true));
        for (const auto *parameter : declaration->parameters())
          signature->add_arguments(scalar_type(parameter->getType(), *context_, false));
      }
      candidate.symbol = candidate.spec.symbol();
    } catch (const std::exception& error) {
      candidate.unsupported_reason = error.what();
      candidate.symbol = candidate.name + "|" + candidate.declaration + "|unsupported";
    }
    candidates_.push_back(std::move(candidate));
    return true;
  }

  void set_context(clang::ASTContext& context, clang::Sema *sema,
                   const clang::HeaderSearch *header_search = nullptr) {
    context_ = &context;
    sema_ = sema;
    header_search_ = header_search;
  }

  void collect_translation_unit(clang::ASTContext &context, clang::Sema *sema) {
    clang::DiagnosticOptions diagnostic_options;
    PrepareDiagnostics diagnostics(diagnostic_options);
    std::unique_ptr<clang::ASTUnit> toolchain;
    if (!invocation_.prepared_headers.empty()) {
      auto arguments = tracer::prepared::toolchain_arguments(invocation_.prepared_profile,
          {runtime::jit_flags.begin(), runtime::jit_flags.end()});
      arguments.insert(arguments.end(), {"-std=c++23", "-x", "c++",
          "-resource-dir=" + std::string(TRACER_CLANG_RESOURCE_DIR)});
      const auto &abi_options = invocation_.prepared_profile.adapter_abi_options();
      arguments.insert(arguments.end(), abi_options.begin(), abi_options.end());
      toolchain = clang::tooling::buildASTFromCodeWithArgs("", tracer::clang_driver_arguments(std::move(arguments)),
          "prepared-toolchain.cc", "clang-tool", std::make_shared<clang::PCHContainerOperations>(),
          clang::tooling::getClangStripDependencyFileAdjuster(), {}, &diagnostics);
      if (!toolchain || diagnostics.had_error() || toolchain->getDiagnostics().hasErrorOccurred())
        throw std::invalid_argument("Clang could not resolve the prepared toolchain headers");
    }
    set_context(context, sema, toolchain ? &toolchain->getPreprocessor().getHeaderSearchInfo() : nullptr);
    TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  void set_receiver(Candidate& candidate, const clang::CXXMethodDecl* method,
                    const clang::PrintingPolicy& policy) {
    auto unavailable = [&candidate](std::string reason) {
      candidate.spec.clear_receiver();
      candidate.spec.set_receiver_unavailable_reason(std::move(reason));
    };
    const auto* record = method->getParent();
    if (!record->isCompleteDefinition() || record->isUnion() || record->isDependentType() ||
        record->getDescribedClassTemplate() ||
        llvm::isa<clang::ClassTemplateSpecializationDecl>(record) || record->isDynamicClass() ||
        record->getNumBases() != 0) {
      unavailable("receiver class has an unsupported layout");
      return;
    }
    const auto fields = record->fields();
    if (std::distance(fields.begin(), fields.end()) >
        static_cast<std::ptrdiff_t>(kMaxReceiverFields)) {
      unavailable("receiver class has more than 128 fields");
      return;
    }

    const auto& source_manager = context_->getSourceManager();
    const auto main_file = source_manager.getMainFileID();
    const auto main_location = source_manager.getLocForStartOfFile(main_file);
    const auto source_path = translation_unit_.empty()
                                 ? source_manager.getFilename(main_location).str()
                                 : translation_unit_;
    if (invocation_.arguments.empty()) {
      unavailable("resolver did not capture the translation unit compiler invocation");
      return;
    }
    std::size_t total_argument_bytes = 0;
    for (const auto& argument : invocation_.arguments) {
      if (argument.size() + 1 > kMaxReceiverArgumentBytes - total_argument_bytes) {
        unavailable("translation unit invocation exceeds receiver metadata limits");
        return;
      }
      total_argument_bytes += argument.size() + 1;
    }
    if (source_path.empty() || source_path.size() >= kMaxReceiverSourcePath ||
        invocation_.working_directory.empty() || invocation_.source_argument.empty() ||
        invocation_.arguments.size() > kMaxReceiverArguments ||
        total_argument_bytes > kMaxReceiverArgumentBytes) {
      unavailable("translation unit invocation exceeds receiver metadata limits");
      return;
    }
    for (const auto* field : fields) {
      if (field->getName().empty() || !is_receiver_field_type(field->getType())) {
        unavailable("receiver class contains an unsupported field");
        return;
      }
    }

    auto* receiver = candidate.spec.mutable_receiver();
    receiver->set_translation_unit(source_path);
    receiver->set_working_directory(invocation_.working_directory);
    for (const auto& argument : invocation_.arguments) {
      receiver->add_compiler_arguments(argument);
    }
    receiver->set_record_name(record->getQualifiedNameAsString());
    receiver->set_clang_version(CLANG_VERSION_STRING);
    receiver->set_target_triple(target_triple_);
    const auto& layout = context_->getASTRecordLayout(record);
    receiver->set_size(static_cast<std::uint64_t>(layout.getSize().getQuantity()));
    receiver->set_alignment(static_cast<std::uint64_t>(layout.getAlignment().getQuantity()));
    receiver->set_is_volatile(method->isVolatile());
    unsigned index = 0;
    for (const auto* field : fields) {
      auto* resolved_field = receiver->add_fields();
      resolved_field->set_name(field->getNameAsString());
      resolved_field->set_canonical_type(field->getType().getCanonicalType().getAsString(policy));
      resolved_field->set_bit_offset(layout.getFieldOffset(index++));
      resolved_field->set_bit_width(field->isBitField() ? field->getBitWidthValue() : 0);
      resolved_field->set_bit_size(context_->getTypeSize(field->getType()));
      resolved_field->set_bit_alignment(context_->getTypeAlign(field->getType()));
      resolved_field->set_is_mutable(field->isMutable());
      resolved_field->set_abi_identity(
          tracer::receiver_abi::type_identity(field->getType(), *context_));
      switch (field->getAccess()) {
      case clang::AS_public:
        resolved_field->set_access("public");
        break;
      case clang::AS_protected:
        resolved_field->set_access("protected");
        break;
      case clang::AS_private:
        resolved_field->set_access("private");
        break;
      case clang::AS_none:
        resolved_field->set_access(record->isClass() ? "private" : "public");
        break;
      }
    }
    if (receiver->ByteSizeLong() > kMaxReceiverArgumentBytes) {
      candidate.spec.clear_receiver();
      candidate.spec.set_receiver_unavailable_reason("receiver metadata exceeds 64 KiB");
    }
  }

  clang::ASTContext* context_ = nullptr;
  std::vector<Candidate>& candidates_;
  const std::string& qualified_name_;
  FrozenInvocation invocation_;
  std::string translation_unit_;
  std::string target_triple_;
  clang::Sema *sema_;
  const clang::HeaderSearch *header_search_ = nullptr;
};

class Action : public clang::ASTFrontendAction {
public:
  explicit Action(std::vector<Candidate>& candidates, std::string& target_error,
                  const std::string& qualified_name, FrozenInvocation& frozen_invocation,
                  std::string& invocation_error, bool prepared = false)
      : candidates_(candidates)
      , target_error_(target_error)
      , qualified_name_(qualified_name)
      , frozen_invocation_(frozen_invocation)
      , invocation_error_(invocation_error), prepared_(prepared) {}

  bool BeginSourceFileAction(clang::CompilerInstance& compiler) override {
    const auto& target = compiler.getTarget();
    const auto& triple = target.getTriple();
    const bool supported_arch = triple.getArch() == llvm::Triple::x86_64 ||
        (prepared_ && frozen_invocation_.receiver_storage && triple.getArch() == llvm::Triple::aarch64);
    if (!supported_arch || !triple.isOSLinux() ||
        target.getPointerWidth(clang::LangAS::Default) != 64 || target.getLongWidth() != 64 ||
        (prepared_ && (!triple.isOSBinFormatELF() || !target.isLittleEndian() ||
                       !target.getCXXABI().isItaniumFamily()))) {
      target_error_ = prepared_
          ? "compilation database ABI does not match the trace target (expected supported Linux "
            "ELF LP64 with the Itanium C++ ABI, found " + triple.str() + ")"
          : "compilation database must target Linux x86_64 (found " + triple.str() + ")";
      return false;
    }
    return true;
  }

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& compiler,
                                                        llvm::StringRef) override;

private:
  std::vector<Candidate>& candidates_;
  std::string& target_error_;
  const std::string& qualified_name_;
  FrozenInvocation& frozen_invocation_;
  std::string& invocation_error_;
  bool prepared_;
};

class Consumer : public clang::ASTConsumer {
public:
  Consumer(std::vector<Candidate>& candidates, const std::string& qualified_name,
           FrozenInvocation invocation, std::string translation_unit, std::string target_triple,
           clang::CompilerInstance *compiler)
      : visitor_(candidates, qualified_name, std::move(invocation), std::move(translation_unit),
                 std::move(target_triple), nullptr), compiler_(compiler) {}

  void HandleTranslationUnit(clang::ASTContext& context) override {
    visitor_.collect_translation_unit(context, compiler_ ? &compiler_->getSema() : nullptr);
  }

private:
  Collector visitor_;
  clang::CompilerInstance *compiler_;
};

std::unique_ptr<clang::ASTConsumer> Action::CreateASTConsumer(clang::CompilerInstance& compiler,
                                                              llvm::StringRef) {
  auto invocation = frozen_invocation_;
  if (compiler.getFrontendOpts().Inputs.empty()) {
    invocation_error_ = "Clang action has no main translation unit input";
    return std::make_unique<Consumer>(candidates_, qualified_name_, std::move(invocation), "",
                                      compiler.getTarget().getTriple().str(), prepared_ ? &compiler : nullptr);
  }
  const auto input = compiler.getFrontendOpts().Inputs.front().getFile().str();
  auto& vfs = compiler.getFileManager().getVirtualFileSystem();
  const auto vfs_working_directory = vfs.getCurrentWorkingDirectory();
  if (!vfs_working_directory) {
    invocation_error_ =
        "cannot read Clang VFS working directory: " + vfs_working_directory.getError().message();
    return std::make_unique<Consumer>(candidates_, qualified_name_, std::move(invocation), "",
                                      compiler.getTarget().getTriple().str(), prepared_ ? &compiler : nullptr);
  }
  auto absolute_vfs_path = [&vfs](llvm::StringRef path) {
    llvm::SmallString<256> normalized(path);
    if (vfs.makeAbsolute(normalized)) {
      return std::string();
    }
    llvm::SmallString<256> real_path;
    if (!vfs.getRealPath(normalized, real_path)) {
      normalized = real_path;
    }
    llvm::sys::path::remove_dots(normalized, true);
    return std::string(normalized.str());
  };
  invocation.working_directory = absolute_vfs_path(*vfs_working_directory);
  const auto absolute_input = absolute_vfs_path(input);
  const auto absolute_adjuster_input = absolute_vfs_path(invocation.source_argument);
  if (invocation.arguments.empty() || absolute_input.empty() ||
      absolute_adjuster_input != absolute_input || invocation.working_directory.empty()) {
    invocation_error_ = "Clang action did not match a complete frozen source invocation";
  }
  invocation.source_argument = absolute_input;
  std::vector<std::string> normalized_arguments;
  normalized_arguments.reserve(invocation.arguments.size() + 1);
  if (!invocation.arguments.empty()) {
    normalized_arguments.push_back(invocation.arguments.front());
  }
  for (std::size_t i = 1; i < invocation.arguments.size(); ++i) {
    const auto& argument = invocation.arguments[i];
    const auto normalized_argument = absolute_vfs_path(argument);
    if (!normalized_argument.empty() && normalized_argument == absolute_input) {
      continue;
    }
    normalized_arguments.push_back(argument);
  }
  invocation.arguments = std::move(normalized_arguments);
  return std::make_unique<Consumer>(candidates_, qualified_name_, std::move(invocation),
                                    absolute_input, compiler.getTarget().getTriple().str(),
                                    prepared_ ? &compiler : nullptr);
}

class ActionFactory : public clang::tooling::FrontendActionFactory {
public:
  ActionFactory(std::vector<Candidate>& candidates, std::string& target_error,
                const std::string& qualified_name, FrozenInvocation& frozen_invocation,
                std::string& invocation_error, bool prepared = false)
      : candidates_(candidates)
      , target_error_(target_error)
      , qualified_name_(qualified_name)
      , frozen_invocation_(frozen_invocation)
      , invocation_error_(invocation_error), prepared_(prepared) {}

  std::unique_ptr<clang::FrontendAction> create() override {
    return std::make_unique<Action>(candidates_, target_error_, qualified_name_, frozen_invocation_,
                                    invocation_error_, prepared_);
  }

private:
  std::vector<Candidate>& candidates_;
  std::string& target_error_;
  const std::string& qualified_name_;
  FrozenInvocation& frozen_invocation_;
  std::string& invocation_error_;
  bool prepared_;
};

std::unique_ptr<clang::tooling::CompilationDatabase> load_database(const std::string& requested) {
  std::string error;
  if (!requested.empty()) {
    fs::path path(requested);
    if (fs::is_regular_file(path)) {
      auto db = clang::tooling::JSONCompilationDatabase::loadFromFile(
          path.string(), error, clang::tooling::JSONCommandLineSyntax::AutoDetect);
      if (!db) {
        throw std::runtime_error("cannot load compilation database: " + error);
      }
      return db;
    }
    if (fs::is_directory(path)) {
      auto db = clang::tooling::CompilationDatabase::loadFromDirectory(path.string(), error);
      if (!db) {
        throw std::runtime_error("cannot load compilation database: " + error);
      }
      return db;
    }
    throw std::runtime_error("compilation database path does not exist: " + requested);
  }
  fs::path cwd = fs::current_path();
  const std::vector<fs::path> bases{cwd, cwd / "build", cwd.parent_path(),
                                    cwd.parent_path() / "build"};
  for (const auto& base : bases) {
    error.clear();
    auto db = clang::tooling::CompilationDatabase::loadFromDirectory(base.string(), error);
    if (db) {
      return db;
    }
  }
  throw std::runtime_error("no compile_commands.json found in the current "
                           "directory, build/, parent, or parent/build/");
}

std::string normalized_path(const std::string& path, const std::string& directory = {}) {
  fs::path p(path);
  if (p.is_relative() && !directory.empty()) {
    p = fs::path(directory) / p;
  }
  std::error_code error;
  auto canonical = fs::weakly_canonical(p, error);
  return (error ? p.lexically_normal() : canonical).string();
}

bool has_resource_dir_argument(const clang::tooling::CommandLineArguments& arguments) {
  return std::any_of(arguments.begin(), arguments.end(), [](const auto& argument) {
    return argument == "-resource-dir" || ::tracer::compat::starts_with(argument, "-resource-dir=");
  });
}

bool matches(const Candidate& candidate, const Selector& selector) {
  if (candidate.name != selector.qualified_name) {
    return false;
  }
  if (!selector.has_parameters || candidate.selected_by_type) {
    return true;
  }
  if (candidate.parameter_spellings.size() != selector.parameters.size() ||
      candidate.is_const != selector.is_const || candidate.is_volatile != selector.is_volatile ||
      candidate.ref_qualifier != selector.ref_qualifier) {
    return false;
  }
  for (std::size_t i = 0; i < selector.parameters.size(); ++i) {
    const auto requested = compact(selector.parameters[i]);
    if (compact(candidate.parameter_spellings[i]) != requested &&
        compact(candidate.canonical_parameter_spellings[i]) != requested) {
      return false;
    }
  }
  return true;
}

} // namespace

static ResolvedCallable select_cpp_candidate(std::vector<Candidate> candidates,
                                                const Selector& selector,
                                                const std::string& name) {
  std::unordered_map<std::string, Candidate> unique;
  for (auto& candidate : candidates) {
    if (!matches(candidate, selector)) {
      continue;
    }
    const auto existing = unique.find(candidate.symbol);
    if (existing == unique.end()) {
      unique.try_emplace(candidate.symbol, std::move(candidate));
    } else if (existing->second.canonical_identity != candidate.canonical_identity ||
               existing->second.unsupported_reason != candidate.unsupported_reason) {
      throw std::runtime_error("conflicting declarations resolve to the same symbol '" +
                               existing->second.spec.symbol() + "'");
    }
  }
  if (unique.empty()) {
    throw std::runtime_error("no callable matched '" + name + "'");
  }
  if (unique.size() != 1) {
    std::string message = "callable selector is ambiguous; candidates:";
    for (const auto& [_, candidate] : unique) {
      message += "\n  " + candidate.declaration;
    }
    throw std::runtime_error(message);
  }
  auto candidate = std::move(unique.begin()->second);
  if (!candidate.unsupported_reason.empty()) {
    throw std::runtime_error("callable is unsupported: " + candidate.unsupported_reason);
  }
  ResolvedCallable resolved;
  resolved.spec = std::move(candidate.spec);
  resolved.declaration = std::move(candidate.declaration);
  resolved.source = std::move(candidate.source);
  resolved.requires_typed_adapter = candidate.requires_typed_adapter;
  return resolved;
}

ResolvedCallable resolve_cpp_callable(const std::string& name,
                                      const std::string& compilation_database,
                                      const std::vector<std::string>& sources) {
  const auto selector = parse_selector(name);
  if (sources.size() > kMaxExplicitSources) {
    throw std::runtime_error("too many --source entries (maximum 128)");
  }
  auto database = load_database(compilation_database);
  std::vector<std::string> source_files;
  if (sources.empty()) {
    source_files = database->getAllFiles();
  } else {
    std::set<std::string> requested;
    for (const auto& source : sources) {
      requested.insert(normalized_path(source));
    }
    for (const auto& file : database->getAllFiles()) {
      const auto commands = database->getCompileCommands(file);
      bool found = false;
      for (const auto& command : commands) {
        if (::tracer::compat::contains(requested, normalized_path(file, command.Directory))) {
          found = true;
          break;
        }
      }
      if (found) {
        source_files.push_back(file);
      }
    }
    if (source_files.size() != requested.size()) {
      throw std::runtime_error("one or more --source files are not present in "
                               "the compilation database");
    }
  }
  std::sort(source_files.begin(), source_files.end());
  source_files.erase(std::unique(source_files.begin(), source_files.end()), source_files.end());
  if (source_files.empty()) {
    throw std::runtime_error("compilation database contains no source files");
  }
  if (source_files.size() > kMaxTranslationUnits) {
    throw std::runtime_error("compilation database has more than 256 source "
                             "files; restrict with --source");
  }

  const fs::path resource_dir(TRACER_CLANG_RESOURCE_DIR);
  bool needs_default_resource_dir = false;
  for (const auto& source : source_files) {
    const auto commands = database->getCompileCommands(source);
    if (commands.empty()) {
      throw std::runtime_error("compilation database has no command for source: " + source);
    }
    for (const auto& command : commands) {
      needs_default_resource_dir |= !has_resource_dir_argument(command.CommandLine);
    }
  }
  if (needs_default_resource_dir && !fs::is_directory(resource_dir)) {
    throw std::runtime_error("configured Clang resource directory is missing: " +
                             resource_dir.string());
  }

  std::vector<Candidate> candidates;
  std::string target_error;
  FrozenInvocation frozen_invocation;
  std::string invocation_error;
  clang::tooling::ClangTool tool(*database, source_files);
  tool.appendArgumentsAdjuster(clang::tooling::getClangStripOutputAdjuster());
  tool.appendArgumentsAdjuster(clang::tooling::getClangSyntaxOnlyAdjuster());
  tool.appendArgumentsAdjuster(
      [resource_dir = resource_dir.string()](const clang::tooling::CommandLineArguments& arguments,
                                             llvm::StringRef) {
        auto adjusted = arguments;
        if (!has_resource_dir_argument(adjusted)) {
          adjusted.push_back("-resource-dir=" + resource_dir);
        }
        return adjusted;
      });
  tool.appendArgumentsAdjuster(
      [&frozen_invocation](const clang::tooling::CommandLineArguments& arguments,
                           llvm::StringRef filename) {
        frozen_invocation.arguments = arguments;
        frozen_invocation.source_argument = filename.str();
        return arguments;
      });
  ActionFactory factory(candidates, target_error, selector.qualified_name, frozen_invocation,
                        invocation_error);
  const int result = tool.run(&factory);
  if (!target_error.empty()) {
    throw std::runtime_error(target_error);
  }
  if (!invocation_error.empty()) {
    throw std::runtime_error(invocation_error);
  }
  if (result != 0) {
    throw std::runtime_error("Clang could not parse every selected translation unit; fix "
                             "diagnostics or restrict --source");
  }

  return select_cpp_candidate(std::move(candidates), selector, name);
}

std::vector<api::ObjectWatchRequest::SourceWriter> discover_object_writers(
    const std::string &type, const std::string &field, const std::string &compdb,
    const std::vector<std::string> &sources) {
  using Writer = api::ObjectWatchRequest::SourceWriter;
  if (type.empty() || field.empty() || sources.empty())
    throw std::invalid_argument("writer discovery requires a record, field and translation unit");
  auto database = load_database(compdb);
  std::map<std::string, Writer> writers;
  class Visitor final : public clang::RecursiveASTVisitor<Visitor> {
    clang::ASTContext &ast_;
    const std::string &type_;
    const std::string &field_;
    std::map<std::string, Writer> &writers_;

    void record(const clang::Expr *expression) {
      if (!expression)
        return;
      const auto *member = llvm::dyn_cast<clang::MemberExpr>(expression->IgnoreParenImpCasts());
      const auto *field = member ? llvm::dyn_cast<clang::FieldDecl>(member->getMemberDecl()) : nullptr;
      if (!field || field->getNameAsString() != field_ ||
          field->getParent()->getQualifiedNameAsString() != type_)
        return;
      auto parent = clang::DynTypedNode::create(*member);
      for (unsigned depth = 0; depth < 64; ++depth) {
        const auto parents = ast_.getParents(parent);
        if (parents.size() != 1)
          return;
        parent = parents[0];
        const auto *function = parent.get<clang::FunctionDecl>();
        if (!function)
          continue;
        if (llvm::isa<clang::CXXConstructorDecl, clang::CXXDestructorDecl>(function) ||
            function->isDependentContext())
          return;
        const auto symbol = tracer::prepared::mangle(function, ast_);
        if (symbol.empty() || writers_.count(symbol))
          return;
        const auto location = ast_.getSourceManager().getPresumedLoc(member->getExprLoc());
        Writer candidate;
        candidate.set_symbol(symbol);
        candidate.set_function(function->getQualifiedNameAsString());
        if (location.isValid()) {
          candidate.set_file(location.getFilename());
          candidate.set_line(location.getLine());
        }
        writers_.emplace(symbol, std::move(candidate));
        return;
      }
    }

  public:
    Visitor(clang::ASTContext &ast, const std::string &type, const std::string &field,
            std::map<std::string, Writer> &writers)
        : ast_(ast), type_(type), field_(field), writers_(writers) {}

    bool VisitBinaryOperator(clang::BinaryOperator *operation) {
      if (operation->isAssignmentOp())
        record(operation->getLHS());
      return true;
    }
    bool VisitUnaryOperator(clang::UnaryOperator *operation) {
      if (operation->isIncrementDecrementOp())
        record(operation->getSubExpr());
      return true;
    }
    bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *operation) {
      const auto *method = operation->getMethodDecl();
      if (method) {
        const auto name = method->getNameAsString();
        if (name == "store" || name == "exchange" || name.starts_with("fetch_") ||
            name.starts_with("compare_exchange_"))
          record(operation->getImplicitObjectArgument());
      }
      return true;
    }
  };
  for (const auto &source : sources) {
    if (database->getCompileCommands(source).empty())
      throw std::invalid_argument("no compile command for writer source: " + source);
    clang::tooling::ClangTool tool(*database, {source});
    tool.appendArgumentsAdjuster(clang::tooling::getClangStripOutputAdjuster());
    tool.appendArgumentsAdjuster(clang::tooling::getClangSyntaxOnlyAdjuster());
    tool.appendArgumentsAdjuster([](const clang::tooling::CommandLineArguments &arguments,
                                   llvm::StringRef) {
      auto result = arguments;
      if (!has_resource_dir_argument(result))
        result.push_back(std::string("-resource-dir=") + TRACER_CLANG_RESOURCE_DIR);
      return result;
    });
    std::vector<std::unique_ptr<clang::ASTUnit>> units;
    if (tool.buildASTs(units) || units.size() != 1)
      throw std::runtime_error("writer discovery requires one successfully parsed command for " + source);
    auto &ast = units.front()->getASTContext();
    Visitor visitor(ast, type, field, writers);
    visitor.TraverseDecl(ast.getTranslationUnitDecl());
  }
  std::vector<Writer> result;
  for (auto &[symbol, writer] : writers)
    result.push_back(std::move(writer));
  return result;
}

ResolvedCallable resolve_header_callable(const std::string& name,
                                         const api::ProbeSpec& headers,
                                         const PreparedHeaderCommand& command,
                                         bool pointer_layouts, bool receiver_storage) {
  const auto selector = parse_selector(name);
  if (!selector.has_parameters || headers.include_headers().empty())
    throw std::runtime_error("prepared generation requires an explicit selector and header context");
  // Use the SDK fallback unless the database names a different installed
  // compiler by path or explicitly selects a toolchain.
  std::vector<std::string> arguments{"--no-default-config"};
  const bool clang_cl = command.driver_mode == "cl";
  std::error_code path_error;
  const bool installed_driver = fs::path(command.driver).is_absolute() &&
      fs::is_regular_file(command.driver, path_error);
  const bool alternate_driver = installed_driver &&
      !fs::equivalent(command.driver, TRACER_PREPARE_BUILD_CXX, path_error) && !path_error;
  FrozenInvocation invocation;
  auto &profile = invocation.prepared_profile;
  profile.set_format_version(tracer::prepared::typed_version);
  profile.add_adapter_toolchain_options("--no-default-config");
  profile.add_adapter_toolchain_options("--driver-mode=" + command.driver_mode);
  for (const auto &option : command.adapter_abi_options)
    profile.add_adapter_abi_options(option);
  if (command.use_default_toolchain && !alternate_driver) {
    profile.set_adapter_uses_default_toolchain(true);
    for (const auto* flag : runtime::jit_flags) {
      arguments.push_back(clang_cl ? std::string("/clang:") + flag : flag);
    }
  }
  if (installed_driver && !profile.adapter_uses_default_toolchain()) {
    // The standalone compiler has a synthetic argv[0]. Preserve the donor's
    // installation directory so Clang can find its C++ standard library.
    profile.add_adapter_toolchain_options("-ccc-install-dir");
    profile.add_adapter_toolchain_options(fs::path(command.driver).parent_path().string());
  }
  const auto absolute_option_path = [&](llvm::StringRef path) {
    return (fs::path(command.directory) / fs::path(path.str())).lexically_normal().string();
  };
  // Carry driver toolchain selection, without the donor's application include
  // paths, forced headers, defines or other unrelated compiler switches.
  for (std::size_t index = 0; index < command.arguments.size(); ++index) {
    llvm::StringRef option(command.arguments[index]);
    option.consume_front("/clang:");
    const auto prefix = tracer::prepared::toolchain_path_option(
        std::string_view(option.data(), option.size()));
    if (option == "-isysroot" || option == "-ccc-install-dir") {
      if (++index == command.arguments.size())
        throw std::invalid_argument("missing prepared toolchain path");
      llvm::StringRef path(command.arguments[index]);
      path.consume_front("/clang:");
      if (option == "-ccc-install-dir") {
        profile.add_adapter_toolchain_options("-ccc-install-dir");
        profile.add_adapter_toolchain_options(absolute_option_path(path));
      } else {
        profile.add_adapter_toolchain_options("-isysroot" + absolute_option_path(path));
      }
    } else if (!prefix.empty()) {
      auto path = option.drop_front(prefix.size());
      if (prefix == "-isysroot")
        path.consume_front("=");
      if (!path.empty())
        profile.add_adapter_toolchain_options(std::string(prefix) + absolute_option_path(path));
    } else if (option.consume_front("--target=")) {
      profile.add_adapter_toolchain_options("--target=" + llvm::Triple::normalize(option));
    } else if (tracer::prepared::toolchain_switch(std::string_view(option.data(), option.size()))) {
      profile.add_adapter_toolchain_options(option.str());
    }
  }
  arguments.insert(arguments.end(), command.arguments.begin(), command.arguments.end());
  arguments.push_back("-resource-dir=" + std::string(TRACER_CLANG_RESOURCE_DIR));
  arguments.insert(arguments.end(), {"-fsyntax-only", "-x", "c++"});
  const auto source = (fs::path(command.directory) / "clang-tracer-prepared-headers.cc").string();
  std::string code;
  for (const auto& header : headers.include_headers())
    code += "#include \"" + header + "\"\n";
  const auto angle = selector.qualified_name.find('<');
  const bool select_template = angle != std::string::npos &&
      selector.qualified_name.find('>', angle) != std::string::npos;
  if (select_template) {
    if (name.find_first_of("#;{}\\\r\n") != std::string::npos)
      throw std::invalid_argument("invalid character in prepared template selector");
    std::string parameters;
    for (const auto &parameter : selector.parameters) {
      if (!parameters.empty())
        parameters += ", ";
      parameters += parameter;
    }
    const auto type = "(" + parameters + ")";
    std::string member = type;
    if (selector.is_const) member += " const";
    if (selector.is_volatile) member += " volatile";
    if (selector.ref_qualifier == clang::RQ_LValue) member += " &";
    if (selector.ref_qualifier == clang::RQ_RValue) member += " &&";
    const auto qualified = selector.qualified_name.rfind("::", 0) == 0
        ? selector.qualified_name : "::" + selector.qualified_name;
    code += "\nnamespace tracer_prepared_selection {\n";
    if (member == type)
      code += "template<class R> void select(R (*)" + type + ");\n";
    code += "template<class R, class C> void select(R (C::*)" + member + ");\n"
            "using Selected = decltype(select(&" + qualified + "));\n}\n";
    arguments.push_back(clang_cl ? "/clang:-fno-access-control" : "-fno-access-control");
  }
  clang::tooling::FixedCompilationDatabase database(command.directory, arguments);
  clang::DiagnosticOptions diagnostic_options;
  PrepareDiagnostics diagnostics(diagnostic_options);
  clang::tooling::ClangTool tool(database, {source});
  tool.clearArgumentsAdjusters();
  tool.setDiagnosticConsumer(&diagnostics);
  tool.mapVirtualFile(source, code);
  std::vector<Candidate> candidates;
  std::string target_error, invocation_error;
  invocation.select_template = select_template;
  invocation.pointer_layouts = pointer_layouts;
  invocation.receiver_storage = receiver_storage;
  invocation.prepared_headers.assign(headers.include_headers().begin(),
                                     headers.include_headers().end());
  tool.appendArgumentsAdjuster([&](const clang::tooling::CommandLineArguments& args,
                                   llvm::StringRef filename) {
    invocation.arguments = args;
    invocation.arguments.front() = command.driver;
    invocation.source_argument = filename.str();
    return invocation.arguments;
  });
  ActionFactory factory(candidates, target_error, selector.qualified_name, invocation,
                        invocation_error, true);
  const auto result = tool.run(&factory);
  if (diagnostics.had_error())
    throw std::invalid_argument("Clang reported an error while parsing prepared headers");
  if (!target_error.empty() || !invocation_error.empty() || result != 0)
    throw std::runtime_error("cannot parse prepared headers: " + target_error + invocation_error);
  return select_cpp_candidate(std::move(candidates), selector, name);
}

void verify_prepared_adapter(const std::string &name, const api::PreparedInjector &adapter,
                             const ResolvedCallable &original, bool pointer_layouts) {
  const auto selector = parse_selector(name);
  auto arguments = tracer::prepared::toolchain_arguments(adapter,
      {runtime::jit_flags.begin(), runtime::jit_flags.end()});
  arguments.insert(arguments.end(), {"-std=c++23", "-x", "c++",
      "-resource-dir=" + std::string(TRACER_CLANG_RESOURCE_DIR)});
  arguments.insert(arguments.end(), adapter.adapter_abi_options().begin(),
                   adapter.adapter_abi_options().end());
  clang::DiagnosticOptions diagnostic_options;
  PrepareDiagnostics diagnostics(diagnostic_options);
  auto unit = clang::tooling::buildASTFromCodeWithArgs(
      tracer::prepared::adapter_source(adapter), tracer::clang_driver_arguments(std::move(arguments)),
      "prepared-adapter.cc",
      "clang-tool", std::make_shared<clang::PCHContainerOperations>(),
      clang::tooling::getClangStripDependencyFileAdjuster(), {}, &diagnostics);
  // A compiler error must bypass the address-only pointer retry.
  if (diagnostics.had_error() || (unit && unit->getDiagnostics().hasErrorOccurred()))
    throw std::invalid_argument("Clang reported an error while compiling the prepared adapter");
  if (!unit)
    throw std::runtime_error("generated prepared adapter does not compile independently");
  tracer::prepared::check_toolchain_headers(unit->getPreprocessor().getHeaderSearchInfo(), adapter);
  std::vector<const clang::CXXRecordDecl *> snapshot_types;
  for (int i = 0; i < adapter.snapshot_types_size(); ++i) {
    const auto alias_name = "tracer_prepared::SnapshotType" + std::to_string(i);
    const auto *record = record_alias(unit->getASTContext(), alias_name);
    if (!record)
      throw std::runtime_error("prepared adapter is missing snapshot alias " + alias_name);
    api::DebugInfo actual;
    tracer::prepared::describe_record(unit->getASTContext(), *record, actual);
    if (actual.SerializeAsString() != adapter.snapshot_types(i).SerializeAsString())
      throw std::runtime_error(
          "generated prepared adapter does not reproduce snapshot layout " + alias_name +
          " (" + adapter.snapshot_types(i).record_name() + ", source bytes " +
          std::to_string(adapter.snapshot_types(i).record_size()) + ", adapter bytes " +
          std::to_string(actual.record_size()) + ")");
    snapshot_types.push_back(record);
  }
  std::set<std::uint32_t> snapshot_roots;
  const auto validate_dynamic_edge = [&](const clang::CXXRecordDecl *base,
                                         std::uint32_t type_index,
                                         bool require_polymorphic) {
    if (type_index >= snapshot_types.size())
      throw std::runtime_error("prepared snapshot policy has an out-of-range type index");
    const auto *candidate = snapshot_types[type_index];
    if (require_polymorphic && !base->isPolymorphic())
      throw std::runtime_error("prepared dynamic snapshot root is not polymorphic");
    if (candidate->getCanonicalDecl() == base->getCanonicalDecl())
      return;
    clang::CXXBasePaths paths;
    if (!candidate->isDerivedFrom(base, paths) ||
        paths.isAmbiguous(unit->getASTContext().getCanonicalType(
            tracer::prepared::record_type(unit->getASTContext(), base))) ||
        std::none_of(paths.begin(), paths.end(), [](const auto &path) {
          return path.Access == clang::AS_public;
        }))
      throw std::runtime_error("prepared snapshot policy has an incompatible dynamic type index");
  };
  std::vector<std::uint32_t> automatic_snapshot_roots;
  for (const auto &policy : adapter.snapshot_records()) {
    if (policy.type_index() >= snapshot_types.size() ||
        !snapshot_roots.insert(policy.type_index()).second)
      throw std::runtime_error("prepared snapshot root index is invalid or duplicated");
    const auto *root = snapshot_types[policy.type_index()];
    if (!policy.dynamic_types().empty() && !root->isPolymorphic())
      throw std::runtime_error("prepared dynamic snapshot root is not polymorphic");
    for (const auto type_index : policy.dynamic_types())
      validate_dynamic_edge(root, type_index, true);
    const auto validate_member = [&](const api::PreparedSnapshotMember &member,
                                     bool automatic) {
      if (member.field().empty())
        throw std::runtime_error("prepared snapshot member has an empty field name");
      const clang::FieldDecl *field = nullptr;
      for (const auto *candidate : root->fields())
        if (candidate->getNameAsString() == member.field()) {
          field = candidate;
          break;
        }
      if (member.erased_any()) {
        const auto *erased = field ? field->getType()->getAsCXXRecordDecl() : nullptr;
        if (automatic || !erased || erased->getQualifiedNameAsString() != "std::any" ||
            field->getType().isVolatileQualified() || member.dynamic_types().empty())
          throw std::runtime_error("prepared erased snapshot requires a direct std::any member and payloads");
        std::set<std::uint32_t> unique;
        for (const auto index : member.dynamic_types()) {
          if (index >= snapshot_types.size() || !unique.insert(index).second)
            throw std::runtime_error("prepared std::any payload index is invalid or duplicated");
          validate_snapshot_candidate(snapshot_types[index]);
        }
        return;
      }
      if (!field || field->getType().isVolatileQualified() ||
          !field->getType()->isPointerType() ||
          field->getType()->getPointeeType().isVolatileQualified())
        throw std::runtime_error("prepared snapshot member is not a direct raw record pointer");
      const auto *base = field->getType()->getPointeeType()->getAsCXXRecordDecl();
      base = base ? base->getDefinition() : nullptr;
      if (!base)
        throw std::runtime_error("prepared snapshot member pointee is incomplete");
      if (member.dynamic_types().empty())
        throw std::runtime_error("prepared snapshot member has no admitted pointee layouts");
      if (automatic && !base->isPolymorphic())
        throw std::runtime_error("automatic snapshot member pointee must be polymorphic");
      for (const auto type_index : member.dynamic_types()) {
        validate_dynamic_edge(base, type_index, base->isPolymorphic());
        if (automatic &&
            snapshot_types[type_index]->getCanonicalDecl() == base->getCanonicalDecl())
          throw std::runtime_error(
              "automatic snapshot member must have a concrete derived pointee layout");
        if (!base->isPolymorphic() &&
            snapshot_types[type_index]->getCanonicalDecl() != base->getCanonicalDecl())
          throw std::runtime_error(
              "non-polymorphic snapshot member must use its exact pointee type");
      }
    };
    if (!policy.automatic_members().empty()) {
      if (!policy.members().empty())
        throw std::runtime_error("prepared snapshot record mixes automatic and explicit members");
      automatic_snapshot_roots.push_back(policy.type_index());
    }
    for (const auto &member : policy.members())
      validate_member(member, false);
    for (const auto &member : policy.automatic_members())
      validate_member(member, true);
  }
  FrozenInvocation invocation;
  std::vector<Candidate> candidates;
  invocation.adapter_symbol = original.spec.symbol();
  invocation.pointer_layouts = pointer_layouts;
  Collector visitor(candidates, selector.qualified_name, std::move(invocation), "", "",
                    &unit->getSema());
  visitor.set_context(unit->getASTContext(), &unit->getSema());
  visitor.TraverseDecl(unit->getASTContext().getTranslationUnitDecl());
  if (diagnostics.had_error())
    throw std::invalid_argument("Clang reported an error while compiling the prepared adapter");
  // Match by linkage because the adapter writes canonical parameter types.
  const auto candidate = std::find_if(candidates.begin(), candidates.end(), [&](const auto &item) {
    return item.symbol == original.spec.symbol() && item.unsupported_reason.empty();
  });
  const std::string failure = "generated prepared adapter does not reproduce the target ABI and layout";
  if (candidate == candidates.end())
    throw std::runtime_error(failure + " for '" + name + "'");
  if (!automatic_snapshot_roots.empty()) {
    if (candidate->snapshot_result_root.empty())
      throw std::runtime_error("automatic snapshot members require a pointer or reference result record");
    for (const auto type_index : automatic_snapshot_roots)
      if (adapter.snapshot_types(static_cast<int>(type_index)).record_name() !=
          candidate->snapshot_result_root)
        throw std::runtime_error(
            "automatic prepared snapshot member is not on the result root record");
  }
  const auto &expected = original.spec.debug_info();
  const auto &actual = candidate->spec.debug_info();
  const auto &expected_signature = original.spec.signature();
  const auto &actual_signature = candidate->spec.signature();
  if (actual_signature.SerializeAsString() == expected_signature.SerializeAsString() &&
      actual.SerializeAsString() == expected.SerializeAsString())
    return;

  if (actual_signature.result() != expected_signature.result() ||
      actual.result_cpp_type() != expected.result_cpp_type() ||
      actual.result_is_reference() != expected.result_is_reference() ||
      actual.result_pass_by_reference() != expected.result_pass_by_reference() ||
      actual.result_object_type().SerializeAsString() !=
          expected.result_object_type().SerializeAsString())
    throw std::runtime_error(failure + " for result type '" + expected.result_cpp_type() + "'");
  for (int i = 0; i < std::min(expected.arguments_size(), actual.arguments_size()); ++i) {
    if (actual.arguments(i).SerializeAsString() != expected.arguments(i).SerializeAsString())
      throw std::runtime_error(failure + " for argument type '" +
                               expected.arguments(i).cpp_type() + "'");
  }
  if (!expected.record_name().empty() &&
      actual.prepared_layout_sha256() != expected.prepared_layout_sha256())
    throw std::runtime_error(failure + " for receiver type '" + expected.record_name() + "'");
  throw std::runtime_error(failure + " for '" + name + "'");
}

} // namespace tracer::client

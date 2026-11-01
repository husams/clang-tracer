#include "context_support_effects.hpp"

#include <algorithm>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <clang/AST/ASTContext.h>
#include <clang/AST/Attr.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/Stmt.h>
#include <clang/Basic/Linkage.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

#include "context_analysis.hpp"
#include "context_paths.hpp"

#ifndef TRACER_CONTEXT_SYSTEM_ROOTS
#define TRACER_CONTEXT_SYSTEM_ROOTS ""
#endif

namespace tracer {
namespace {
struct Edge {
  const clang::FunctionDecl *callee = nullptr;
  unsigned catch_depth = 0;
};

struct FunctionEffects {
  const clang::FunctionDecl *function = nullptr;
  std::vector<Edge> calls;
  unsigned local_catch_depth = 0;
  bool owns_or_escapes = false;
  bool may_throw = false;
  bool may_allocate = false;
  std::string invalid;
};

bool user_method(const clang::FunctionDecl *function) {
  if (!function || !function->isUserProvided() || !function->hasBody())
    return false;
  const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(function);
  return !method || !method->getParent()->getQualifiedNameAsString().starts_with("std::");
}

bool contains_exception_ptr_impl(
    clang::QualType type,
    std::unordered_set<const clang::CXXRecordDecl *> &seen) {
  if (type.isNull()) return false;
  type = type.getCanonicalType().getUnqualifiedType();
  if (type->isReferenceType())
    return contains_exception_ptr_impl(type->getPointeeType(), seen);
  if (type->isPointerType())
    return contains_exception_ptr_impl(type->getPointeeType(), seen);
  if (type->isArrayType())
    return contains_exception_ptr_impl(
        type->getAsArrayTypeUnsafe()->getElementType(), seen);
  const auto *record = type->getAsCXXRecordDecl();
  if (!record) return false;
  record = record->getCanonicalDecl();
  if (record->getQualifiedNameAsString().find("exception_ptr") !=
      std::string::npos)
    return true;
  if (!seen.insert(record).second) return false;
  if (const auto *special =
          llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record)) {
    const auto &arguments = special->getTemplateArgs();
    for (unsigned index = 0; index < arguments.size(); ++index) {
      const auto &argument = arguments[index];
      if (argument.getKind() == clang::TemplateArgument::Type &&
          contains_exception_ptr_impl(argument.getAsType(), seen))
        return true;
    }
    return false;
  }
  // Walk user records only; standard-library representation internals are
  // deliberately summarized by their template arguments above.
  if (record->getQualifiedNameAsString().starts_with("std::")) return false;
  if (const auto *definition = record->getDefinition())
    for (const auto *field : definition->fields())
      if (contains_exception_ptr_impl(field->getType(), seen)) return true;
  return false;
}

bool contains_exception_ptr(clang::QualType type) {
  std::unordered_set<const clang::CXXRecordDecl *> seen;
  return contains_exception_ptr_impl(type, seen);
}

std::string exception_ptr_error() {
  return "context_exception_retention_unsupported: exception_ptr can retain "
         "more live exception objects than the bounded native reserve";
}

void append_cleanup(clang::QualType type, FunctionEffects &effects,
                    unsigned catch_depth) {
  if (type->isReferenceType() || type->isPointerType()) return;
  type = type.getNonReferenceType();
  if (type->isArrayType()) type = type->getAsArrayTypeUnsafe()->getElementType();
  const auto *record = type->getAsCXXRecordDecl();
  if (!record) return;
  if (const auto *special =
          llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record)) {
    const auto name = special->getSpecializedTemplate()->getQualifiedNameAsString();
    const auto &args = special->getTemplateArgs();
    if (name == "std::vector" || name == "std::deque" ||
        name == "std::list" || name == "std::set" ||
        name == "std::unordered_set") {
      if (args.size() && args[0].getKind() == clang::TemplateArgument::Type)
        append_cleanup(args[0].getAsType(), effects, catch_depth);
    } else if (name == "std::map" || name == "std::unordered_map") {
      for (unsigned i = 0; i < std::min<unsigned>(2, args.size()); ++i)
        if (args[i].getKind() == clang::TemplateArgument::Type)
          append_cleanup(args[i].getAsType(), effects, catch_depth);
    }
  }
  const auto *destructor = record->getDestructor();
  if (user_method(destructor))
    effects.calls.push_back({destructor->getCanonicalDecl(), catch_depth});
}

void append_container_mutation(clang::CXXMemberCallExpr *call,
                               FunctionEffects &effects, unsigned depth) {
  const auto *method = call->getMethodDecl();
  if (!method || !call->getImplicitObjectArgument()) return;
  const auto method_name = method->getNameAsString();
  const auto type = call->getImplicitObjectArgument()->getType().getNonReferenceType();
  const auto *special = llvm::dyn_cast_or_null<
      clang::ClassTemplateSpecializationDecl>(type->getAsCXXRecordDecl());
  if (!special) return;
  const auto name = special->getSpecializedTemplate()->getQualifiedNameAsString();
  const bool sequence = name == "std::vector" || name == "std::deque" ||
      name == "std::list" || name == "std::set" || name == "std::unordered_set";
  const bool mapping = name == "std::map" || name == "std::unordered_map";
  if (!sequence && !mapping) return;
  const auto &args = special->getTemplateArgs();
  for (unsigned i = 0; i < (mapping ? 2u : 1u) && i < args.size(); ++i) {
    if (args[i].getKind() != clang::TemplateArgument::Type) continue;
    const auto *element = args[i].getAsType()->getAsCXXRecordDecl();
    if (!element) continue;
    const auto *destructor = element->getDestructor();
    if ((method_name == "clear" || method_name == "pop_back" ||
         method_name == "erase" || method_name == "resize") &&
        user_method(destructor))
      effects.calls.push_back({destructor->getCanonicalDecl(), depth});
    const bool constructs = method_name == "emplace" ||
        method_name == "emplace_back" || method_name == "push_back" ||
        method_name == "insert" || method_name == "try_emplace";
    if (constructs)
      for (const auto *constructor : element->ctors())
        if (user_method(constructor) &&
            constructor->getNumParams() == call->getNumArgs())
          effects.calls.push_back({constructor->getCanonicalDecl(), depth});
  }
}

class EffectVisitor final
    : public clang::RecursiveASTVisitor<EffectVisitor> {
public:
  EffectVisitor(clang::ASTContext &context, FunctionEffects &effects)
      : context_(context), effects_(effects) {}

  bool TraverseCXXTryStmt(clang::CXXTryStmt *statement) {
    if (!statement) return true;
    if (!TraverseStmt(statement->getTryBlock())) return false;
    for (unsigned i = 0; i < statement->getNumHandlers(); ++i) {
      ++catch_depth_;
      effects_.local_catch_depth =
          std::max(effects_.local_catch_depth, catch_depth_);
      if (!TraverseStmt(statement->getHandler(i))) return false;
      --catch_depth_;
    }
    return true;
  }

  bool VisitCXXThrowExpr(clang::CXXThrowExpr *statement) {
    effects_.may_throw = true;
    const auto *value = statement->getSubExpr();
    if (!value) return true;
    auto type = value->getType().getNonReferenceType();
    if (type->isIncompleteType()) {
      effects_.invalid = "context_exception_object_too_large: thrown object "
                         "has unknown size";
      return true;
    }
    const auto bytes = context_.getTypeSizeInChars(type).getQuantity();
    const auto alignment = context_.getTypeAlignInChars(type).getQuantity();
    if (alignment > 16) {
      effects_.invalid = "context_exception_alignment_exceeded: thrown object "
                         "alignment exceeds 16 bytes";
      return true;
    }
    if (bytes > 768) {
      effects_.invalid = "context_exception_object_too_large: thrown object "
                         "exceeds 768 bytes";
    }
    return true;
  }

  bool VisitCXXNewExpr(clang::CXXNewExpr *) {
    effects_.owns_or_escapes = true;
    effects_.may_allocate = true;
    return true;
  }

  bool VisitReturnStmt(clang::ReturnStmt *statement) {
    if (const auto *value = statement->getRetValue()) {
      const auto type = value->getType();
      if (type->isPointerType() || type->isReferenceType() ||
          type.getAsString().find("unique_ptr<") != std::string::npos ||
          type.getAsString().find("shared_ptr<") != std::string::npos)
        effects_.owns_or_escapes = true;
    }
    return true;
  }

  bool VisitCallExpr(clang::CallExpr *call) {
    const auto *callee = call->getDirectCallee();
    if (!callee) {
      effects_.invalid = "unsupported_context_effect: reachable indirect "
                         "call has no validated native target";
      return true;
    }
    if (contains_exception_ptr(callee->getReturnType()))
      effects_.invalid = exception_ptr_error();
    for (const auto *parameter : callee->parameters())
      if (contains_exception_ptr(parameter->getType()))
        effects_.invalid = exception_ptr_error();
    for (const auto *argument : call->arguments())
      if (contains_exception_ptr(argument->getType()))
        effects_.invalid = exception_ptr_error();
    const auto name = callee->getQualifiedNameAsString();
    const auto short_name = callee->getNameAsString();
    const bool standard_namespace = name.starts_with("std::");
    if ((standard_namespace &&
         (short_name == "terminate" || short_name == "abort" ||
          short_name == "set_new_handler" || short_name == "set_terminate")) ||
        name == "abort" || name == "::abort") {
      effects_.invalid = "unsupported_context_effect: reachable direct effect " + name;
    }
    if (name == "operator new" || name == "operator new[]" ||
        name == "malloc" || name == "calloc" || name == "realloc")
      effects_.owns_or_escapes = effects_.may_allocate = true;
    effects_.calls.push_back({callee->getCanonicalDecl(), catch_depth_});
    return true;
  }

  bool VisitVarDecl(clang::VarDecl *variable) {
    if (contains_exception_ptr(variable->getType()))
      effects_.invalid = exception_ptr_error();
    if (variable->isStaticLocal() && variable->hasInit() &&
        !variable->hasConstantInitialization())
      effects_.invalid =
          "context_dynamic_local_static_unsupported: reachable function has "
          "a dynamically initialized local static";
    if (variable->isLocalVarDeclOrParm() && !variable->hasGlobalStorage())
      append_cleanup(variable->getType(), effects_, catch_depth_);
    return true;
  }
  bool VisitDeclRefExpr(clang::DeclRefExpr *reference) {
    if (const auto *variable =
            llvm::dyn_cast<clang::ValueDecl>(reference->getDecl());
        variable && contains_exception_ptr(variable->getType()))
      effects_.invalid = exception_ptr_error();
    return true;
  }
  bool VisitCXXConstructExpr(clang::CXXConstructExpr *construction) {
    if (const auto *constructor = construction->getConstructor()) {
      if (user_method(constructor))
        effects_.calls.push_back(
            {constructor->getCanonicalDecl(), catch_depth_});
      const auto *function_type =
          constructor->getType()->getAs<clang::FunctionProtoType>();
      if (!function_type || !function_type->isNothrow())
        effects_.may_throw = true;
      const auto type_name =
          construction->getType().getCanonicalType().getAsString();
      if ((type_name.starts_with("std::basic_string<") ||
           type_name.starts_with("std::__1::basic_string<") ||
           type_name.starts_with("std::__cxx11::basic_string<")) &&
          construction->getNumArgs() != 0)
        effects_.may_allocate = true;
    }
    return true;
  }
  bool VisitCXXBindTemporaryExpr(clang::CXXBindTemporaryExpr *temporary) {
    append_cleanup(temporary->getType(), effects_, catch_depth_);
    return true;
  }
  bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *expression) {
    append_cleanup(expression->getDestroyedType(), effects_, catch_depth_);
    return true;
  }
  bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *call) {
    append_container_mutation(call, effects_, catch_depth_);
    const auto *method = call->getMethodDecl();
    if (method && method->getParent()->getQualifiedNameAsString().starts_with(
                      "std::") &&
        (method->getNameAsString() == "push_back" ||
         method->getNameAsString() == "emplace_back" ||
         method->getNameAsString() == "emplace" ||
         method->getNameAsString() == "insert" ||
         method->getNameAsString() == "try_emplace" ||
         method->getNameAsString() == "insert_or_assign" ||
         method->getNameAsString() == "append" ||
         method->getNameAsString() == "assign" ||
         method->getNameAsString() == "reserve" ||
         method->getNameAsString() == "resize" ||
         method->getNameAsString() == "operator+="))
      effects_.may_allocate = true;
    return true;
  }
  bool VisitGCCAsmStmt(clang::GCCAsmStmt *) {
    effects_.invalid = "unsupported_context_effect: user inline assembly is "
                       "not admitted in a reachable body";
    return true;
  }
  bool VisitMSAsmStmt(clang::MSAsmStmt *) {
    effects_.invalid = "unsupported_context_effect: user inline assembly is "
                       "not admitted in a reachable body";
    return true;
  }

private:
  clang::ASTContext &context_;
  FunctionEffects &effects_;
  unsigned catch_depth_ = 0;
};

bool user_source_location(const clang::SourceManager &sources,
                          clang::SourceLocation location) {
  if (!location.isValid()) return false;
  location = sources.getSpellingLoc(location);
  const auto filename = sources.getFilename(location);
  if (filename.empty()) return true;
  llvm::SmallString<256> physical;
  // Header search classification (-isystem, #pragma GCC system_header) is
  // not provenance: users can place project declarations there. Trust only
  // headers physically rooted in the configured compiler/toolchain roots.
  if (llvm::sys::fs::real_path(filename, physical)) return true;

  llvm::StringRef roots(TRACER_CONTEXT_SYSTEM_ROOTS);
  while (!roots.empty()) {
    const auto pair = roots.split('|');
    roots = pair.second;
    if (pair.first.empty()) continue;
    llvm::SmallString<256> root;
    if (!llvm::sys::fs::real_path(pair.first, root) &&
        llvm::StringRef(physical).starts_with(llvm::StringRef(root).str() + "/"))
      return false;
  }

  try {
    llvm::SmallString<256> engine;
    if (!llvm::sys::fs::real_path(context_header_directory(), engine)) {
      const llvm::StringRef path(physical);
      if (path == llvm::StringRef(engine).str() + "/context_facade.hpp" ||
          path == llvm::StringRef(engine).str() + "/context_codec.hpp" ||
          path == llvm::StringRef(engine).str() + "/context_analysis.hpp" ||
          path == llvm::StringRef(engine).str() + "/context_execution.hpp" ||
          path == llvm::StringRef(engine).str() + "/context_hooks.hpp" ||
          path == llvm::StringRef(engine).str() + "/context_lifecycle.hpp")
        return false;
      llvm::SmallString<256> callback;
      if (!llvm::sys::fs::real_path(
              llvm::StringRef(engine).str() + "/../include/tracer/callback.hpp",
              callback) &&
          llvm::StringRef(physical) == llvm::StringRef(callback))
        return false;
      if (!llvm::sys::fs::real_path(
              llvm::StringRef(engine).str() + "/tracer/callback.hpp",
              callback) &&
          llvm::StringRef(physical) == llvm::StringRef(callback))
        return false;
    }
  } catch (...) {
    // If installed compiler headers cannot be located, treat the declaration
    // as user code so reachable effects remain subject to validation.
  }
  return true;
}

void collect_nested_user_records(
    clang::QualType type,
    std::vector<const clang::CXXRecordDecl *> &records,
    std::unordered_set<const clang::CXXRecordDecl *> &seen) {
  if (type.isNull()) return;
  type = type.getNonReferenceType().getUnqualifiedType();
  if (type->isPointerType() || type->isMemberPointerType()) return;
  if (type->isArrayType()) {
    collect_nested_user_records(
        type->getAsArrayTypeUnsafe()->getElementType(), records, seen);
    return;
  }
  const auto *record = type->getAsCXXRecordDecl();
  if (!record) return;
  record = record->getCanonicalDecl();
  const auto *definition = record->getDefinition();
  if (!definition || !seen.insert(record).second) return;
  const auto name = record->getQualifiedNameAsString();
  const bool standard = name.starts_with("std::") ||
      name.starts_with("__gnu_cxx::") || name.starts_with("__cxxabiv1::");
  if (!standard) records.push_back(definition);
  if (const auto *special =
          llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record)) {
    const auto &arguments = special->getTemplateArgs();
    for (unsigned index = 0; index < arguments.size(); ++index)
      if (arguments[index].getKind() == clang::TemplateArgument::Type)
        collect_nested_user_records(arguments[index].getAsType(), records,
                                    seen);
    if (standard) return;
  }
  if (standard) return;
  for (const auto *field : definition->fields())
    collect_nested_user_records(field->getType(), records, seen);
  for (const auto &base : definition->bases())
    collect_nested_user_records(base.getType(), records, seen);
}

bool reserved_native_identity(std::string_view name) {
  return name.starts_with("std::") || name.starts_with("::std::") ||
      name.starts_with("_Unwind_") || name.starts_with("__cxa_") ||
      name == "operator new" || name == "operator new[]" ||
      name == "operator delete" || name == "operator delete[]";
}

class NativeDeclarationGuard final
    : public clang::RecursiveASTVisitor<NativeDeclarationGuard> {
public:
  explicit NativeDeclarationGuard(clang::SourceManager &sources)
      : sources_(sources) {}

  bool VisitDecl(clang::Decl *decl) {
    if (!decl || !user_source_location(sources_, decl->getLocation()))
      return true;
    for (const auto *attribute : decl->attrs()) {
      const std::string_view spelling = attribute->getSpelling();
      if (spelling == "asm" || spelling == "alias" || spelling == "ifunc" ||
          spelling == "section" || spelling == "target" ||
          spelling == "target_clones") {
        error_ = "unsupported_context_native_declaration: user " +
                 std::string(spelling) + " attribute is not admitted";
        return true;
      }
    }
    if (const auto *function = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
      if (function->getFormalLinkage() != clang::Linkage::None &&
          reserved_native_identity(function->getQualifiedNameAsString()))
        error_ = "unsupported_context_native_declaration: user declaration " +
                 function->getQualifiedNameAsString() +
                 " collides with a reserved runtime identity";
    } else if (const auto *variable = llvm::dyn_cast<clang::VarDecl>(decl)) {
      if (variable->hasExternalStorage() &&
          reserved_native_identity(variable->getQualifiedNameAsString()))
        error_ = "unsupported_context_native_declaration: user declaration " +
                 variable->getQualifiedNameAsString() +
                 " collides with a reserved runtime identity";
    }
    return true;
  }

  bool VisitGCCAsmStmt(clang::GCCAsmStmt *statement) {
    if (user_source_location(sources_, statement->getBeginLoc()))
      error_ = "unsupported_context_native_declaration: user inline assembly "
               "is not admitted";
    return true;
  }
  bool VisitMSAsmStmt(clang::MSAsmStmt *statement) {
    if (user_source_location(sources_, statement->getBeginLoc()))
      error_ = "unsupported_context_native_declaration: user inline assembly "
               "is not admitted";
    return true;
  }

  const std::string &error() const noexcept { return error_; }

private:
  clang::SourceManager &sources_;
  std::string error_;
};

class FunctionCollector final
    : public clang::RecursiveASTVisitor<FunctionCollector> {
public:
  FunctionCollector(clang::SourceManager &sources,
                    std::vector<const clang::FunctionDecl *> &out)
      : sources_(sources), out_(out) {}
  bool VisitFunctionDecl(clang::FunctionDecl *function) {
    if (function->hasBody() && !function->isImplicit() &&
        function->getLocation().isValid() &&
        user_source_location(sources_, function->getLocation()) &&
        seen_.insert(function->getCanonicalDecl()).second)
      out_.push_back(function);
    return true;
  }
private:
  clang::SourceManager &sources_;
  std::vector<const clang::FunctionDecl *> &out_;
  std::unordered_set<const clang::FunctionDecl *> seen_;
};

class RecordCollector final
    : public clang::RecursiveASTVisitor<RecordCollector> {
public:
  RecordCollector(clang::SourceManager &sources,
                  std::vector<const clang::CXXRecordDecl *> &out)
      : sources_(sources), out_(out) {}
  bool VisitCXXRecordDecl(clang::CXXRecordDecl *record) {
    if (record->isThisDeclarationADefinition() && !record->isImplicit() &&
        record->getLocation().isValid() &&
        user_source_location(sources_, record->getLocation()))
      out_.push_back(record);
    return true;
  }
private:
  clang::SourceManager &sources_;
  std::vector<const clang::CXXRecordDecl *> &out_;
};

std::string validate(clang::ASTUnit &unit) {
  auto &context = unit.getASTContext();
  const auto declarations = analyze_context_declarations(context);
  if (!declarations.error.empty()) return declarations.error;
  NativeDeclarationGuard native_declarations(context.getSourceManager());
  native_declarations.TraverseDecl(context.getTranslationUnitDecl());
  if (!native_declarations.error().empty()) return native_declarations.error();
  std::vector<const clang::FunctionDecl *> functions;
  std::vector<const clang::FunctionDecl *> roots;
  FunctionCollector collector(context.getSourceManager(), functions);
  collector.TraverseDecl(context.getTranslationUnitDecl());

  std::unordered_map<const clang::FunctionDecl *, FunctionEffects> graph;
  std::string error;
  for (const auto *function : functions) {
    auto *definition = function->getDefinition();
    if (!definition || !definition->hasBody()) continue;
    auto &effects = graph[function->getCanonicalDecl()];
    effects.function = function->getCanonicalDecl();
    EffectVisitor visitor(context, effects);
    visitor.TraverseStmt(const_cast<clang::Stmt *>(definition->getBody()));
    if (contains_exception_ptr(function->getReturnType()))
      effects.invalid = exception_ptr_error();
    for (const auto *parameter : function->parameters())
      if (contains_exception_ptr(parameter->getType()))
        effects.invalid = exception_ptr_error();
  }

  std::unordered_set<std::string> state_types;
  for (const auto &state : declarations.states)
    state_types.insert(state.state_type);
  std::vector<const clang::CXXRecordDecl *> records;
  RecordCollector record_collector(context.getSourceManager(), records);
  record_collector.TraverseDecl(context.getTranslationUnitDecl());
  std::vector<const clang::CXXRecordDecl *> payload_records;
  std::unordered_set<const clang::CXXRecordDecl *> payload_seen;
  for (const auto *record : records)
    if (state_types.contains(record->getQualifiedNameAsString()))
      collect_nested_user_records(
          context.getTypeDeclType(static_cast<const clang::TypeDecl *>(record)),
          payload_records, payload_seen);
  for (const auto *record : records) {
    if (!payload_seen.contains(record->getCanonicalDecl())) continue;
    for (const auto *constructor : record->ctors())
      if (constructor->hasBody()) roots.push_back(constructor->getCanonicalDecl());
    if (const auto *destructor = record->getDestructor();
        destructor && destructor->hasBody())
      roots.push_back(destructor->getCanonicalDecl());
  }
  for (const auto &class_plan : declarations.classes)
    for (const auto &hook : class_plan.hooks)
      for (const auto *function : functions)
        if (const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(function);
            method && method->getParent()->getQualifiedNameAsString() ==
                          class_plan.type &&
            method->getNameAsString() == hook.method)
          roots.push_back(function->getCanonicalDecl());

  std::unordered_set<const clang::FunctionDecl *> reachable;
  std::vector<const clang::FunctionDecl *> pending = roots;
  while (!pending.empty()) {
    const auto *function = pending.back();
    pending.pop_back();
    if (!reachable.insert(function->getCanonicalDecl()).second) continue;
    const auto it = graph.find(function->getCanonicalDecl());
    if (it == graph.end()) continue;
    if (!it->second.invalid.empty()) return it->second.invalid;
    for (const auto &edge : it->second.calls) pending.push_back(edge.callee);
  }

  // The graph walk bounds simultaneously active user catch handlers. Cycles
  // are rejected when any edge carries an active catch or the cycle owns or
  // returns a pointer-like object, since either can consume unbounded reserve.
  std::unordered_set<const clang::FunctionDecl *> active;
  std::function<unsigned(const clang::FunctionDecl *, unsigned, bool)> visit =
      [&](const clang::FunctionDecl *function, unsigned parent_depth,
          bool path_owns) -> unsigned {
    auto found = graph.find(function);
    if (found == graph.end()) return parent_depth;
    const auto &effects = found->second;
    path_owns |= effects.owns_or_escapes;
    if (active.contains(function)) {
      if (parent_depth > 0 || path_owns)
        error = "context_exception_capacity_unbounded: recursive call path "
                "has active catches or owning/pointer escape";
      return parent_depth;
    }
    active.insert(function);
    unsigned maximum = parent_depth + effects.local_catch_depth;
    for (const auto &edge : effects.calls) {
      const auto depth = parent_depth + edge.catch_depth;
      maximum = std::max(maximum, visit(edge.callee, depth, path_owns));
      if (!error.empty()) break;
    }
    active.erase(function);
    return maximum;
  };
  for (const auto *function : reachable) {
    if (visit(function, 0, false) > 16)
      return "context_exception_capacity_exceeded: transitive catch depth "
             "exceeds 16";
    if (!error.empty()) return error;
  }

  for (const auto *cxx : payload_records) {
    const auto *destructor = cxx->getDestructor();
    const auto *destructor_type = destructor
        ? destructor->getType()->getAs<clang::FunctionProtoType>()
        : nullptr;
    if (!destructor || !destructor->hasBody() || !destructor_type ||
        !destructor_type->isNothrow())
      continue;
    const auto found = graph.find(destructor->getCanonicalDecl());
    if (found == graph.end()) continue;
    // A noexcept user destructor that throws, allocates, calls a throwing
    // user function, or invokes an opaque function cannot be safely drained.
    std::unordered_set<const clang::FunctionDecl *> seen;
    std::function<bool(const clang::FunctionDecl *)> unsafe =
        [&](const clang::FunctionDecl *function) {
          if (!seen.insert(function).second) return false;
          const auto it = graph.find(function->getCanonicalDecl());
          if (it == graph.end()) return true;
          const auto &effects = it->second;
          if (effects.owns_or_escapes || effects.may_throw ||
              effects.may_allocate || !effects.invalid.empty())
            return true;
          for (const auto &edge : effects.calls) {
            const auto *function_type =
                edge.callee->getType()->getAs<clang::FunctionProtoType>();
            if (!function_type || !function_type->isNothrow())
              return true;
            if (edge.callee->isUserProvided() && unsafe(edge.callee)) return true;
          }
          return false;
        };
    if (unsafe(destructor->getCanonicalDecl()))
      return "context_noexcept_destructor_effect: state destructor may throw "
             "or allocate";
  }
  for (const auto *function : reachable) {
    if (!graph.contains(function->getCanonicalDecl())) continue;
    const auto *function_type =
        function->getType()->getAs<clang::FunctionProtoType>();
    if (!function_type || !function_type->isNothrow()) continue;
    std::unordered_set<const clang::FunctionDecl *> seen;
    std::function<bool(const clang::FunctionDecl *)> unsafe =
        [&](const clang::FunctionDecl *candidate) {
          if (!seen.insert(candidate->getCanonicalDecl()).second)
            return false;
          const auto it = graph.find(candidate->getCanonicalDecl());
          if (it == graph.end()) return true;
          const auto &effects = it->second;
          if (effects.owns_or_escapes || effects.may_throw ||
              effects.may_allocate || !effects.invalid.empty())
            return true;
          for (const auto &edge : effects.calls) {
            const auto *callee_type =
                edge.callee->getType()->getAs<clang::FunctionProtoType>();
            if (!callee_type || !callee_type->isNothrow()) return true;
            if (edge.callee->isUserProvided() && unsafe(edge.callee))
              return true;
          }
          return false;
        };
    if (unsafe(function))
      return "context_noexcept_function_effect: reachable noexcept function "
             "may throw, allocate, or invoke an unproven effect";
  }
  return {};
}
} // namespace

bool context_source_is_trusted_support(const clang::SourceManager &sources,
                                       clang::SourceLocation location) {
  return location.isValid() && !user_source_location(sources, location);
}

std::string validate_context_support_effects(clang::ASTUnit &unit) {
  return validate(unit);
}
} // namespace tracer

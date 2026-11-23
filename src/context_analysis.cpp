#include "context_analysis.hpp"

#include <algorithm>
#include <map>
#include <set>

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/StmtCXX.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Basic/Version.h>
#include <clang/Lex/Lexer.h>

#include "context_support_effects.hpp"
#include "tracer/compat.hpp"

namespace tracer {
std::string context_class_type_name(const clang::CXXRecordDecl &record) {
  if (!llvm::isa<clang::ClassTemplateSpecializationDecl>(record))
    return record.getQualifiedNameAsString();
  auto &context = record.getASTContext();
  clang::PrintingPolicy policy(context.getLangOpts());
  policy.SuppressTagKeyword = true;
  policy.SuppressDefaultTemplateArgs = false;
  policy.SuppressInlineNamespace = false;
#if CLANG_VERSION_MAJOR >= 22
  return context.getCanonicalTagType(&record).getAsString(policy);
#else
  return context.getRecordType(&record).getAsString(policy);
#endif
}

namespace {
const clang::ClassTemplateSpecializationDecl *
specialization(clang::QualType type, const char *name) {
  if (type.isNull() || type->isDependentType()) return nullptr;
  const auto *record = type.getNonReferenceType()->getAsCXXRecordDecl();
  const auto *special =
      llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(record);
  return special && special->getSpecializedTemplate()
                            ->getQualifiedNameAsString() == name
             ? special
             : nullptr;
}

clang::QualType
argument(const clang::ClassTemplateSpecializationDecl &special) {
  const auto &args = special.getTemplateArgs();
  return args.size() == 1 && args[0].getKind() == clang::TemplateArgument::Type
             ? args[0].getAsType()
             : clang::QualType{};
}

std::string canonical(clang::QualType type) {
  if (type.isNull()) return {};
  if (const auto *record = type.getNonReferenceType()->getAsCXXRecordDecl()) {
    return record->getQualifiedNameAsString();
  }
  return type.getCanonicalType().getAsString();
}

std::string canonical_with_template_arguments(clang::QualType type) {
  if (type.isNull()) return {};
  type = type.getNonReferenceType().getCanonicalType();
  const auto *record = type->getAsCXXRecordDecl();
  if (record &&
      llvm::isa<clang::ClassTemplateSpecializationDecl>(record))
    return type.getAsString();
  return canonical(type);
}

void markers(const clang::CXXRecordDecl *record,
             std::vector<ContextDependency> &found, unsigned depth = 0) {
  if (!record || !record->hasDefinition() || depth > 32) {
    return;
  }
  record = record->getDefinition();
  for (const auto &base : record->bases()) {
    if (base.getType().isNull() || base.getType()->isDependentType()) continue;
    for (const auto *name : {"tracer::state", "tracer::shared_state"}) {
      if (const auto *special = specialization(base.getType(), name)) {
        auto scope = argument(*special);
        if (!scope.isNull()) {
          const auto scope_name = canonical(scope);
          const bool shared = std::string(name) == "tracer::shared_state";
          const auto existing = std::find_if(
              found.begin(), found.end(), [&](const ContextDependency &value) {
                return value.scope_type == scope_name;
              });
          if (existing == found.end())
            found.push_back({{}, scope_name, shared, {}});
          else
            existing->shared |= shared;
        }
      }
    }
    markers(base.getType()->getAsCXXRecordDecl(), found, depth + 1);
  }
}

void unique_add(std::vector<std::string> &types, std::string type) {
  if (std::find(types.begin(), types.end(), type) == types.end()) {
    types.push_back(std::move(type));
  }
}

void unique_add_context_dependency(std::vector<ContextDependency> &states,
                                  ContextDependency state) {
  const auto found = std::find_if(
      states.begin(), states.end(), [&](const ContextDependency &existing) {
        if (existing.generic != state.generic) return false;
        if (state.generic)
          return existing.scope_type == state.scope_type &&
                 existing.payload_type == state.payload_type;
        return existing.state_type == state.state_type;
      });
  if (found == states.end()) states.push_back(std::move(state));
}

void unique_add_generic(std::vector<ContextDependency> &accesses,
                        ContextDependency access) {
  if (std::none_of(accesses.begin(), accesses.end(),
                   [&](const ContextDependency &existing) {
                     return existing.scope_type == access.scope_type &&
                            existing.payload_type == access.payload_type;
                   }))
    accesses.push_back(std::move(access));
}

void append_cleanup(clang::QualType type,
                    std::vector<const clang::FunctionDecl *> &calls) {
  if (type.isNull() || type->isDependentType()) return;
  if (type->isReferenceType() || type->isPointerType()) return;
  type = type.getNonReferenceType();
  if (type->isArrayType())
    type = type->getAsArrayTypeUnsafe()->getElementType();
  const auto *record = type->getAsCXXRecordDecl();
  if (const auto *special =
          llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(record)) {
    const auto name = special->getSpecializedTemplate()->getQualifiedNameAsString();
    const auto &arguments = special->getTemplateArgs();
    if (name == "std::vector" || name == "std::deque" ||
        name == "std::list" || name == "std::set" ||
        name == "std::unordered_set" || name == "std::array" ||
        name == "std::optional" || name == "std::unique_ptr" ||
        name == "std::shared_ptr") {
      if (arguments.size() && arguments[0].getKind() ==
                                  clang::TemplateArgument::Type)
        append_cleanup(arguments[0].getAsType(), calls);
    } else if (name == "std::map" || name == "std::unordered_map") {
      for (unsigned i = 0; i < std::min<unsigned>(2, arguments.size()); ++i)
        if (arguments[i].getKind() == clang::TemplateArgument::Type)
          append_cleanup(arguments[i].getAsType(), calls);
    }
  }
  const auto *destructor = record ? record->getDestructor() : nullptr;
  const auto record_name = record ? record->getQualifiedNameAsString() : "";
  if (destructor && destructor->isUserProvided() && destructor->hasBody() &&
      !::tracer::compat::starts_with(record_name, "std::")) {
    calls.push_back(destructor);
  }
}

bool user_provided_type_method(const clang::FunctionDecl *function) {
  if (!function || !function->isUserProvided() || !function->hasBody())
    return false;
  const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(function);
  return !method ||
         !::tracer::compat::starts_with(method->getParent()->getQualifiedNameAsString(), "std::");
}

void append_user_operator(const clang::CXXRecordDecl *record,
                          const char *operator_name,
                          std::vector<const clang::FunctionDecl *> &calls) {
  if (!record) return;
  for (const auto *method : record->methods())
    if (method->getNameAsString() == operator_name &&
        user_provided_type_method(method))
      calls.push_back(method);
  const clang::DeclContext *context = record->getDeclContext();
  while (context && !context->isNamespace() && !context->isTranslationUnit())
    context = context->getParent();
  if (!context) return;
  for (const auto *declaration : context->decls()) {
    const auto *function = llvm::dyn_cast<clang::FunctionDecl>(declaration);
    if (!function || function->getNameAsString() != operator_name ||
        function->getNumParams() != 2 || !user_provided_type_method(function))
      continue;
    bool matches = true;
    for (unsigned i = 0; i < 2; ++i) {
      const auto *parameter_record =
          function->getParamDecl(i)->getType().getNonReferenceType()
              ->getAsCXXRecordDecl();
      matches &= parameter_record &&
                 parameter_record->getCanonicalDecl() == record->getCanonicalDecl();
    }
    if (matches) calls.push_back(function);
  }
}

class TrustedStandardNamespaces final
    : public clang::RecursiveASTVisitor<TrustedStandardNamespaces> {
public:
  explicit TrustedStandardNamespaces(clang::SourceManager &sources)
      : sources_(sources) {}
  bool VisitNamespaceDecl(clang::NamespaceDecl *space) {
    const auto name = space->getNameAsString();
    if ((name == "std" || name == "__gnu_cxx" || name == "__cxxabiv1") &&
        !sources_.isInSystemHeader(space->getLocation())) {
      error = "unsupported_context_effect: source declaration in protected "
              "standard namespace " + name;
      return false;
    }
    return true;
  }
  bool VisitNamedDecl(clang::NamedDecl *declaration) {
    const auto qualified = declaration->getQualifiedNameAsString();
    const bool protected_namespace = ::tracer::compat::starts_with(qualified, "std::") ||
                                     ::tracer::compat::starts_with(qualified, "__gnu_cxx::") ||
                                     ::tracer::compat::starts_with(qualified, "__cxxabiv1::");
    if (protected_namespace && declaration->getLocation().isValid() &&
        !sources_.isInSystemHeader(declaration->getLocation())) {
      error = "unsupported_context_effect: source declaration in protected "
              "standard namespace " + qualified;
      return false;
    }
    return true;
  }
  std::string error;
private:
  clang::SourceManager &sources_;
};

bool append_container_mutation(
    clang::CXXMemberCallExpr *,
    std::vector<const clang::FunctionDecl *> &);

class ContextUseScanner final
    : public clang::RecursiveASTVisitor<ContextUseScanner> {
public:
  explicit ContextUseScanner(bool collect_calls = false)
      : collect_calls_(collect_calls) {}
  bool TraverseFunctionDecl(clang::FunctionDecl *function) {
    if (function) {
      const auto name = function->getQualifiedNameAsString();
      if (::tracer::compat::starts_with(name, "std::") ||
          ::tracer::compat::starts_with(name, "__gnu_cxx::") ||
          ::tracer::compat::starts_with(name, "__cxxabiv1::") ||
          ::tracer::compat::starts_with(name, "tracer::Call::")) {
        return true;
      }
    }
    return clang::RecursiveASTVisitor<ContextUseScanner>::
        TraverseFunctionDecl(function);
  }
  bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *call) {
    const auto *method = call->getMethodDecl();
    uses_context |= method && method->getParent() &&
        method->getParent()->getQualifiedNameAsString() == "tracer::Context";
    uses_stack_capture |= method && method->getParent() &&
        method->getParent()->getQualifiedNameAsString() == "tracer::Call" &&
        method->getNameAsString() == "capture_stack";
    if (collect_calls_)
      (void)append_container_mutation(call, callees);
    if (collect_calls_)
      if (const auto *callee = call->getDirectCallee())
        callees.push_back(callee);
    return true;
  }
  bool VisitCallExpr(clang::CallExpr *call) {
    if (collect_calls_ && !llvm::isa<clang::CXXMemberCallExpr>(call))
      if (const auto *callee = call->getDirectCallee())
        callees.push_back(callee);
    return true;
  }
  bool VisitVarDecl(clang::VarDecl *variable) {
    if (collect_calls_ && variable->isLocalVarDeclOrParm() &&
        !variable->hasGlobalStorage())
      append_cleanup(variable->getType(), callees);
    return true;
  }
  bool VisitCXXConstructExpr(clang::CXXConstructExpr *construction) {
    if (collect_calls_ &&
        user_provided_type_method(construction->getConstructor()))
      callees.push_back(construction->getConstructor());
    return true;
  }
  bool VisitCXXBindTemporaryExpr(clang::CXXBindTemporaryExpr *temporary) {
    if (collect_calls_ && temporary->getTemporary() &&
        user_provided_type_method(temporary->getTemporary()->getDestructor()))
      callees.push_back(temporary->getTemporary()->getDestructor());
    return true;
  }
  bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *expression) {
    if (!collect_calls_) return true;
    const auto destroyed = expression->getDestroyedType();
    if (destroyed.isNull() || destroyed->isDependentType()) return true;
    const auto *record = destroyed->getAsCXXRecordDecl();
    if (record)
      if (const auto *destructor = record->getDestructor(); destructor &&
          user_provided_type_method(destructor))
        callees.push_back(destructor);
    return true;
  }
  bool uses_context = false;
  bool uses_stack_capture = false;
  std::vector<const clang::FunctionDecl *> callees;
private:
  bool collect_calls_;
};

class ContextRequirementScanner final
    : public clang::RecursiveASTVisitor<ContextRequirementScanner> {
public:
  bool VisitCXXConstructorDecl(clang::CXXConstructorDecl *constructor) {
    for (const auto *parameter : constructor->parameters())
      if (specialization(parameter->getType(), "tracer::State")) {
        required = true;
        return false;
      }
    return true;
  }
  bool VisitCXXMethodDecl(clang::CXXMethodDecl *method) {
    if (method->getNameAsString() != "pre" &&
        method->getNameAsString() != "post")
      return true;
    for (unsigned i = 1; i < method->getNumParams(); ++i) {
      std::vector<ContextDependency> injected;
      markers(method->getParamDecl(i)->getType().getNonReferenceType()
                  ->getAsCXXRecordDecl(),
              injected);
      if (injected.size() == 1) {
        required = true;
        return false;
      }
    }
    return true;
  }
  bool required = false;
};

bool function_reaches_context_use(
    const clang::FunctionDecl *function,
    std::vector<const clang::FunctionDecl *> &visited) {
  if (!function || !function->hasBody()) return false;
  const auto *canonical = function->getCanonicalDecl();
  if (std::find(visited.begin(), visited.end(), canonical) != visited.end())
    return false;
  visited.push_back(canonical);
  ContextUseScanner scanner(true);
  scanner.TraverseStmt(const_cast<clang::Stmt *>(function->getBody()));
  if (scanner.uses_context) return true;
  for (const auto *callee : scanner.callees) {
    const auto name = callee->getQualifiedNameAsString();
    if (::tracer::compat::starts_with(name, "std::") ||
        ::tracer::compat::starts_with(name, "__gnu_cxx::") ||
        ::tracer::compat::starts_with(name, "__cxxabiv1::")) {
      continue;
    }
    if (callee->getLocation().isValid() &&
        function_reaches_context_use(callee, visited))
      return true;
  }
  return false;
}

bool function_reaches_stack_capture(
    const clang::FunctionDecl *function,
    std::vector<const clang::FunctionDecl *> &visited) {
  if (!function || !function->hasBody()) return false;
  const auto *canonical = function->getCanonicalDecl();
  if (std::find(visited.begin(), visited.end(), canonical) != visited.end())
    return false;
  visited.push_back(canonical);
  ContextUseScanner scanner(true);
  scanner.TraverseStmt(const_cast<clang::Stmt *>(function->getBody()));
  if (scanner.uses_stack_capture) return true;
  for (const auto *callee : scanner.callees) {
    const auto name = callee->getQualifiedNameAsString();
    if (::tracer::compat::starts_with(name, "std::") ||
        ::tracer::compat::starts_with(name, "__gnu_cxx::") ||
        ::tracer::compat::starts_with(name, "__cxxabiv1::")) {
      continue;
    }
    if (callee->getLocation().isValid() &&
        function_reaches_stack_capture(callee, visited))
      return true;
  }
  return false;
}

class GenericPayloadStackScan final
    : public clang::RecursiveASTVisitor<GenericPayloadStackScan> {
public:
  explicit GenericPayloadStackScan(
      clang::ASTContext &context,
      const std::vector<std::string> &payload_types)
      : context_(context), payload_types_(payload_types) {}
  bool TraverseFunctionDecl(clang::FunctionDecl *function) {
    if (function) {
      const auto name = function->getQualifiedNameAsString();
      if (::tracer::compat::starts_with(name, "std::") ||
          ::tracer::compat::starts_with(name, "__gnu_cxx::") ||
          ::tracer::compat::starts_with(name, "__cxxabiv1::") ||
          ::tracer::compat::starts_with(name, "tracer::Call::")) {
        return true;
      }
    }
    return clang::RecursiveASTVisitor<GenericPayloadStackScan>::
        TraverseFunctionDecl(function);
  }
  bool VisitCXXRecordDecl(clang::CXXRecordDecl *record) {
    if (!record->isThisDeclarationADefinition() || record->isImplicit())
      return true;
    const auto type = canonical_with_template_arguments(
#if CLANG_VERSION_MAJOR >= 22
        clang::QualType(context_.getCanonicalTagType(record).getTypePtr(), 0));
#else
        clang::QualType(record->getTypeForDecl(), 0));
#endif
    if (std::find(payload_types_.begin(), payload_types_.end(), type) ==
        payload_types_.end())
      return true;
    for (const auto *constructor : record->ctors()) {
      std::vector<const clang::FunctionDecl *> visited;
      requires_stack_capture |=
          function_reaches_stack_capture(constructor, visited);
    }
    return true;
  }
  bool requires_stack_capture = false;
private:
  clang::ASTContext &context_;
  const std::vector<std::string> &payload_types_;
};

bool append_container_mutation(clang::CXXMemberCallExpr *call,
                               std::vector<const clang::FunctionDecl *> &calls) {
  const auto *method = call->getMethodDecl();
  if (!method || !call->getImplicitObjectArgument()) return true;
  const auto name = method->getNameAsString();
  auto type = call->getImplicitObjectArgument()->getType().getNonReferenceType();
  if (type.isNull() || type->isDependentType()) return true;
  const auto *special = llvm::dyn_cast_or_null<
      clang::ClassTemplateSpecializationDecl>(type->getAsCXXRecordDecl());
  if (!special) return true;
  const auto container = special->getSpecializedTemplate()->getQualifiedNameAsString();
  const bool sequence = container == "std::vector" || container == "std::deque" ||
      container == "std::list" || container == "std::set" ||
      container == "std::multiset" || container == "std::unordered_set" ||
      container == "std::unordered_multiset";
  const bool mapping = container == "std::map" || container == "std::multimap" ||
      container == "std::unordered_map" || container == "std::unordered_multimap";
  const bool ordered = container == "std::set" || container == "std::multiset" ||
      container == "std::map" || container == "std::multimap";
  const bool unordered = container == "std::unordered_set" ||
      container == "std::unordered_multiset" || container == "std::unordered_map" ||
      container == "std::unordered_multimap";
  if (!sequence && !mapping) return true;
  std::vector<clang::QualType> elements;
  const auto &arguments = special->getTemplateArgs();
  for (unsigned i = 0; i < (mapping ? 2u : 1u) && i < arguments.size(); ++i)
    if (arguments[i].getKind() == clang::TemplateArgument::Type &&
        !arguments[i].getAsType().isNull() &&
        !arguments[i].getAsType()->isDependentType())
      elements.push_back(arguments[i].getAsType());
  const unsigned allocator_index = container == "std::vector" ||
      container == "std::deque" || container == "std::list" ? 1u :
      container == "std::map" || container == "std::multimap" ? 3u :
      container == "std::unordered_map" || container == "std::unordered_multimap" ? 4u :
      container == "std::unordered_set" || container == "std::unordered_multiset" ? 3u :
      container == "std::set" || container == "std::multiset" ? 2u : 2u;
  if (allocator_index < arguments.size() &&
      arguments[allocator_index].getKind() == clang::TemplateArgument::Type) {
    if (arguments[allocator_index].getAsType().isNull() ||
        arguments[allocator_index].getAsType()->isDependentType())
      return true;
    const auto *allocator = llvm::dyn_cast_or_null<
        clang::ClassTemplateSpecializationDecl>(
            arguments[allocator_index].getAsType()->getAsCXXRecordDecl());
    if (!allocator || allocator->getSpecializedTemplate()->getQualifiedNameAsString() !=
                          "std::allocator")
      return false;
  }
  const bool constructs = name == "emplace" || name == "emplace_back" ||
      name == "push_back" || name == "insert" || name == "try_emplace" ||
      name == "insert_or_assign";
  const bool destroys = name == "clear" || name == "pop_back" ||
      name == "erase" || name == "resize" || name == "operator=";
  for (auto element : elements) {
    element = element.getNonReferenceType();
    const auto *record = element->getAsCXXRecordDecl();
    if (!record) continue;
    if (constructs) {
      for (const auto *constructor : record->ctors())
        if (user_provided_type_method(constructor) &&
            constructor->getNumParams() == call->getNumArgs())
          calls.push_back(constructor);
    }
    if (destroys)
      if (const auto *destructor = record->getDestructor(); destructor &&
          user_provided_type_method(destructor))
        calls.push_back(destructor);
  }
  const bool comparison = name == "insert" || name == "emplace" ||
      name == "emplace_back" || name == "push_back" || name == "find" ||
      name == "contains" || name == "count" || name == "lower_bound" ||
      name == "upper_bound" || name == "equal_range" || name == "erase" ||
      name == "merge" || name == "try_emplace" ||
      name == "insert_or_assign" || name == "operator[]" || name == "at";
  if (comparison && ordered && !elements.empty()) {
    const unsigned comparator_index = mapping ? 2u : 1u;
    const auto *comparator_type = comparator_index < arguments.size() &&
            arguments[comparator_index].getKind() == clang::TemplateArgument::Type &&
            !arguments[comparator_index].getAsType().isNull() &&
            !arguments[comparator_index].getAsType()->isDependentType()
        ? arguments[comparator_index].getAsType()->getAsCXXRecordDecl()
        : nullptr;
    const auto *comparator_special = llvm::dyn_cast_or_null<
        clang::ClassTemplateSpecializationDecl>(comparator_type);
    const auto comparator_name = comparator_special &&
        comparator_special->getSpecializedTemplate()->getQualifiedNameAsString() == "std::less";
    if (comparator_name) {
      append_user_operator(elements.front()->getAsCXXRecordDecl(), "operator<",
                           calls);
    } else if (comparator_type) {
      for (const auto *candidate : comparator_type->methods())
        if (candidate->getNameAsString() == "operator()" &&
            user_provided_type_method(candidate))
          calls.push_back(candidate);
    }
  }
  if (comparison && unordered) {
    const unsigned hash_index = mapping ? 2u : 1u;
    const unsigned equal_index = hash_index + 1;
    for (const unsigned index : {hash_index, equal_index}) {
      if (index >= arguments.size() ||
          arguments[index].getKind() != clang::TemplateArgument::Type ||
          arguments[index].getAsType().isNull() ||
          arguments[index].getAsType()->isDependentType())
        continue;
      const auto *functor = arguments[index].getAsType()->getAsCXXRecordDecl();
      if (!functor) continue;
      const auto *functor_special = llvm::dyn_cast<
          clang::ClassTemplateSpecializationDecl>(functor);
      const auto functor_name = functor_special
          ? functor_special->getSpecializedTemplate()->getQualifiedNameAsString()
          : std::string{};
      if (functor_name == "std::equal_to") {
        if (!elements.empty())
          append_user_operator(elements.front()->getAsCXXRecordDecl(),
                               "operator==", calls);
      } else if (functor_name == "std::hash") {
        continue;
      } else {
        for (const auto *candidate : functor->methods())
          if (candidate->getNameAsString() == "operator()" &&
              user_provided_type_method(candidate))
            calls.push_back(candidate);
      }
    }
  }
  return true;
}

class VisitorContract final
    : public clang::RecursiveASTVisitor<VisitorContract> {
public:
  explicit VisitorContract(clang::SourceManager &sources) : sources_(sources) {}
  bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *call) {
    if (!append_container_mutation(call, calls)) returns_borrow = true;
    const auto *method = call->getMethodDecl();
    if (method && method->getParent()->getQualifiedNameAsString() ==
                      "tracer::Context") {
      if (method->getNameAsString() == "event")
        event = true;
      else if (method->getNameAsString() == "init_scope")
        initializes_scope = true;
    } else if (const auto *callee = call->getDirectCallee()) {
      calls.push_back(callee);
    }
    return true;
  }
  bool VisitCallExpr(clang::CallExpr *call) {
    const auto *callee = call->getDirectCallee();
    const bool standard_string_append = [&] {
      const auto *method = llvm::dyn_cast_or_null<clang::CXXMethodDecl>(callee);
      if (!method || method->getNameAsString() != "operator+=") return false;
      if (!context_source_is_trusted_support(sources_, method->getLocation()))
        return false;
      const auto is_std_basic_string = [](const std::string& name) {
        return ::tracer::compat::starts_with(name, "std::") &&
               ::tracer::compat::ends_with(name, "basic_string");
      };
      const auto *specialization = llvm::dyn_cast_or_null<
          clang::ClassTemplateSpecializationDecl>(method->getParent());
      if (specialization) {
        const auto template_name = specialization->getSpecializedTemplate()
                                       ->getQualifiedNameAsString();
        return is_std_basic_string(template_name);
      }
      const auto parent_name = method->getParent()->getQualifiedNameAsString();
      return is_std_basic_string(parent_name);
    }();
    // In overloaded operator syntax, CXXOperatorCallExpr exposes the borrowed
    // receiver as an ordinary argument. Treating every such argument as an
    // escape rejects harmless `std::string& += ...` inside a synchronous
    // visitor. The standard basic_string operation does not retain its
    // receiver; all other user callbacks and unknown calls remain checked.
    if (!standard_string_append) {
      for (const auto *argument : call->arguments()) {
        const auto *ref = llvm::dyn_cast<clang::DeclRefExpr>(
            argument->IgnoreParenImpCasts());
        if (!ref) continue;
        for (const auto *parameter : visitor_parameters)
          if (ref->getDecl() == parameter) returns_borrow = true;
        for (const auto *alias : borrowed_aliases)
          if (ref->getDecl() == alias) returns_borrow = true;
      }
    }
    if (!llvm::isa<clang::CXXMemberCallExpr>(call))
      if (callee) calls.push_back(callee);
    return true;
  }
  bool VisitReturnStmt(clang::ReturnStmt *ret) {
    const auto *value = ret->getRetValue();
    if (value && !value->getType().isNull() &&
        (value->getType()->isReferenceType() ||
                  value->getType()->isPointerType()))
      returns_borrow = true;
    return true;
  }
  bool VisitUnaryOperator(clang::UnaryOperator *op) {
    if (op->getOpcode() != clang::UO_AddrOf) return true;
    const auto *ref = llvm::dyn_cast<clang::DeclRefExpr>(op->getSubExpr()->IgnoreParenImpCasts());
    if (!ref) return true;
    for (const auto *parameter : visitor_parameters)
      if (ref->getDecl() == parameter) returns_borrow = true;
    for (const auto *alias : borrowed_aliases)
      if (ref->getDecl() == alias) returns_borrow = true;
    return true;
  }
  bool VisitVarDecl(clang::VarDecl *variable) {
    if (variable->isLocalVarDeclOrParm() && !variable->hasGlobalStorage())
      append_cleanup(variable->getType(), calls);
    const auto *initializer = variable->getInit();
    if (!initializer || variable->getType().isNull() ||
        !variable->getType()->isReferenceType()) return true;
    const auto *ref = llvm::dyn_cast<clang::DeclRefExpr>(
        initializer->IgnoreParenImpCasts());
    if (!ref) return true;
    bool borrows = false;
    for (const auto *parameter : visitor_parameters)
      borrows |= ref->getDecl() == parameter;
    for (const auto *alias : borrowed_aliases)
      borrows |= ref->getDecl() == alias;
    if (borrows) borrowed_aliases.push_back(variable);
    return true;
  }
  bool VisitCXXConstructExpr(clang::CXXConstructExpr *construction) {
    const auto *constructor = construction->getConstructor();
    if (user_provided_type_method(constructor))
      calls.push_back(constructor);
    return true;
  }
  bool VisitCXXBindTemporaryExpr(clang::CXXBindTemporaryExpr *temporary) {
    if (temporary->getTemporary()) {
      if (const auto *destructor = temporary->getTemporary()->getDestructor();
          user_provided_type_method(destructor))
        calls.push_back(destructor);
    }
    return true;
  }
  bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *expression) {
    const auto destroyed = expression->getDestroyedType();
    if (destroyed.isNull() || destroyed->isDependentType()) return true;
    const auto *record = destroyed->getAsCXXRecordDecl();
    if (record)
      if (const auto *destructor = record->getDestructor(); destructor &&
          user_provided_type_method(destructor))
        calls.push_back(destructor);
    return true;
  }
  bool event = false;
  bool initializes_scope = false;
  bool returns_borrow = false;
  std::vector<const clang::ParmVarDecl *> visitor_parameters;
  std::vector<const clang::VarDecl *> borrowed_aliases;
  std::vector<const clang::FunctionDecl *> calls;
private:
  clang::SourceManager &sources_;
};

class LifecycleContextAccess final
    : public clang::RecursiveASTVisitor<LifecycleContextAccess> {
public:
  bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *call) {
    const auto *method = call->getMethodDecl();
    if (method && method->getParent()->getQualifiedNameAsString() ==
                      "tracer::Context") {
      const auto name = method->getNameAsString();
      accesses_state |= name == "get" || name == "with" ||
                        name == "init_scope" || name == "end_scope";
    }
    return true;
  }
  bool accesses_state = false;
};

ContextDependency generic_dependency(clang::QualType scope,
                                     clang::QualType payload) {
  ContextDependency dependency;
  dependency.scope_type = canonical_with_template_arguments(scope);
  dependency.payload_type =
      canonical_with_template_arguments(
          payload.getNonReferenceType().getUnqualifiedType());
  dependency.generic = true;
  dependency.shared = false; // Generic contexts default to confined policy.
  return dependency;
}

// A standard-library function reached from user code. Problems found inside it
// are reported at the user's call, naming the function.
struct SystemEntry {
  clang::SourceLocation location;
  std::string name;
};
using SystemEntries = std::map<const clang::FunctionDecl *, SystemEntry>;

constexpr const char *kNoEffectSummary =
    " has no visible body or validated engine effect summary";

// The first rejection of a hook wins, so the text and the location always
// describe the same construct.
void reject_effect(ContextHookPlan &plan, clang::SourceManager &sources,
                   const SystemEntry *entry, clang::SourceLocation location,
                   const std::string &reason, bool without_summary = false) {
  if (plan.unsupported_effects && !plan.unsupported_effect.empty()) return;
  plan.unsupported_effects = true;
  if (entry) {
    plan.unsupported_effect = entry->name +
        " is not a supported operation in a context hook (" + reason +
        " inside it)";
    location = entry->location;
  } else {
    plan.unsupported_effect = reason + (without_summary ? kNoEffectSummary : "");
  }
  if (location.isInvalid()) return;
  const auto presumed = sources.getPresumedLoc(sources.getExpansionLoc(location));
  if (presumed.isInvalid()) return;
  plan.unsupported_file = presumed.getFilename();
  plan.unsupported_line = presumed.getLine();
  plan.unsupported_column = presumed.getColumn();
}

class HookCalls final : public clang::RecursiveASTVisitor<HookCalls> {
public:
  HookCalls(ContextHookPlan &plan,
            ContextDeclarations &result,
            clang::ASTContext &context,
            std::vector<const clang::FunctionDecl *> &calls,
            const SystemEntry *entry)
      : plan_(plan), result_(result), context_(context), calls_(calls),
        entry_(entry) {}

  // Call-site location of every entry of the calls vector, same order.
  const std::vector<clang::SourceLocation> &locations() const {
    return locations_;
  }

  bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *call) {
    if (!append_container_mutation(call, calls_))
      reject("custom std container allocator", call->getBeginLoc());
    synchronize(call->getBeginLoc());
    auto *method = call->getMethodDecl();
    if (method && method->getParent()->getQualifiedNameAsString() ==
                      "tracer::Call") {
      if (method->getNameAsString() == "capture_stack")
        plan_.captures_stack = true;
      if (method->getNameAsString() == "context")
        return true;
    }
    if (method && method->getParent()->getQualifiedNameAsString() ==
                      "tracer::Call")
      return true;
    if (!method ||
        method->getParent()->getQualifiedNameAsString() != "tracer::Context") {
      if (const auto *callee = call->getDirectCallee()) calls_.push_back(callee);
      synchronize(call->getBeginLoc());
      return true;
    }
    const auto name = method->getNameAsString();
    const auto *template_args = method->getTemplateSpecializationArgs();
    if (name == "event") {
      plan_.events = true;
      plan_.all_visible_events = true;
    }
    if (name == "init_scope" || name == "end_scope")
      plan_.local_scope_changes = true;
    if (name == "init_scope") plan_.initializes_scope = true;
    if (name == "end_scope") plan_.ends_scope = true;
    if (name == "init_scope" && template_args && template_args->size() >= 1 &&
        template_args->get(0).getKind() == clang::TemplateArgument::Type &&
        call->getNumArgs()) {
      auto access = generic_dependency(
          template_args->get(0).getAsType(),
          call->getArg(0)->getType().getNonReferenceType());
      unique_add_context_dependency(result_.states, access);
      unique_add_generic(plan_.generic_scope_initializers, access);
    }
    if (name == "with") {
      const auto *lambda = call->getNumArgs()
          ? llvm::dyn_cast<clang::LambdaExpr>(call->getArg(0)->IgnoreParenImpCasts())
          : nullptr;
      if (!lambda) {
        reject("visitor passed to with() is not a lambda expression",
               call->getBeginLoc());
      } else {
        VisitorContract contract(context_.getSourceManager());
        for (const auto *parameter : lambda->getCallOperator()->parameters())
          contract.visitor_parameters.push_back(parameter);
        contract.TraverseStmt(lambda->getBody());
        std::vector<const clang::FunctionDecl *> pending = contract.calls;
        std::vector<const clang::FunctionDecl *> visited;
        while (!pending.empty()) {
          const auto *callee = pending.back();
          pending.pop_back();
          const auto qualified_name = callee->getQualifiedNameAsString();
          // Standard-library member implementations may return references to
          // their own receiver (for example basic_string::operator+=). That
          // implementation detail does not let the visitor retain its
          // borrowed state reference. User-defined callbacks remain traversed
          // and are checked below; protected-namespace spoofing is rejected by
          // the context declaration and native effect validators.
          if (::tracer::compat::starts_with(qualified_name, "std::") ||
              ::tracer::compat::starts_with(qualified_name, "__gnu_cxx::") ||
              ::tracer::compat::starts_with(qualified_name, "__cxxabiv1::")) {
            continue;
          }
          if (!callee->hasBody() ||
              std::find(visited.begin(), visited.end(), callee->getCanonicalDecl()) !=
                  visited.end()) continue;
          visited.push_back(callee->getCanonicalDecl());
          VisitorContract nested(context_.getSourceManager());
          nested.TraverseStmt(const_cast<clang::Stmt *>(callee->getBody()));
          contract.event |= nested.event;
          contract.initializes_scope |= nested.initializes_scope;
          contract.returns_borrow |= nested.returns_borrow;
          pending.insert(pending.end(), nested.calls.begin(), nested.calls.end());
        }
        if (contract.event || contract.initializes_scope ||
            contract.returns_borrow) {
          plan_.invalid_access = true;
          if (contract.event)
            plan_.invalid_access_effect =
                "event() is reachable inside an independent visitor";
          else if (contract.initializes_scope)
            plan_.invalid_access_effect =
                "init_scope() cannot run while a visitor access interval is held";
          else
            plan_.invalid_access_effect =
                "a borrowed state reference can escape its visitor interval";
        }
      }
    }
    if (name != "get" && name != "with") {
      return true;
    }
    const bool generic_access = template_args &&
        ((name == "get" && template_args->size() == 2) ||
         (name == "with" && template_args->size() >= 3));
    if (generic_access &&
        template_args->get(0).getKind() == clang::TemplateArgument::Type &&
        template_args->get(1).getKind() == clang::TemplateArgument::Type &&
        (name == "get" || name == "with")) {
      auto access = generic_dependency(template_args->get(0).getAsType(),
                                       template_args->get(1).getAsType());
      unique_add_context_dependency(result_.states, access);
      unique_add_generic(name == "get" ? plan_.generic_direct
                                        : plan_.generic_visitors,
                         access);
      if (name == "get") plan_.direct_frame = true;
      if (name == "get") {
        auto &sources = context_.getSourceManager();
        const auto range = clang::Lexer::makeFileCharRange(
            clang::CharSourceRange::getTokenRange(call->getSourceRange()),
            sources, context_.getLangOpts());
        if (range.isValid() && !range.getBegin().isMacroID() &&
            !range.getEnd().isMacroID() &&
            sources.isWrittenInMainFile(range.getBegin()))
          plan_.generic_get_source_ranges.emplace_back(
              sources.getFileOffset(range.getBegin()),
              sources.getFileOffset(range.getEnd()) -
                  sources.getFileOffset(range.getBegin()));
      }
    } else if (template_args && template_args->size() &&
        template_args->get(0).getKind() == clang::TemplateArgument::Type) {
      unique_add(name == "get" ? plan_.direct : plan_.visitors,
                 canonical(template_args->get(0).getAsType()));
      if (name == "get") plan_.direct_frame = true;
    }
    return true;
  }

  bool VisitCallExpr(clang::CallExpr *call) {
    if (!llvm::isa<clang::CXXMemberCallExpr>(call)) {
      if (const auto *callee = call->getDirectCallee())
        calls_.push_back(callee);
      else
        reject("indirect call expression", call->getBeginLoc(), true);
    }
    synchronize(call->getBeginLoc());
    return true;
  }

  bool VisitCXXNewExpr(clang::CXXNewExpr *expression) {
    reject("raw owning allocation in reachable hook", expression->getBeginLoc());
    return true;
  }

  bool VisitVarDecl(clang::VarDecl *variable) {
    if (variable->isLocalVarDeclOrParm() && !variable->hasGlobalStorage())
      append_cleanup(variable->getType(), calls_);
    synchronize(variable->getLocation());
    return true;
  }

  bool VisitCXXConstructExpr(clang::CXXConstructExpr *construction) {
    const auto *constructor = construction->getConstructor();
    if (user_provided_type_method(constructor))
      calls_.push_back(constructor);
    synchronize(construction->getBeginLoc());
    return true;
  }

  bool VisitCXXBindTemporaryExpr(clang::CXXBindTemporaryExpr *temporary) {
    if (!temporary->getTemporary()) return true;
    if (const auto *destructor = temporary->getTemporary()->getDestructor();
        user_provided_type_method(destructor))
      calls_.push_back(destructor);
    synchronize(temporary->getBeginLoc());
    return true;
  }

  bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *expression) {
    const auto destroyed = expression->getDestroyedType();
    if (destroyed.isNull() || destroyed->isDependentType()) return true;
    const auto *record = destroyed->getAsCXXRecordDecl();
    if (record)
      if (const auto *destructor = record->getDestructor(); destructor &&
          user_provided_type_method(destructor))
        calls_.push_back(destructor);
    synchronize(expression->getBeginLoc());
    return true;
  }

  bool VisitBinaryOperator(clang::BinaryOperator *operation) {
    if (!operation->isAssignmentOp() ||
        !operation->getRHS()->getType()->isPointerType())
      return true;
    const auto *lhs = operation->getLHS()->IgnoreParenImpCasts();
    bool escapes = llvm::isa<clang::MemberExpr>(lhs);
    if (const auto *reference = llvm::dyn_cast<clang::DeclRefExpr>(lhs))
      if (const auto *variable = llvm::dyn_cast<clang::VarDecl>(reference->getDecl()))
        escapes |= variable->hasGlobalStorage();
    if (escapes)
      reject("pointer stored outside hook-local lifetime",
             operation->getBeginLoc());
    return true;
  }

  bool VisitDeclRefExpr(clang::DeclRefExpr *ref) {
    if (ref->getNameInfo().getAsString() == "state" &&
        !llvm::isa<clang::ParmVarDecl>(ref->getDecl())) {
      plan_.direct_frame = true;
      plan_.uses_state_name = true;
    }
    return true;
  }

  bool VisitUnaryOperator(clang::UnaryOperator *op) {
    if (op->getOpcode() != clang::UO_AddrOf) return true;
    const auto *expr = op->getSubExpr()->IgnoreParenImpCasts();
    if (const auto *ref = llvm::dyn_cast<clang::DeclRefExpr>(expr)) {
      if (ref->getNameInfo().getAsString() == "state")
        plan_.invalid_access = true;
    } else if (const auto *call = llvm::dyn_cast<clang::CXXMemberCallExpr>(expr)) {
      const auto *method = call->getMethodDecl();
      if (method && method->getParent()->getQualifiedNameAsString() ==
                        "tracer::Context" && method->getNameAsString() == "get")
        plan_.invalid_access = true;
    }
    return true;
  }

private:
  void reject(const std::string &reason, clang::SourceLocation location,
              bool without_summary = false) {
    reject_effect(plan_, context_.getSourceManager(), entry_, location, reason,
                  without_summary);
  }
  // Callees are appended to calls_ by several helpers; stamp whatever they
  // added with the location of the construct being visited.
  void synchronize(clang::SourceLocation location) {
    locations_.resize(calls_.size(), location);
  }

  ContextHookPlan &plan_;
  ContextDeclarations &result_;
  clang::ASTContext &context_;
  std::vector<const clang::FunctionDecl *> &calls_;
  const SystemEntry *entry_;
  std::vector<clang::SourceLocation> locations_;
};

void scan_reachable(const clang::FunctionDecl *function, ContextHookPlan &plan,
                    ContextDeclarations &result,
                    clang::ASTContext &context,
                    std::vector<const clang::FunctionDecl *> &visited,
                    SystemEntries &entries,
                    const SystemEntry *entry = nullptr) {
  if (!function || !function->hasBody()) return;
  const auto *canonical_decl = function->getCanonicalDecl();
  if (std::find(visited.begin(), visited.end(), canonical_decl) != visited.end())
    return;
  visited.push_back(canonical_decl);
  if (entry) entries[canonical_decl] = *entry;
  if (const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(function))
    unique_add(plan.reachable_methods, method->getNameAsString());
  std::vector<const clang::FunctionDecl *> calls;
  HookCalls visitor(plan, result, context, calls, entry);
  visitor.TraverseStmt(const_cast<clang::Stmt *>(function->getBody()));
  auto &sources = context.getSourceManager();
  const auto &locations = visitor.locations();
  for (std::size_t index = 0; index < calls.size(); ++index) {
    const auto *callee = calls[index];
    const auto location = index < locations.size() ? locations[index]
                                                   : clang::SourceLocation();
    if (callee->getBuiltinID() == 0) {
      static const std::vector<std::string> supported_std_effects = {
          "operator[]", "push_back", "emplace_back", "emplace", "insert",
          "clear", "reset", "size", "empty", "begin", "end", "at",
          "front", "back", "operator=", "operator+=", "operator++",
          // The call is modeled here only to let the support-effects pass
          // reject exception_ptr retention by type. It is not a general
          // exemption for exception_ptr values.
          "current_exception"};
      const auto name = callee->getQualifiedNameAsString();
      const bool supported = ::tracer::compat::starts_with(name, "std::") &&
                             std::find(supported_std_effects.begin(), supported_std_effects.end(),
                                       callee->getNameAsString()) != supported_std_effects.end();
      if (supported) continue;
      if (!callee->hasBody()) {
        reject_effect(plan, sources, entry, location, name + kNoEffectSummary);
        continue;
      }
    }
    if (!callee->hasBody()) continue;
    const auto *definition = callee->getDefinition();
    SystemEntry system_entry;
    const SystemEntry *callee_entry = nullptr;
    if (sources.isInSystemHeader(definition->getLocation())) {
      if (entry) {
        callee_entry = entry;
      } else {
        system_entry = {location, callee->getQualifiedNameAsString()};
        callee_entry = &system_entry;
      }
    }
    scan_reachable(definition, plan, result, context, visited, entries,
                   callee_entry);
  }
}

// Decides which pointers could refer to context-owned state, so a pointer cast
// or a returned pointer is only rejected when it can launder one. The analysis
// is flow-insensitive and interprocedural over the functions the hook reaches:
// values derive from the context accessors, the injected state parameters, the
// state shorthand and visitor parameters, and the derivation follows
// initializers, assignments, arguments, out-parameters and returns.
class ContextProvenance {
public:
  struct Violation {
    const clang::FunctionDecl *function = nullptr;
    clang::SourceLocation location;
    const char *reason = nullptr;
  };

  ContextProvenance(const clang::FunctionDecl *hook,
                    const std::vector<const clang::FunctionDecl *> &visited) {
    for (const auto *canonical : visited) {
      const clang::FunctionDecl *definition = nullptr;
      if (!canonical->hasBody(definition) || !definition) continue;
      definitions_.push_back(definition);
      scanned_[canonical->getCanonicalDecl()] = definition;
    }
    if (hook)
      if (const clang::FunctionDecl *definition = nullptr;
          hook->hasBody(definition) && definition)
        for (unsigned i = 1; i < definition->getNumParams(); ++i)
          taint(definition->getParamDecl(i));
    for (const auto *definition : definitions_) seed_parameters(definition);
  }

  // Runs the analysis; true when a pointer to context-owned state leaves its
  // access interval.
  bool find(Violation &violation) {
    unsigned pass = 0;
    do {
      changed_ = false;
      for (const auto *definition : definitions_) {
        Flow flow(*this, definition);
        flow.run();
      }
    } while (changed_ && ++pass < kMaxPasses);
    // No fixed point: fall back to rejecting every pointer cast and return.
    conservative_ = changed_;
    for (const auto *definition : definitions_) {
      Sinks sinks(*this, definition);
      sinks.run();
      if (sinks.found) {
        violation = sinks.violation;
        return true;
      }
    }
    return false;
  }

private:
  static constexpr unsigned kMaxPasses = 64;

  struct Reading {
    bool derived = false;
    bool union_member = false;
  };

  class Reader final : public clang::RecursiveASTVisitor<Reader> {
  public:
    Reader(const ContextProvenance &owner, const clang::FunctionDecl *method)
        : owner_(owner), method_(method) {}
    Reading reading;

    bool TraverseUnaryExprOrTypeTraitExpr(clang::UnaryExprOrTypeTraitExpr *) {
      return true;
    }
    bool TraverseCXXNoexceptExpr(clang::CXXNoexceptExpr *) { return true; }
    bool TraverseLambdaExpr(clang::LambdaExpr *lambda) {
      // The body is its own function; the closure derives from what it holds.
      for (const auto &capture : lambda->captures()) {
        if (capture.capturesThis()) {
          reading.derived |= owner_.derived_this(method_);
        } else if (capture.capturesVariable()) {
          reading.derived |= owner_.tainted(capture.getCapturedVar());
        }
      }
      return true;
    }
    bool VisitDeclRefExpr(clang::DeclRefExpr *reference) {
      reading.derived |= owner_.tainted(reference->getDecl());
      return true;
    }
    bool VisitCXXThisExpr(clang::CXXThisExpr *) {
      reading.derived |= owner_.derived_this(method_);
      return true;
    }
    bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *call) {
      const auto *method = call->getMethodDecl();
      reading.derived |= method && method->getParent() &&
          method->getParent()->getQualifiedNameAsString() == "tracer::Context";
      return true;
    }
    bool VisitCallExpr(clang::CallExpr *call) {
      reading.derived |= owner_.returns_derived(call->getDirectCallee());
      return true;
    }
    bool VisitMemberExpr(clang::MemberExpr *member) {
      if (const auto *field =
              llvm::dyn_cast<clang::FieldDecl>(member->getMemberDecl()))
        reading.union_member |= field->getParent()->isUnion();
      return true;
    }

  private:
    const ContextProvenance &owner_;
    const clang::FunctionDecl *method_;
  };

  class Flow final : public clang::RecursiveASTVisitor<Flow> {
  public:
    Flow(ContextProvenance &owner, const clang::FunctionDecl *root)
        : owner_(owner), method_(owner.enclosing_method(root)) {
      functions_.push_back(root);
    }

    void run() { owner_.traverse_body(*this, functions_.front()); }

    bool TraverseLambdaExpr(clang::LambdaExpr *lambda) {
      const auto *call_operator = lambda->getCallOperator();
      owner_.register_lambda(call_operator);
      functions_.push_back(call_operator);
      const bool result =
          clang::RecursiveASTVisitor<Flow>::TraverseLambdaExpr(lambda);
      functions_.pop_back();
      return result;
    }

    bool VisitVarDecl(clang::VarDecl *variable) {
      if (owner_.state_like(variable->getType())) owner_.taint(variable);
      const auto *init = variable->getInit();
      if (init && owner_.flows(init, variable->getType(), method_))
        owner_.taint(variable);
      return true;
    }

    bool VisitCXXForRangeStmt(clang::CXXForRangeStmt *loop) {
      if (loop->getRangeInit() && loop->getLoopVariable() &&
          owner_.reading(loop->getRangeInit(), method_).derived)
        owner_.taint(loop->getLoopVariable());
      return true;
    }

    bool VisitBinaryOperator(clang::BinaryOperator *operation) {
      if (operation->isAssignmentOp() &&
          owner_.carries(operation->getRHS(), method_))
        owner_.taint_bases(operation->getLHS());
      return true;
    }

    bool VisitReturnStmt(clang::ReturnStmt *statement) {
      const auto *value = statement->getRetValue();
      if (!value) return true;
      const auto *function = functions_.back();
      if (owner_.flows(value, function->getReturnType(), method_))
        owner_.mark_returns_derived(function);
      return true;
    }

    bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *call) {
      const auto *method = call->getMethodDecl();
      if (!method || method->getNameAsString() != "with" ||
          method->getParent()->getQualifiedNameAsString() != "tracer::Context" ||
          !call->getNumArgs())
        return true;
      // The visitor parameters borrow the state.
      if (const auto *lambda = llvm::dyn_cast<clang::LambdaExpr>(
              call->getArg(0)->IgnoreParenImpCasts()))
        for (const auto *parameter : lambda->getCallOperator()->parameters())
          owner_.taint(parameter);
      return true;
    }

    bool VisitCallExpr(clang::CallExpr *call) {
      const auto *callee = call->getDirectCallee();
      std::vector<const clang::Expr *> arguments(call->arg_begin(),
                                                 call->arg_end());
      const clang::Expr *object = nullptr;
      if (const auto *member = llvm::dyn_cast<clang::CXXMemberCallExpr>(call)) {
        object = member->getImplicitObjectArgument();
      } else if (const auto *op =
                     llvm::dyn_cast<clang::CXXOperatorCallExpr>(call)) {
        const auto *method = llvm::dyn_cast_or_null<clang::CXXMethodDecl>(callee);
        if (method && method->isInstance() && !arguments.empty()) {
          object = arguments.front();
          arguments.erase(arguments.begin());
        }
        if (assignment_operator(op->getOperator()) && op->getNumArgs() == 2 &&
            owner_.carries(op->getArg(1), method_))
          owner_.taint_bases(op->getArg(0));
      }
      invoke(callee, object, arguments);
      return true;
    }

    bool VisitCXXConstructExpr(clang::CXXConstructExpr *construction) {
      std::vector<const clang::Expr *> arguments(construction->arg_begin(),
                                                 construction->arg_end());
      invoke(construction->getConstructor(), nullptr, arguments);
      return true;
    }

  private:
    static bool assignment_operator(clang::OverloadedOperatorKind kind) {
      switch (kind) {
      case clang::OO_Equal:
      case clang::OO_PlusEqual:
      case clang::OO_MinusEqual:
      case clang::OO_StarEqual:
      case clang::OO_SlashEqual:
      case clang::OO_PercentEqual:
      case clang::OO_CaretEqual:
      case clang::OO_AmpEqual:
      case clang::OO_PipeEqual:
      case clang::OO_LessLessEqual:
      case clang::OO_GreaterGreaterEqual:
        return true;
      default:
        return false;
      }
    }

    static bool writable_through(clang::QualType type) {
      if (type.isNull()) return false;
      return (type->isPointerType() || type->isLValueReferenceType()) &&
             !type->getPointeeType().isConstQualified();
    }

    // Whether the callee can store through this argument. Without a
    // parameter (variadic) only the argument's own type tells.
    static bool writable_argument(clang::QualType parameter,
                                  const clang::Expr *argument) {
      return writable_through(parameter.isNull() ? argument->getType()
                                                 : parameter);
    }

    static bool mutable_object(const clang::FunctionDecl *callee) {
      const auto *method = llvm::dyn_cast_or_null<clang::CXXMethodDecl>(callee);
      return !method || !method->isConst();
    }

    void invoke(const clang::FunctionDecl *callee, const clang::Expr *object,
                const std::vector<const clang::Expr *> &arguments) {
      const auto *definition = owner_.definition(callee);
      const auto parameter_type = [&](std::size_t index) {
        return callee && index < callee->getNumParams()
                   ? callee->getParamDecl(index)->getType()
                   : clang::QualType();
      };
      bool any = object && owner_.carries(object, method_);
      const bool object_carries = any;
      std::vector<bool> flowing(arguments.size());
      for (std::size_t i = 0; i < arguments.size(); ++i) {
        flowing[i] = owner_.flows(arguments[i], parameter_type(i), method_);
        any |= flowing[i];
      }
      if (!any && !definition) return;
      if (!definition) {
        // No body to follow: assume the call may copy anything it is given
        // into anything it can write.
        if (object && mutable_object(callee)) owner_.taint_bases(object);
        for (std::size_t i = 0; i < arguments.size(); ++i)
          if (writable_argument(parameter_type(i), arguments[i]))
            owner_.taint_bases(arguments[i]);
        return;
      }
      for (std::size_t i = 0;
           i < arguments.size() && i < definition->getNumParams(); ++i) {
        const auto *parameter = definition->getParamDecl(i);
        if (flowing[i]) owner_.taint(parameter);
        if (owner_.tainted(parameter) && writable_through(parameter->getType()))
          owner_.taint_bases(arguments[i]);
      }
      if (object_carries && llvm::isa<clang::CXXMethodDecl>(definition))
        owner_.mark_derived_this(definition);
      if (any && object && mutable_object(definition))
        owner_.taint_bases(object);
    }

    ContextProvenance &owner_;
    const clang::FunctionDecl *method_;
    std::vector<const clang::FunctionDecl *> functions_;
  };

  class Sinks final : public clang::RecursiveASTVisitor<Sinks> {
  public:
    Sinks(ContextProvenance &owner, const clang::FunctionDecl *root)
        : owner_(owner), root_(root), method_(owner.enclosing_method(root)) {}
    bool found = false;
    Violation violation;

    void run() { owner_.traverse_body(*this, root_); }

    bool VisitExplicitCastExpr(clang::ExplicitCastExpr *cast) {
      // Nothing is converted by these, so no address can be hidden in them.
      if (cast->getCastKind() == clang::CK_NoOp ||
          cast->getCastKind() == clang::CK_ToVoid)
        return true;
      const auto *operand = cast->getSubExpr();
      const auto *stripped = operand->IgnoreParenImpCasts();
      if (llvm::isa<clang::CXXNullPtrLiteralExpr>(stripped)) return true;
      const auto to = cast->getType();
      if (!to->isPointerType() && !stripped->getType()->isPointerType())
        return true;
      if (!to->isPointerType() &&
          (to->isVoidType() || to->isRecordType() || to->isBooleanType()))
        return true;
      if (!owner_.conservative_ && !owner_.reading(operand, method_).derived)
        return true;
      return report(cast->getBeginLoc(), "pointer cast of context-owned state");
    }

    bool VisitReturnStmt(clang::ReturnStmt *statement) {
      const auto *value = statement->getRetValue();
      if (!value || !value->getType()->isPointerType()) return true;
      if (!owner_.conservative_ && !owner_.reading(value, method_).derived)
        return true;
      return report(statement->getBeginLoc(),
                    "pointer to context-owned state returned from a reachable "
                    "function");
    }

  private:
    bool report(clang::SourceLocation location, const char *reason) {
      found = true;
      violation = {root_, location, reason};
      return false;
    }

    ContextProvenance &owner_;
    const clang::FunctionDecl *root_;
    const clang::FunctionDecl *method_;
  };

  template <typename Visitor>
  void traverse_body(Visitor &visitor, const clang::FunctionDecl *definition) {
    if (const auto *constructor =
            llvm::dyn_cast<clang::CXXConstructorDecl>(definition))
      for (const auto *initializer : constructor->inits())
        if (initializer->getInit())
          visitor.TraverseStmt(initializer->getInit());
    visitor.TraverseStmt(const_cast<clang::Stmt *>(definition->getBody()));
  }

  // The method `this` belongs to: lambdas use the this of their context.
  const clang::FunctionDecl *
  enclosing_method(const clang::FunctionDecl *function) const {
    while (const auto *method =
               llvm::dyn_cast_or_null<clang::CXXMethodDecl>(function)) {
      if (!method->getParent()->isLambda()) return function;
      function = llvm::dyn_cast_or_null<clang::FunctionDecl>(
          method->getParent()->getLambdaContextDecl());
    }
    return function;
  }

  const clang::FunctionDecl *definition(const clang::FunctionDecl *callee) const {
    if (!callee) return nullptr;
    const auto found = scanned_.find(callee->getCanonicalDecl());
    return found == scanned_.end() ? nullptr : found->second;
  }

  void register_lambda(const clang::CXXMethodDecl *call_operator) {
    if (scanned_.emplace(call_operator->getCanonicalDecl(), call_operator).second)
      seed_parameters(call_operator);
  }

  void seed_parameters(const clang::FunctionDecl *function) {
    for (const auto *parameter : function->parameters())
      if (state_like(parameter->getType())) taint(parameter);
  }

  void taint(const clang::VarDecl *variable) {
    // The call object never holds context state; its accessors stay clean.
    if (is_record(variable->getType(), "tracer::Call")) return;
    if (tainted_.insert(variable).second) changed_ = true;
  }

  void mark_derived_this(const clang::FunctionDecl *function) {
    if (derived_this_.insert(function->getCanonicalDecl()).second)
      changed_ = true;
  }

  void mark_returns_derived(const clang::FunctionDecl *function) {
    if (returns_derived_.insert(function->getCanonicalDecl()).second)
      changed_ = true;
  }

  bool derived_this(const clang::FunctionDecl *method) const {
    return method && derived_this_.count(method->getCanonicalDecl());
  }

  bool returns_derived(const clang::FunctionDecl *callee) const {
    return callee && returns_derived_.count(callee->getCanonicalDecl());
  }

  bool tainted(const clang::ValueDecl *declaration) const {
    if (const auto *variable =
            llvm::dyn_cast_or_null<clang::VarDecl>(declaration)) {
      // The state shorthand placeholder, as VisitDeclRefExpr sees it.
      if (!llvm::isa<clang::ParmVarDecl>(variable) && variable->getIdentifier() &&
          variable->getName() == "state")
        return true;
      return tainted_.count(variable) != 0;
    }
    if (const auto *binding =
            llvm::dyn_cast_or_null<clang::BindingDecl>(declaration))
      return tainted(binding->getDecomposedDecl());
    return false;
  }

  Reading reading(const clang::Expr *expression,
                  const clang::FunctionDecl *method) const {
    Reader reader(*this, method);
    reader.TraverseStmt(const_cast<clang::Expr *>(expression));
    return reader.reading;
  }

  static bool plain_scalar(clang::QualType type) {
    return !type.isNull() && !type->isDependentType() &&
           (type->isIntegralOrEnumerationType() || type->isFloatingType() ||
            type->isVoidType());
  }

  // The value itself can lead back to context state: arithmetic does not,
  // unless it was read through a union that could hold a pointer.
  bool carries(const clang::Expr *expression,
               const clang::FunctionDecl *method) const {
    const auto read = reading(expression, method);
    return read.derived && (!plain_scalar(expression->getType()) ||
                            read.union_member);
  }

  // A value bound to a reference refers to the object itself, so a scalar
  // member of the state is as good as a pointer there.
  bool flows(const clang::Expr *expression, clang::QualType destination,
             const clang::FunctionDecl *method) const {
    const auto read = reading(expression, method);
    if (!read.derived) return false;
    return (!destination.isNull() && destination->isReferenceType()) ||
           !plain_scalar(expression->getType()) || read.union_member;
  }

  static bool is_record(clang::QualType type, const char *name) {
    if (type.isNull() || type->isDependentType()) return false;
    const auto *record = type.getNonReferenceType()->getAsCXXRecordDecl();
    return record && record->getQualifiedNameAsString() == name;
  }

  bool state_like(clang::QualType type) {
    if (type.isNull() || type->isDependentType()) return false;
    type = type.getNonReferenceType();
    while (!type.isNull()) {
      if (type->isPointerType())
        type = type->getPointeeType().getNonReferenceType();
      else if (type->isArrayType())
        type = type->getAsArrayTypeUnsafe()->getElementType();
      else
        break;
    }
    if (type.isNull() || type->isDependentType()) return false;
    const auto *record = type->getAsCXXRecordDecl();
    if (!record) return false;
    const auto cached = state_types_.find(record);
    if (cached != state_types_.end()) return cached->second;
    std::vector<ContextDependency> found;
    markers(record, found);
    const bool state = !found.empty() ||
        record->getQualifiedNameAsString() == "tracer::Context" ||
        specialization(type, "tracer::State");
    state_types_[record] = state;
    return state;
  }

  // The variables an lvalue or pointer expression designates part of.
  void bases(const clang::Expr *expression,
             std::vector<const clang::VarDecl *> &found) const {
    while (expression) {
      expression = expression->IgnoreParens();
      if (const auto *cast = llvm::dyn_cast<clang::CastExpr>(expression)) {
        expression = cast->getSubExpr();
      } else if (const auto *reference =
                     llvm::dyn_cast<clang::DeclRefExpr>(expression)) {
        const auto *declaration = reference->getDecl();
        if (const auto *binding = llvm::dyn_cast<clang::BindingDecl>(declaration))
          declaration = binding->getDecomposedDecl();
        if (const auto *variable = llvm::dyn_cast_or_null<clang::VarDecl>(declaration))
          found.push_back(variable);
        return;
      } else if (const auto *member = llvm::dyn_cast<clang::MemberExpr>(expression)) {
        expression = member->getBase();
      } else if (const auto *subscript =
                     llvm::dyn_cast<clang::ArraySubscriptExpr>(expression)) {
        expression = subscript->getBase();
      } else if (const auto *unary = llvm::dyn_cast<clang::UnaryOperator>(expression)) {
        expression = unary->getSubExpr();
      } else if (const auto *binary = llvm::dyn_cast<clang::BinaryOperator>(expression)) {
        if (binary->isAssignmentOp() || binary->getOpcode() == clang::BO_Comma) {
          expression = binary->isAssignmentOp() ? binary->getLHS() : binary->getRHS();
        } else {
          bases(binary->getLHS(), found);
          expression = binary->getRHS();
        }
      } else if (const auto *conditional =
                     llvm::dyn_cast<clang::AbstractConditionalOperator>(expression)) {
        bases(conditional->getTrueExpr(), found);
        expression = conditional->getFalseExpr();
      } else if (const auto *member_call =
                     llvm::dyn_cast<clang::CXXMemberCallExpr>(expression)) {
        expression = member_call->getImplicitObjectArgument();
      } else if (const auto *call = llvm::dyn_cast<clang::CallExpr>(expression)) {
        // operator[], operator*, std::move and the like hand back their argument.
        expression = call->getNumArgs() ? call->getArg(0) : nullptr;
      } else if (const auto *temporary =
                     llvm::dyn_cast<clang::MaterializeTemporaryExpr>(expression)) {
        expression = temporary->getSubExpr();
      } else if (const auto *bound =
                     llvm::dyn_cast<clang::CXXBindTemporaryExpr>(expression)) {
        expression = bound->getSubExpr();
      } else if (const auto *cleanups =
                     llvm::dyn_cast<clang::ExprWithCleanups>(expression)) {
        expression = cleanups->getSubExpr();
      } else if (const auto *construction =
                     llvm::dyn_cast<clang::CXXConstructExpr>(expression)) {
        expression = construction->getNumArgs() ? construction->getArg(0) : nullptr;
      } else {
        return;
      }
    }
  }

  void taint_bases(const clang::Expr *expression) {
    std::vector<const clang::VarDecl *> found;
    bases(expression, found);
    for (const auto *variable : found) taint(variable);
  }

  std::vector<const clang::FunctionDecl *> definitions_;
  std::map<const clang::FunctionDecl *, const clang::FunctionDecl *> scanned_;
  std::set<const clang::VarDecl *> tainted_;
  std::set<const clang::FunctionDecl *> derived_this_;
  std::set<const clang::FunctionDecl *> returns_derived_;
  std::map<const clang::CXXRecordDecl *, bool> state_types_;
  bool changed_ = false;
  bool conservative_ = false;
};

class Declarations final : public clang::RecursiveASTVisitor<Declarations> {
public:
  Declarations(clang::ASTContext &context, ContextDeclarations &result)
      : context_(context), result_(result) {}

  bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *call) {
    const auto *method = call->getMethodDecl();
    if (method &&
        method->getParent()->getQualifiedNameAsString() == "tracer::Context") {
      result_.requires_context = true;
    }
    return true;
  }

  bool VisitCXXRecordDecl(clang::CXXRecordDecl *record) {
    if (!record->isThisDeclarationADefinition() || record->isImplicit()) {
      return true;
    }
    std::vector<ContextDependency> state_marker;
    markers(record, state_marker);
    if (state_marker.size() == 1) {
      state_marker.front().state_type = record->getQualifiedNameAsString();
      unique_add_context_dependency(result_.states, state_marker.front());
      for (const auto *constructor : record->ctors()) {
        std::vector<const clang::FunctionDecl *> visited;
        result_.requires_stack_capture |=
            function_reaches_stack_capture(constructor, visited);
      }
    }
    if (!state_marker.empty()) {
      for (const auto *constructor : record->ctors()) {
        if (!constructor->hasBody()) continue;
        LifecycleContextAccess lifecycle_access;
        lifecycle_access.TraverseStmt(const_cast<clang::Stmt *>(constructor->getBody()));
        if (lifecycle_access.accesses_state) {
          result_.error = "invalid_context_lifecycle_access: state constructors "
                          "cannot access active context state or scopes";
          return false;
        }
      }
      if (const auto *destructor = record->getDestructor();
          destructor && destructor->hasBody()) {
        LifecycleContextAccess lifecycle_access;
        lifecycle_access.TraverseStmt(const_cast<clang::Stmt *>(destructor->getBody()));
        if (lifecycle_access.accesses_state) {
          result_.error = "invalid_context_lifecycle_access: state destructors "
                          "cannot access active context state or scopes";
          return false;
        }
      }
    }
    ContextClassPlan plan;
    plan.type = context_class_type_name(*record);
    unsigned dependency_constructors = 0;
    for (const auto *constructor : record->ctors()) {
      bool has_dependency = false;
      for (const auto *parameter : constructor->parameters()) {
        const auto *token =
            specialization(parameter->getType(), "tracer::State");
        if (!token) {
          continue;
        }
        has_dependency = true;
        result_.requires_context = true;
        const auto type = argument(*token);
        std::vector<ContextDependency> found;
        if (!type.isNull()) {
          markers(type->getAsCXXRecordDecl(), found);
        }
        if (found.size() != 1) {
          result_.error = "invalid_state_marker: State<T> requires exactly one "
                          "state<Scope> or "
                          "shared_state<Scope> base";
          return false;
        }
        found[0].state_type = canonical(type);
        found[0].parameter_name = parameter->getNameAsString();
        plan.dependencies.push_back(std::move(found[0]));
      }
      if (has_dependency) {
        ++dependency_constructors;
        if (constructor->doesThisDeclarationHaveABody() ||
            constructor->isDeleted()) {
          result_.error = "invalid_state_constructor: State<T> constructor "
                          "must be declaration-only";
          return false;
        }
        for (const auto *parameter : constructor->parameters()) {
          if (!specialization(parameter->getType(), "tracer::State")) {
            result_.error =
                "invalid_state_constructor: unsupported non-State dependency";
            return false;
          }
        }
      }
    }
    if (dependency_constructors > 1) {
      result_.error =
          "invalid_state_constructor: multiple dependency constructors";
      return false;
    }
    const bool has_dependency = !plan.dependencies.empty();
  bool context_relevant = has_dependency;
  ContextUseScanner record_context(true);
  record_context.TraverseDecl(const_cast<clang::CXXRecordDecl *>(record));
  context_relevant |= record_context.uses_context;
  std::vector<const clang::FunctionDecl *> context_visited;
  for (const auto *callee : record_context.callees)
    context_relevant |= function_reaches_context_use(callee, context_visited);
  for (const auto *method : record->methods()) {
    if (method->getNameAsString() != "pre" &&
        method->getNameAsString() != "post")
      continue;
    // Extra hook parameters are the typed State injection surface, even when
    // their type lacks a valid marker; retain validation so unmarked injection
    // attempts cannot fall through to the legacy callback path.
    context_relevant |= method->getNumParams() > 1;
    for (unsigned i = 1; i < method->getNumParams(); ++i) {
      std::vector<ContextDependency> injected_markers;
      markers(method->getParamDecl(i)->getType().getNonReferenceType()
                  ->getAsCXXRecordDecl(),
              injected_markers);
      context_relevant |= injected_markers.size() == 1;
    }
  }
  // The shared-context planner only owns context-aware interceptor shapes.
  // Legacy callbacks, including their non-void PRE ABI and native helpers,
  // remain under the ordinary callback-shape validator.
  if (!context_relevant) return true;
    bool uses_context = has_dependency;
    for (const auto *method : record->methods()) {
      const auto name = method->getNameAsString();
      if (name != "pre" && name != "post") {
        continue;
      }
      if (std::any_of(plan.hooks.begin(), plan.hooks.end(),
                      [&](const auto &existing) { return existing.method == name; })) {
        result_.error = "invalid_state_hook: overloaded pre/post hooks are "
                        "ambiguous for generated dispatch";
        return false;
      }
      ContextHookPlan hook;
      hook.method = name;
      if (method->isStatic() || method->getAccess() != clang::AS_public ||
          !method->hasBody() || !method->getNumParams()) {
        result_.error =
            "invalid_state_hook: hooks must be public defined instance methods";
        return false;
      }
      if (!method->getReturnType()->isVoidType()) {
        result_.error = "invalid_state_hook: hook must return void so no "
                        "borrow can escape its generated access interval";
        return false;
      }
      if (!method->getParamDecl(0)->getType()->isLValueReferenceType() ||
          canonical(method->getParamDecl(0)->getType()) != "tracer::Call") {
        result_.error = "invalid_state_hook: first hook parameter must be "
                        "tracer::Call& or const tracer::Call&";
        return false;
      }
      for (unsigned i = 1; i < method->getNumParams(); ++i) {
        const auto type = method->getParamDecl(i)->getType();
        if (!type->isLValueReferenceType() ||
            type->getPointeeType().isConstQualified()) {
          result_.error =
              "invalid_state_injection: expected registered mutable T&";
          return false;
        }
        const auto state = canonical(type);
        std::vector<ContextDependency> injected_markers;
        markers(type.getNonReferenceType()->getAsCXXRecordDecl(),
                injected_markers);
        if (injected_markers.size() != 1) {
          result_.error = "invalid_state_injection: injected type " + state +
                          " must have exactly one state<Scope> or "
                          "shared_state<Scope> marker (found " +
                          std::to_string(injected_markers.size()) + ")";
          return false;
        }
        injected_markers.front().state_type = state;
        const bool has_matching_dependency = std::any_of(
            plan.dependencies.begin(), plan.dependencies.end(),
            [&](const ContextDependency &dependency) {
              return dependency.state_type == state;
            });
        if (!has_matching_dependency) {
          result_.error = "invalid_state_injection: injected type " + state +
                          " requires a matching State<T> constructor dependency";
          return false;
        }
        unique_add_context_dependency(result_.states, injected_markers.front());
        unique_add(hook.injected, state);
        hook.direct_frame = true;
      }
      bool has_state_parameter = false;
      for (const auto *parameter : method->parameters())
        has_state_parameter |= parameter->getNameAsString() == "state";
      std::vector<const clang::FunctionDecl *> visited;
      SystemEntries entries;
      scan_reachable(method, hook, result_, context_, visited, entries);
      if (!hook.invalid_access && !hook.unsupported_effects) {
        ContextProvenance provenance(method, visited);
        ContextProvenance::Violation violation;
        if (provenance.find(violation)) {
          const auto found = entries.find(violation.function->getCanonicalDecl());
          reject_effect(hook, context_.getSourceManager(),
                        found == entries.end() ? nullptr : &found->second,
                        violation.location, violation.reason);
        }
      }
      if (hook.invalid_access) {
        if (::tracer::compat::starts_with(hook.invalid_access_effect, "init_scope")) {
          result_.error = "invalid_context_access_plan: " + hook.invalid_access_effect;
        } else {
          result_.error = "invalid_context_borrow: " + hook.invalid_access_effect;
        }
        return false;
      }
      if (!has_state_parameter && has_dependency && hook.uses_state_name) {
        if (plan.dependencies.size() == 1) {
          hook.shorthand.emplace_back("state", plan.dependencies.front().state_type);
        } else {
          for (const auto &dependency : plan.dependencies) {
            if (dependency.parameter_name.empty()) {
              result_.error = "invalid_state_shorthand: multiple dependencies "
                              "used by shorthand require named constructor "
                              "parameters";
              return false;
            }
            hook.shorthand.emplace_back("state." + dependency.parameter_name,
                                        dependency.state_type);
          }
        }
      }
      if (hook.unsupported_effects) {
        result_.error = "unsupported_context_effect: " + hook.unsupported_effect;
        if (hook.unsupported_line) {
          result_.error_file = hook.unsupported_file;
          result_.error_line = hook.unsupported_line;
          result_.error_column = hook.unsupported_column;
          result_.error += " at " + hook.unsupported_file + ":" +
                           std::to_string(hook.unsupported_line) + ":" +
                           std::to_string(hook.unsupported_column);
        }
        return false;
      }
      if (hook.initializes_scope && hook.direct_frame) {
        result_.error = "invalid_context_access_plan: init_scope is not "
                        "allowed while a direct access frame is held";
        return false;
      }
      uses_context |= !hook.injected.empty() || !hook.direct.empty() ||
                      !hook.visitors.empty() || !hook.generic_direct.empty() ||
                      !hook.generic_visitors.empty() ||
                      !hook.generic_scope_initializers.empty() || hook.events ||
                      hook.local_scope_changes;
      plan.hooks.push_back(std::move(hook));
    }
    // A class is planned when any hook uses context. Its other hooks stay in
    // the plan so the generated dispatch still runs them.
    if (!uses_context) return true;
    plan.context_only = !has_dependency;
    result_.classes.push_back(std::move(plan));
    return true;
  }

private:
  clang::ASTContext &context_;
  ContextDeclarations &result_;
};
} // namespace

ContextDeclarations analyze_context_declarations(clang::ASTContext &context) {
  ContextDeclarations result;
  ContextUseScanner context_use;
  context_use.TraverseDecl(context.getTranslationUnitDecl());
  ContextRequirementScanner requirement;
  requirement.TraverseDecl(context.getTranslationUnitDecl());
  result.requires_context = context_use.uses_context || requirement.required;
  std::string protected_namespace_error;
  if (context_use.uses_context) {
    TrustedStandardNamespaces namespaces(context.getSourceManager());
    namespaces.TraverseDecl(context.getTranslationUnitDecl());
    protected_namespace_error = std::move(namespaces.error);
    if (!protected_namespace_error.empty()) {
      result.requires_context = true;
      result.error = std::move(protected_namespace_error);
      return result;
    }
  }
  Declarations visitor(context, result);
  visitor.TraverseDecl(context.getTranslationUnitDecl());
  if (!result.error.empty()) return result;
  std::vector<std::string> generic_payload_types;
  for (const auto &class_plan : result.classes)
    for (const auto &hook : class_plan.hooks) {
      result.requires_stack_capture |= hook.captures_stack;
      for (const auto &initializer : hook.generic_scope_initializers)
        if (std::find(generic_payload_types.begin(), generic_payload_types.end(),
                      initializer.payload_type) == generic_payload_types.end())
          generic_payload_types.push_back(initializer.payload_type);
    }
  if (!generic_payload_types.empty()) {
    GenericPayloadStackScan generic_scan(context, generic_payload_types);
    generic_scan.TraverseDecl(context.getTranslationUnitDecl());
    result.requires_stack_capture |= generic_scan.requires_stack_capture;
  }
  return result;
}

std::string context_activation_error(const ContextDeclarations &declarations) {
  if (!declarations.requires_context) {
    return {};
  }
  if (!declarations.error.empty()) {
    return declarations.error;
  }
  return "unsupported_state_support_profile: shared-context activation "
         "requires a validated owner "
         "generation, sealed support/allocator closure, native ownership "
         "bridges and unwind path; "
         "these are not yet available";
}
} // namespace tracer

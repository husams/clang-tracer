#include "context_lifecycle.hpp"
#include "context_analysis.hpp"

#include <algorithm>

#include <clang/AST/DeclCXX.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Lex/Preprocessor.h>

namespace tracer {
namespace {
// Generated constructor ABI: current_context() returns the engine-owned
// per-thread context reference. InvocationFrame(context) saves its exact
// cursor; push_create_or_attach<T>() consumes a constructor State<T> token by
// type and performs rollback-safe binding installation. The frame destructor
// never throws and restores surviving owned/borrowed bindings exactly once.
struct Insertion {
  std::size_t offset;
  std::string members;
};

class LifecycleClasses final
    : public clang::RecursiveASTVisitor<LifecycleClasses> {
public:
  LifecycleClasses(clang::ASTUnit &unit,
                   const ContextDeclarations &declarations,
                   GeneratedContextLifecycle &result, std::string facade)
      : unit_(unit), declarations_(declarations), result_(result),
        facade_(std::move(facade)) {}

  bool VisitCXXRecordDecl(clang::CXXRecordDecl *record) {
    if (!record->isThisDeclarationADefinition() || record->isImplicit())
      return true;
    const auto type = record->getQualifiedNameAsString();
    const auto found =
        std::find_if(declarations_.classes.begin(), declarations_.classes.end(),
                     [&](const auto &plan) { return plan.type == type; });
    if (found == declarations_.classes.end())
      return true;
    const auto reject = [&](const char *reason) {
      result_.error = std::string("unsupported_state_lifecycle: ") + reason;
      return false;
    };
    auto &context = unit_.getASTContext();
    auto &sources = context.getSourceManager();
    const auto close = record->getBraceRange().getEnd();
    if (!record->getIdentifier() || record->isLocalClass() ||
        record->isUnion() || record->getDescribedClassTemplate() ||
        llvm::isa<clang::ClassTemplateSpecializationDecl>(record) ||
        llvm::isa<clang::CXXRecordDecl>(record->getDeclContext()))
      return reject(
          "interceptor must be a named non-template namespace-scope class");
    for (const auto *scope = record->getDeclContext(); scope;
         scope = scope->getParent())
      if (const auto *ns = llvm::dyn_cast<clang::NamespaceDecl>(scope);
          ns && ns->isAnonymousNamespace())
        return reject("anonymous interceptor namespaces cannot yet be named in "
                      "generated definitions");
    if (close.isInvalid() || close.isMacroID() ||
        !sources.isWrittenInMainFile(close))
      return reject(
          "lifecycle insertion requires a direct main-file class definition");
    if (const auto *destructor = record->getDestructor();
        destructor && !destructor->isImplicit())
      return reject("explicit interceptor destructor cannot yet be merged");
    unsigned constructors = 0;
    for (const auto *constructor : record->ctors()) {
      if (constructor->isImplicit())
        continue;
      if (constructor->isCopyOrMoveConstructor())
        return reject("explicit copy/move construction conflicts with "
                      "invocation ownership");
      ++constructors;
      const auto *prototype =
          constructor->getType()->getAs<clang::FunctionProtoType>();
      if (constructor->getAccess() != clang::AS_public ||
          constructor->isConstexpr() ||
          constructor->getDescribedFunctionTemplate() ||
          (prototype && (prototype->isNothrow() || prototype->isVariadic())))
        return reject("dependency constructor must be public, non-constexpr "
                      "and potentially throwing");
      for (const auto *parameter : constructor->parameters()) {
        if (parameter->getType()->isReferenceType())
          return reject("State constructor tokens must be passed by value");
        const auto *token =
            llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
                parameter->getType()->getAsCXXRecordDecl());
        if (!token || token->getTemplateArgs().size() != 1 ||
            token->getTemplateArgs()[0].getKind() !=
                clang::TemplateArgument::Type)
          return reject("invalid State constructor token");
        const auto requested = token->getTemplateArgs()[0].getAsType();
        const auto *state = requested->getAsCXXRecordDecl();
        if (requested.isConstQualified() || requested.isVolatileQualified() ||
            !state || !state->getIdentifier() ||
            llvm::isa<clang::ClassTemplateSpecializationDecl>(state))
          return reject(
              "state must be a named unqualified non-template record");
        for (const auto *scope = state->getDeclContext(); scope;
             scope = scope->getParent())
          if (const auto *ns = llvm::dyn_cast<clang::NamespaceDecl>(scope);
              ns && ns->isAnonymousNamespace())
            return reject("anonymous state namespaces cannot yet be named in "
                          "generated definitions");
      }
    }
    const bool context_only = found->dependencies.empty();
    if ((!context_only && constructors != 1) ||
        (context_only && constructors != 0))
      return reject(context_only
                        ? "context-only interceptor must use its implicit "
                          "default constructor"
                        : "expected exactly one generated dependency constructor");
    for (const auto *method : record->methods())
      if (!method->isImplicit() && (method->isCopyAssignmentOperator() ||
                                    method->isMoveAssignmentOperator()))
        return reject(
            "explicit assignment conflicts with invocation ownership");

    const auto name = record->getNameAsString();
    const auto buffer = sources.getBufferData(sources.getMainFileID());
    std::string member;
    for (unsigned suffix = 0; suffix < 4096; ++suffix) {
      member = "tracer_context_frame_" + std::to_string(suffix);
      auto *identifier = &context.Idents.get(member);
      if (!unit_.getPreprocessor().isMacroDefined(identifier) &&
          record->lookup(clang::DeclarationName(identifier)).empty() &&
          !buffer.contains(member))
        break;
      member.clear();
    }
    if (member.empty())
      return reject("could not reserve a collision-free lifecycle member");
    std::string members =
        "\nprivate:\n  ::" + facade_ + "::InvocationFrame " + member + ";\npublic:\n";
    if (context_only) members += "  " + name + "();\n";
    members += "  ~" + name + "() noexcept;\n";
    members += "  " + name + "(const " + name + "&) = delete;\n";
    members += "  " + name + "& operator=(const " + name + "&) = delete;\n";
    members += "  " + name + "(" + name + "&&) = delete;\n";
    members += "  " + name + "& operator=(" + name + "&&) = delete;\n";
    insertions.push_back({sources.getFileOffset(close), std::move(members)});
    definitions += "\n" + type + "::" + name + "(";
    for (std::size_t i = 0; i < found->dependencies.size(); ++i) {
      if (i)
        definitions += ", ";
      definitions +=
          "::tracer::State<::" + found->dependencies[i].state_type + ">";
    }
    definitions += ") : " + member + "(::" + facade_ + "::current_context()) {\n";
    for (const auto &dependency : found->dependencies)
      definitions += "  " + member +
                     ".push_create_or_attach<::" + dependency.state_type +
                     ">();\n";
    definitions += "}\n" + type + "::~" + name + "() noexcept {\n  " + member +
                   ".pop_all_and_restore();\n}\n";
    result_.classes.push_back({type, member});
    return true;
  }

  std::vector<Insertion> insertions;
  std::string definitions;

private:
  clang::ASTUnit &unit_;
  const ContextDeclarations &declarations_;
  GeneratedContextLifecycle &result_;
  std::string facade_;
};
} // namespace

GeneratedContextLifecycle generate_context_lifecycle(
    clang::ASTUnit &unit, const std::string &facade_namespace) {
  GeneratedContextLifecycle result;
  result.facade_namespace = facade_namespace;
  const auto declarations = analyze_context_declarations(unit.getASTContext());
  if (!declarations.error.empty()) {
    result.error = declarations.error;
    return result;
  }
  auto &sources = unit.getSourceManager();
  const auto source = sources.getBufferData(sources.getMainFileID());
  if (source.size() > 1024 * 1024 || declarations.classes.size() > 64) {
    result.error =
        "unsupported_state_lifecycle: generation size limit exceeded";
    return result;
  }
  LifecycleClasses visitor(unit, declarations, result, facade_namespace);
  if (!visitor.TraverseDecl(unit.getASTContext().getTranslationUnitDecl()) ||
      !result.error.empty()) {
    result.classes.clear();
    return result;
  }
  if (result.classes.size() != declarations.classes.size()) {
    result.error = "unsupported_state_lifecycle: incomplete class generation";
    result.classes.clear();
    return result;
  }
  result.compilation_copy = source.str();
  std::sort(visitor.insertions.begin(), visitor.insertions.end(),
            [](const auto &a, const auto &b) { return a.offset > b.offset; });
  for (const auto &insertion : visitor.insertions)
    result.compilation_copy.insert(insertion.offset, insertion.members);
  result.compilation_copy += visitor.definitions;
  // The generated frame is a member declared inside the user class, before
  // the later trait specializations. Make only the non-template facade names
  // visible first; access templates remain defined after the user types.
  result.compilation_copy =
      "#include \"context_facade.hpp\"\nnamespace " + facade_namespace +
      " { using namespace ::tracer::context_facade; }\n" +
      result.compilation_copy;
  return result;
}
} // namespace tracer

#include <filesystem>
#include <set>
#include <stdexcept>

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/QualTypeNames.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/Support/raw_ostream.h>

#include "live_mock.hpp"

namespace tracer::live {

std::string normalize_mock_selector(std::string spelling) {
  std::erase_if(spelling, [](unsigned char ch) { return ch == ' ' || ch == '\t'; });
  if (spelling.starts_with("::")) {
    spelling.erase(0, 2);
  }
  return spelling;
}

namespace {
class MockDeclarations : public clang::RecursiveASTVisitor<MockDeclarations> {
public:
  MockDeclarations(clang::ASTContext& ast, std::span<const std::string> selectors)
      : ast_(ast)
      , selectors_(selectors.begin(), selectors.end())
      , mangler_(ast.createMangleContext()) {}

  bool shouldVisitTemplateInstantiations() const { return true; }

  bool VisitFunctionDecl(const clang::FunctionDecl* decl) {
    if (!decl->isThisDeclarationADefinition() || decl->isImplicit() || decl->isDependentContext() ||
        llvm::isa<clang::CXXConstructorDecl, clang::CXXDestructorDecl>(decl)) {
      return true;
    }
    const auto& sources = ast_.getSourceManager();
    if (sources.isInSystemHeader(decl->getLocation())) {
      return true;
    }
    std::string symbol;
    if (mangler_->shouldMangleDeclName(decl)) {
      llvm::raw_string_ostream output(symbol);
      mangler_->mangleName(clang::GlobalDecl(decl), output);
    } else {
      symbol = decl->getNameAsString();
    }
    auto signature = llvm::demangle(symbol);
    if (signature == symbol) {
      signature += '(';
      for (const auto* parameter : decl->parameters()) {
        if (signature.back() != '(') {
          signature += ", ";
        }
        signature += type_name(parameter->getType().getCanonicalType());
      }
      signature += ')';
    }
    const auto key = normalize_mock_selector(signature);
    if (!selectors_.contains(key)) {
      return true;
    }
    if (declarations.contains(key)) {
      throw std::invalid_argument("ambiguous mock declaration for " + signature);
    }

    auto& entry = declarations[key];
    entry.linkage = symbol;
    auto& callable = entry.callable;
    callable.selector = signature;
    const auto* prototype = decl->getType()->getAs<clang::FunctionProtoType>();
    if (!prototype || prototype->isVariadic() || prototype->getCallConv() != clang::CC_C ||
        clang::isUnresolvedExceptionSpec(prototype->getExceptionSpecType()) ||
        prototype->hasDynamicExceptionSpec() || decl->hasAttr<clang::NakedAttr>()) {
      throw std::invalid_argument("unsupported native mock callable ABI: " + callable.selector);
    }
    callable.is_noexcept = prototype->isNothrow();
    callable.result = type_name(decl->getReturnType());
    for (const auto* param : decl->parameters()) {
      callable.parameters.push_back(type_name(param->getType()));
    }
    if (const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(decl);
        method && !method->isStatic()) {
      if (method->isPureVirtual() || method->getRefQualifier() == clang::RQ_RValue) {
        throw std::invalid_argument("unsupported pure virtual or rvalue receiver: " +
                                    callable.selector);
      }
      callable.receiver = type_name(method->getThisType()->getPointeeType());
      add_header(entry, method->getParent()->getDefinition()->getLocation());
    }
    for (const auto* ancestor : decl->redecls()) {
      add_header(entry, ancestor->getLocation());
    }
    return true;
  }

  std::map<std::string, MockDeclaration> declarations;

private:
  std::string type_name(clang::QualType type) const {
    auto policy = ast_.getPrintingPolicy();
    policy.SuppressUnwrittenScope = true;
    policy.FullyQualifiedName = true;
    return clang::TypeName::getFullyQualifiedName(type, ast_, policy, true);
  }

  void add_header(MockDeclaration& entry, clang::SourceLocation location) const {
    const auto& sources = ast_.getSourceManager();
    location = sources.getExpansionLoc(location);
    if (location.isInvalid() || sources.isInMainFile(location) ||
        sources.isInSystemHeader(location)) {
      return;
    }
    auto file = std::filesystem::path(sources.getFilename(location).str());
    if (file.is_relative()) {
      file = std::filesystem::path(
                 ast_.getSourceManager().getFileManager().getFileSystemOpts().WorkingDir) /
             file;
    }
    const auto absolute = file.lexically_normal().string();
    if (!absolute.empty() &&
        std::find(entry.headers.begin(), entry.headers.end(), absolute) == entry.headers.end()) {
      entry.headers.push_back(absolute);
    }
  }

  clang::ASTContext& ast_;
  std::set<std::string> selectors_;
  std::unique_ptr<clang::MangleContext> mangler_;
};
} // namespace

std::map<std::string, MockDeclaration>
describe_mock_callables(clang::ASTContext& ast, std::span<const std::string> selectors) {
  MockDeclarations visitor(ast, selectors);
  visitor.TraverseDecl(ast.getTranslationUnitDecl());
  return std::move(visitor.declarations);
}
} // namespace tracer::live

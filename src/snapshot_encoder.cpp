#include "snapshot_encoder.hpp"

#include <algorithm>
#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <set>

namespace tracer {
SnapshotEncoders find_snapshot_encoders(clang::ASTContext &ast) {
  SnapshotEncoders result;
  std::set<const clang::FunctionDecl *> seen;
  for (const auto *decl : ast.getTranslationUnitDecl()->decls()) {
    const auto *scope = llvm::dyn_cast<clang::NamespaceDecl>(decl);
    if (!scope || scope->getName() != "tracer_snapshot")
      continue;
    for (const auto *member : scope->decls()) {
      const auto *named = llvm::dyn_cast<clang::NamedDecl>(member);
      if (!named || !named->getIdentifier() || named->getName() != "encode")
        continue;
      result.requested = true;
      const auto *function = llvm::dyn_cast<clang::FunctionDecl>(named);
      if (!function) {
        result.error = "tracer_snapshot::encode must be a non-template function";
        return result;
      }
      if (!seen.insert(function->getCanonicalDecl()).second)
        continue;
      const clang::FunctionDecl *definition = nullptr;
      const auto *prototype = function->getType()->getAs<clang::FunctionProtoType>();
      if (!function->hasBody(definition) || function->isDeleted() ||
          function->getNumParams() != 1 || !prototype || prototype->isVariadic() ||
          !prototype->isNothrow()) {
        result.error = "define tracer_snapshot::encode(const Implementation&) noexcept";
        return result;
      }
      const auto *parameter = function->getParamDecl(0)->getType()->getAs<clang::LValueReferenceType>();
      const auto input = parameter ? parameter->getPointeeType() : clang::QualType();
      if (input.isNull() || !input.isConstQualified() || input.isVolatileQualified() ||
          !input->getAsCXXRecordDecl()) {
        result.error = "tracer_snapshot::encode must take a const reference to an implementation record";
        return result;
      }
      const auto output = function->getReturnType().getCanonicalType().getUnqualifiedType();
      const auto *record = output->getAsCXXRecordDecl();
      if (!record || !record->getDefinition() || !record->getIdentifier() ||
          record->isUnion() || record->getDeclContext()->isFunctionOrMethod()) {
        result.error = "tracer_snapshot::encode must return a named, complete struct or class by value";
        return result;
      }
      const auto canonical = input.getCanonicalType().getUnqualifiedType();
      if (std::any_of(result.encoders.begin(), result.encoders.end(),
                      [&](const auto &prior) { return prior.input == canonical; })) {
        result.error = "multiple tracer_snapshot::encode functions match one implementation";
        return result;
      }
      result.encoders.push_back({canonical, output});
    }
  }
  return result;
}
} // namespace tracer

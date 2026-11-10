#pragma once

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/ExprCXX.h>
#include <clang/Basic/SourceManager.h>

#include <algorithm>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace tracer::record_factory {

struct Argument {
  unsigned index;
  bool move;
};

struct Initializer {
  const clang::FieldDecl *field;
  clang::QualType base;
  std::vector<Argument> arguments;
};

struct Recipe {
  const clang::CXXConstructorDecl *constructor;
  std::vector<const clang::FieldDecl *> fields_by_parameter;
  std::vector<Initializer> initializers;
};

namespace detail {

inline clang::QualType value_type(clang::QualType type) {
  return type.getNonReferenceType().getCanonicalType().getUnqualifiedType();
}

inline bool standard_lifetime(const clang::CXXConstructorDecl &constructor) {
  return constructor.getParent()->isInStdNamespace() &&
         constructor.getASTContext().getSourceManager().isInSystemHeader(
             constructor.getLocation());
}

inline const clang::Expr *unwrap(const clang::Expr *expression) {
  if (const auto *cleanup = llvm::dyn_cast<clang::ExprWithCleanups>(expression))
    return cleanup->getSubExpr();
  if (const auto *temporary = llvm::dyn_cast<clang::CXXBindTemporaryExpr>(expression))
    return temporary->getSubExpr();
  if (const auto *temporary = llvm::dyn_cast<clang::MaterializeTemporaryExpr>(expression))
    return temporary->getSubExpr();
  return expression->IgnoreParens();
}

inline std::optional<Argument>
parameter(const clang::Expr *expression, const clang::CXXConstructorDecl &constructor) {
  const auto expected = value_type(expression->getType());
  bool move = false;
  for (unsigned steps = 0; steps != 32; ++steps) {
    if (value_type(expression->getType()) != expected)
      return std::nullopt;
    const auto *unwrapped = unwrap(expression);
    if (unwrapped != expression) {
      expression = unwrapped;
      continue;
    }
    if (const auto *cast = llvm::dyn_cast<clang::ImplicitCastExpr>(expression)) {
      if (cast->getCastKind() != clang::CK_NoOp &&
          cast->getCastKind() != clang::CK_LValueToRValue)
        return std::nullopt;
      expression = cast->getSubExpr();
      continue;
    }
    if (const auto *construct = llvm::dyn_cast<clang::CXXConstructExpr>(expression)) {
      const auto &special = *construct->getConstructor();
      if (!special.isCopyOrMoveConstructor() || construct->getNumArgs() != 1 ||
          (!special.isTrivial() && !standard_lifetime(special)))
        return std::nullopt;
      expression = construct->getArg(0);
      continue;
    }
    if (const auto *call = llvm::dyn_cast<clang::CallExpr>(expression)) {
      const auto *function = call->getDirectCallee();
      if (move || !function || !function->getIdentifier() || function->getName() != "move" ||
          !function->isInStdNamespace() || call->getNumArgs() != 1 ||
          !constructor.getASTContext().getSourceManager().isInSystemHeader(
              function->getLocation()))
        return std::nullopt;
      move = true;
      expression = call->getArg(0);
      continue;
    }
    const auto *reference = llvm::dyn_cast<clang::DeclRefExpr>(expression);
    const auto *param = reference ? llvm::dyn_cast<clang::ParmVarDecl>(reference->getDecl())
                                  : nullptr;
    if (!param || param->getFunctionScopeIndex() >= constructor.getNumParams() ||
        constructor.getParamDecl(param->getFunctionScopeIndex()) != param ||
        value_type(param->getType()) != expected)
      return std::nullopt;
    return Argument{param->getFunctionScopeIndex(), move};
  }
  return std::nullopt;
}

inline bool field_usable(const clang::FieldDecl &field) {
  const auto type = field.getType().getCanonicalType();
  return field.isUnnamedBitField() ||
         (field.getIdentifier() && !field.isBitField() && !type.isVolatileQualified() &&
          !type->isReferenceType() && !type->isAnyPointerType() &&
          !type->isMemberPointerType() && !type->isArrayType());
}

inline bool fields_usable(const clang::CXXRecordDecl &record,
                          std::set<const clang::CXXRecordDecl *> &seen,
                          unsigned depth) {
  if (depth == 16 || record.getNumVBases() || record.isUnion() ||
      !seen.insert(record.getCanonicalDecl()).second ||
      !std::all_of(record.field_begin(), record.field_end(),
                   [](const auto *field) { return field_usable(*field); }))
    return false;
  for (const auto &base : record.bases()) {
    const auto *base_record = base.getType()->getAsCXXRecordDecl();
    const auto *definition = base_record ? base_record->getDefinition() : nullptr;
    if (!definition || !fields_usable(*definition, seen, depth + 1))
      return false;
  }
  return true;
}

inline const clang::CXXConstructExpr *construction(const clang::Expr *expression) {
  while (const auto *unwrapped = unwrap(expression)) {
    if (unwrapped == expression)
      break;
    expression = unwrapped;
  }
  return llvm::dyn_cast<clang::CXXConstructExpr>(expression);
}

inline bool default_construction(const clang::CXXConstructExpr &construct) {
  const auto &constructor = *construct.getConstructor();
  const auto &record = *constructor.getParent();
  const bool fieldless = (constructor.isImplicit() || constructor.isDefaulted()) &&
                         record.field_empty() && !record.getNumBases();
  return constructor.isDefaultConstructor() && !constructor.isDeleted() &&
         !construct.getNumArgs() &&
         (constructor.isTrivial() || fieldless || standard_lifetime(constructor));
}

inline bool simple_default(const clang::Expr &expression) {
  if (const auto *init = llvm::dyn_cast<clang::CXXDefaultInitExpr>(&expression)) {
    const auto *value = init->getExpr()->IgnoreParenImpCasts();
    if (const auto *reference = llvm::dyn_cast<clang::DeclRefExpr>(value))
      return llvm::isa<clang::EnumConstantDecl>(reference->getDecl());
    return llvm::isa<clang::IntegerLiteral, clang::FloatingLiteral,
                     clang::CharacterLiteral, clang::CXXBoolLiteralExpr>(value);
  }
  const auto *construct = construction(&expression);
  return construct && default_construction(*construct);
}

inline const clang::CXXConstructorDecl *candidate(const clang::CXXRecordDecl &record) {
  for (const auto *decl : record.decls()) {
    if (const auto *templ = llvm::dyn_cast<clang::FunctionTemplateDecl>(decl);
        templ && llvm::isa<clang::CXXConstructorDecl>(templ->getTemplatedDecl()))
      return nullptr;
  }
  const clang::CXXConstructorDecl *selected = nullptr;
  for (const auto *ctor : record.ctors()) {
    if (ctor->isImplicit() || ctor->isDefaultConstructor() || ctor->isCopyOrMoveConstructor())
      continue;
    if (selected || ctor->isDeleted() || ctor->isVariadic() ||
        ctor->getTemplatedKind() != clang::FunctionDecl::TK_NonTemplate)
      return nullptr;
    selected = ctor;
  }
  if (!selected)
    return nullptr;
  const auto *definition = llvm::dyn_cast_or_null<clang::CXXConstructorDecl>(selected->getDefinition());
  const auto *body = definition ? llvm::dyn_cast_or_null<clang::CompoundStmt>(definition->getBody())
                                : nullptr;
  return body && body->body_empty() && !definition->isDelegatingConstructor() ? definition : nullptr;
}

inline std::optional<Recipe> infer(const clang::CXXRecordDecl &record, unsigned depth) {
  if (depth == 16)
    return std::nullopt;
  const auto *ctor = candidate(record);
  if (!ctor || ctor->getNumParams() > 32)
    return std::nullopt;
  for (const auto *param : ctor->parameters()) {
    const auto type = param->getType().getCanonicalType();
    const auto value = type.getNonReferenceType();
    if (param->isParameterPack() || value.isVolatileQualified() || value->isAnyPointerType() ||
        value->isMemberPointerType() ||
        (type->isLValueReferenceType() && !value.isConstQualified()))
      return std::nullopt;
  }

  Recipe recipe{ctor, std::vector<const clang::FieldDecl *>(ctor->getNumParams()), {}};
  const auto bind = [&](const Argument &argument, const clang::FieldDecl *field) {
    if (recipe.fields_by_parameter[argument.index] ||
        std::find(recipe.fields_by_parameter.begin(), recipe.fields_by_parameter.end(), field) !=
            recipe.fields_by_parameter.end())
      return false;
    recipe.fields_by_parameter[argument.index] = field;
    return true;
  };
  for (const auto *init : ctor->inits()) {
    const auto *expression = init->getInit();
    const auto *field = init->getMember();
    if (field && !init->isWritten()) {
      // A synthesized adapter omits in-class defaults. Its decoder can fill
      // assignable fields afterwards, but const fields need the constructor.
      if (field->getType().isConstQualified() || !simple_default(*expression))
        return std::nullopt;
      continue;
    }
    if (field) {
      const auto argument = parameter(expression, *ctor);
      if (!argument || value_type(ctor->getParamDecl(argument->index)->getType()) !=
                           value_type(field->getType()) ||
          !bind(*argument, field))
        return std::nullopt;
      recipe.initializers.push_back({field, {}, {*argument}});
      continue;
    }
    if (!init->isBaseInitializer() || init->isBaseVirtual() || init->isPackExpansion())
      return std::nullopt;
    const auto *construct = construction(expression);
    if (!construct)
      return std::nullopt;
    Initializer replay{nullptr, init->getTypeSourceInfo()->getType(), {}};
    if (default_construction(*construct)) {
      if (init->isWritten())
        recipe.initializers.push_back(std::move(replay));
      continue;
    }
    const auto *base_ctor = construct->getConstructor();
    const auto base = infer(*base_ctor->getParent(), depth + 1);
    if (!base || base->constructor->getCanonicalDecl() != base_ctor->getCanonicalDecl() ||
        construct->getNumArgs() != base->fields_by_parameter.size())
      return std::nullopt;
    for (unsigned index = 0; index != construct->getNumArgs(); ++index) {
      const auto argument = parameter(construct->getArg(index), *ctor);
      if (!argument || value_type(ctor->getParamDecl(argument->index)->getType()) !=
                           value_type(base_ctor->getParamDecl(index)->getType()) ||
          !bind(*argument, base->fields_by_parameter[index]))
        return std::nullopt;
      replay.arguments.push_back(*argument);
    }
    recipe.initializers.push_back(std::move(replay));
  }
  if (std::any_of(recipe.fields_by_parameter.begin(), recipe.fields_by_parameter.end(),
                  [](const auto *field) { return field == nullptr; }))
    return std::nullopt;
  return recipe;
}

} // namespace detail

inline std::optional<Recipe> infer(const clang::CXXRecordDecl &record) {
  const auto *definition = record.getDefinition();
  std::set<const clang::CXXRecordDecl *> seen;
  if (!definition || !detail::fields_usable(*definition, seen, 0))
    return std::nullopt;
  return detail::infer(*definition, 0);
}

} // namespace tracer::record_factory

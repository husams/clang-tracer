#pragma once

#include "tracer.pb.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Attr.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/VTableBuilder.h>
#include <clang/AST/Mangle.h>
#include <clang/Sema/Sema.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/SHA256.h>

#include <memory>
#include <stdexcept>
#include <unordered_set>

namespace tracer::prepared {

inline std::string type_name(clang::ASTContext &ast, clang::QualType type) {
  clang::PrintingPolicy policy(ast.getLangOpts());
  policy.SuppressTagKeyword = true;
  policy.FullyQualifiedName = true;
  return type.getCanonicalType().getAsString(policy);
}

// Fingerprint layouts used by the wrapper and snapshot codec. A pointer is
// part of its containing layout, but the object it points at is not.
inline std::string record_layout(clang::ASTContext &ast,
                                 const clang::CXXRecordDecl &root) {
  llvm::SHA256 hash;
  std::unordered_set<const clang::CXXRecordDecl *> seen;
  const auto add = [&](const std::string &value) {
    hash.update(std::to_string(value.size()) + ":" + value);
  };
  const auto number = [&](auto value) { add(std::to_string(value)); };
  const auto visit = [&](auto &&visit, const clang::CXXRecordDecl *record) -> void {
    record = record->getDefinition();
    if (!record || !seen.insert(record).second)
      return;
    add(type_name(ast, ast.getRecordType(record)));
    const auto &layout = ast.getASTRecordLayout(record);
    number(layout.getSize().getQuantity());
    number(layout.getAlignment().getQuantity());
    number(record->canPassInRegisters());
    if (record->getNumVBases()) {
      // A snapshot's derived-to-virtual-base cast reads these offsets from
      // the original object's vtable. Check the slot as well as the layout.
      auto &vtables = *static_cast<clang::ItaniumVTableContext *>(ast.getVTableContext());
      for (const auto &base : record->vbases()) {
        const auto *decl = base.getType()->getAsCXXRecordDecl()->getDefinition();
        add(type_name(ast, base.getType()));
        number(layout.getVBaseClassOffset(decl).getQuantity());
        number(vtables.getVirtualBaseOffsetOffset(record, decl).getQuantity());
      }
    }
    // An owning pointer's storage does not describe the pointee we read.
    if (const auto *special =
            llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record);
        special && special->getQualifiedNameAsString().rfind("std::", 0) == 0) {
      const auto name = special->getSpecializedTemplate()->getName();
      if (name == "unique_ptr" || name == "shared_ptr") {
        const auto type = special->getTemplateArgs()[0].getAsType();
        if (!type.isVolatileQualified())
          if (const auto *pointee = type->getAsCXXRecordDecl())
            visit(visit, pointee);
      }
    }
    for (const auto &base : record->bases()) {
      const auto *decl = base.getType()->getAsCXXRecordDecl()->getDefinition();
      add(type_name(ast, base.getType()));
      number(base.isVirtual());
      number(base.isVirtual() ? layout.getVBaseClassOffset(decl).getQuantity()
                              : layout.getBaseClassOffset(decl).getQuantity());
      visit(visit, decl);
    }
    for (const auto *field : record->fields()) {
      add(field->getNameAsString());
      add(type_name(ast, field->getType()));
      number(layout.getFieldOffset(field->getFieldIndex()));
      number(field->isBitField() ? field->getBitWidthValue() : 0);
      auto value = field->getType();
      while (const auto *array = ast.getAsArrayType(value))
        value = array->getElementType();
      if (const auto *nested = value->getAsCXXRecordDecl())
        visit(visit, nested);
    }
  };
  visit(visit, &root);
  const auto digest = hash.final();
  return {reinterpret_cast<const char *>(digest.data()), digest.size()};
}

inline void describe_record(clang::ASTContext &ast,
                            const clang::CXXRecordDecl &record,
                            clangtracer::v1::DebugInfo &info) {
  const auto &layout = ast.getASTRecordLayout(&record);
  info.set_record_name(type_name(ast, ast.getRecordType(&record)));
  info.set_record_size(layout.getSize().getQuantity());
  info.set_record_alignment(layout.getAlignment().getQuantity());
  info.set_record_is_opaque(true);
  info.set_record_pass_by_reference(!record.canPassInRegisters());
  info.set_record_calling_convention(record.canPassInRegisters()
      ? clangtracer::v1::RECORD_PASS_BY_VALUE
      : clangtracer::v1::RECORD_PASS_BY_REFERENCE);
  info.set_prepared_layout_sha256(record_layout(ast, record));
}

struct PreparedType {
  clangtracer::v1::ValueType wire;
  std::string spelling;
  const clang::CXXRecordDecl *record;
  bool reference;
};

inline const clang::CXXRecordDecl *prepared_pointer_record(clang::QualType type) {
  if (!type->isPointerType())
    return nullptr;
  const auto *record = type->getPointeeType()->getAsCXXRecordDecl();
  if (record)
    record = record->getDefinition();
  return record && !record->isUnion() && !record->isInStdNamespace() ? record : nullptr;
}

inline PreparedType prepared_type(clang::ASTContext &ast, clang::QualType type,
                           bool result) {
  type = type.getCanonicalType().getUnqualifiedType();
  const auto object = type.getNonReferenceType();
  if (object.getAddressSpace() != clang::LangAS::Default)
    throw std::runtime_error("non-default address-space types are unsupported");
  const auto *record = object->getAsCXXRecordDecl();
  if (record)
    record = record->getDefinition();
  const auto spelling = type_name(ast, type);
  if (type->isReferenceType())
    return {clangtracer::v1::POINTER, spelling, record, true};
  if (type->isRecordType()) {
    if (!record)
      throw std::runtime_error("by-value types require a complete C++ record");
    if (record->hasAttr<clang::TrivialABIAttr>() &&
        (!record->hasTrivialCopyConstructor() || !record->hasTrivialDestructor()))
      throw std::runtime_error("nontrivial [[clang::trivial_abi]] records are unsupported");
    return {clangtracer::v1::OBJECT, spelling, record, false};
  }
  if (type->isBooleanType())
    return {clangtracer::v1::BOOL, spelling, nullptr, false};
  if (!type->isBitIntType() && (type->isIntegerType() || type->isEnumeralType())) {
    const auto bits = ast.getTypeSize(type);
    const bool is_signed = type->isSignedIntegerOrEnumerationType();
    if (bits <= 32 && is_signed)
      return {clangtracer::v1::INT32, spelling, nullptr, false};
    if (bits <= 64)
      return {is_signed ? clangtracer::v1::INT64 : clangtracer::v1::UINT64, spelling, nullptr, false};
  }
  if (type->isVoidType() && result)
    return {clangtracer::v1::VOID, spelling, nullptr, false};
  if (type->isPointerType() &&
      type->getPointeeType().getAddressSpace() == clang::LangAS::Default &&
      ast.getTypeSize(type) == 64)
    return {clangtracer::v1::POINTER, spelling, nullptr, false};
  if (type->isSpecificBuiltinType(clang::BuiltinType::Float))
    return {clangtracer::v1::FLOAT32, spelling, nullptr, false};
  if (type->isSpecificBuiltinType(clang::BuiltinType::Double))
    return {clangtracer::v1::FLOAT64, spelling, nullptr, false};
  throw std::runtime_error("unsupported prepared ABI type: " + spelling);
}

inline void describe_prepared_callable(clang::ASTContext &ast,
                               const clang::FunctionDecl &declaration,
                               const clang::CXXMethodDecl *method,
                               clangtracer::v1::ProbeSpec &spec,
                               clang::Sema *sema = nullptr,
                               const clangtracer::v1::DebugInfo *expected = nullptr,
                               bool pointer_layouts = true) {
  auto &signature = *spec.mutable_signature();
  auto &info = *spec.mutable_debug_info();
  if (method && !method->isStatic()) {
    describe_record(ast, *method->getParent(), info);
    info.set_receiver_volatile(method->isVolatile());
  }
  const auto inspect = [&](clang::QualType type, bool is_result, bool pointer_layout) {
    const auto referent = type.getNonReferenceType();
    if (sema && referent->isRecordType() && referent->isIncompleteType())
      (void)sema->isCompleteType(declaration.getLocation(), referent);
    auto description = prepared_type(ast, type, is_result);
    if (pointer_layouts && pointer_layout && description.wire == clangtracer::v1::POINTER)
      if (const auto *pointee = prepared_pointer_record(type))
        description.record = pointee;
    return description;
  };
  // Earlier standalone packages could declare a pointee without recording
  // its layout. Verify only the pointer layouts promised by a loaded package.
  const auto result = inspect(declaration.getReturnType(), true,
                              !expected || expected->has_result_object_type());
  signature.set_result(result.wire);
  info.set_result_cpp_type(result.spelling);
  info.set_result_is_reference(result.reference);
  info.set_result_is_object_value(result.wire == clangtracer::v1::OBJECT);
  if (result.record) {
    describe_record(ast, *result.record,
                                     *info.mutable_result_object_type());
    if (result.reference || result.wire == clangtracer::v1::OBJECT)
      info.set_result_pass_by_reference(!result.record->canPassInRegisters());
  }
  for (const auto *parameter : declaration.parameters()) {
    const auto index = info.arguments_size();
    const auto type = inspect(parameter->getType(), false, !expected ||
        (index < expected->arguments_size() && expected->arguments(index).has_object_type()));
    signature.add_arguments(type.wire);
    auto &argument = *info.add_arguments();
    argument.set_index(info.arguments_size() - 1);
    argument.set_cpp_type(type.spelling);
    argument.set_is_reference(type.reference);
    argument.set_is_object_value(type.wire == clangtracer::v1::OBJECT);
    if (type.record)
      describe_record(ast, *type.record,
                                       *argument.mutable_object_type());
  }
}

inline std::string mangle(const clang::FunctionDecl* declaration, clang::ASTContext& context) {
  auto mangler = std::unique_ptr<clang::MangleContext>(context.createMangleContext());
  if (!mangler->shouldMangleDeclName(declaration)) {
    if (declaration->getLanguageLinkage() == clang::CLanguageLinkage) {
      return declaration->getNameAsString();
    }
    throw std::runtime_error("callable has no supported C++ mangled symbol");
  }
  std::string symbol;
  llvm::raw_string_ostream output(symbol);
  mangler->mangleName(clang::GlobalDecl(declaration), output);
  output.flush();
  return symbol;
}

inline bool template_instance(const clang::FunctionDecl &declaration) {
  if (declaration.getTemplateSpecializationArgs())
    return true;
  if (const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(&declaration))
    return llvm::isa<clang::ClassTemplateSpecializationDecl>(method->getParent());
  return false;
}

inline bool adapter_callable(const clang::FunctionDecl &declaration,
                             clang::ASTContext &ast, const std::string &name,
                             const std::string &symbol) {
  if (const auto *label = declaration.getAttr<clang::AsmLabelAttr>())
    return label->getLabel() == symbol;
  return declaration.getQualifiedNameAsString() == name &&
         !declaration.getDeclContext()->isDependentContext() &&
         !declaration.getType()->isDependentType() &&
         mangle(&declaration, ast) == symbol;
}

} // namespace tracer::prepared

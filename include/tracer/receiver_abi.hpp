#pragma once

#include <string>

#include <clang/AST/ASTContext.h>
#include <clang/AST/Type.h>
#include <llvm/ADT/SmallString.h>

namespace tracer::receiver_abi {

// Describes ABI-relevant properties that a type spelling cannot capture alone,
// such as target-dependent builtin widths and signedness.
inline std::string type_identity(clang::QualType input, const clang::ASTContext& context) {
  const auto type = input.getCanonicalType();
  if (const auto* array = llvm::dyn_cast<clang::ConstantArrayType>(type.getTypePtr())) {
    llvm::SmallString<32> count;
    array->getSize().toString(count, 10, false);
    return "array[count=" + count.str().str() +
           ";element=" + type_identity(array->getElementType(), context) + "]";
  }
  if (type->isPointerType()) {
    const auto pointer = type->getAs<clang::PointerType>();
    const auto pointee = pointer->getPointeeType();
    return "pointer[size=" + std::to_string(context.getTypeSize(type)) +
           ";align=" + std::to_string(context.getTypeAlign(type)) +
           ";address_space=" + std::to_string(static_cast<unsigned>(type.getAddressSpace())) +
           ";pointee_address_space=" +
           std::to_string(static_cast<unsigned>(pointee.getAddressSpace())) +
           ";pointee=" + type_identity(pointee, context) + "]";
  }
  if (const auto* builtin = llvm::dyn_cast<clang::BuiltinType>(type.getTypePtr())) {
    std::string identity =
        "builtin[kind=" + std::to_string(static_cast<unsigned>(builtin->getKind()));
    if (!type->isVoidType()) {
      identity += ";size=" + std::to_string(context.getTypeSize(type)) +
                  ";align=" + std::to_string(context.getTypeAlign(type));
    }
    if (type->isIntegerType()) {
      identity += type->isSignedIntegerType() ? ";signed=1" : ";signed=0";
    }
    return identity + "]";
  }
  return "unsupported[" + type.getAsString() + "]";
}

} // namespace tracer::receiver_abi

#pragma once

#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>
#include <llvm/ADT/StringRef.h>

#include <llvm/ADT/SmallVector.h>
#include <clang/AST/DeclTemplate.h>
#include <set>
#include <string>
#include <vector>

// Raw pointer and reference members that a prepared snapshot can follow.
namespace tracer::snapshot_members {

// The type a pointer or reference member refers to, without cv-qualifiers.
inline clang::QualType referent(clang::QualType type) {
  return type->getPointeeType();
}

inline bool followable(clang::QualType type) {
  return (type->isPointerType() || type->isReferenceType()) &&
         !type.isVolatileQualified() && !referent(type).isVolatileQualified();
}

inline const clang::CXXRecordDecl *referent_record(clang::QualType type) {
  const auto *record = referent(type)->getAsCXXRecordDecl();
  return record ? record->getDefinition() : nullptr;
}

// Data members declared by `record` and by its public, non-virtual bases.
inline void collect(const clang::CXXRecordDecl *record,
                    std::vector<const clang::FieldDecl *> &fields) {
  record = record ? record->getDefinition() : nullptr;
  if (!record)
    return;
  for (const auto &base : record->bases()) {
    const auto access = base.getAccessSpecifier();
    if (base.isVirtual() ||
        !(access == clang::AS_public ||
          (access == clang::AS_none && record->isStruct())))
      continue;
    collect(base.getType()->getAsCXXRecordDecl(), fields);
  }
  for (const auto *field : record->fields())
    fields.push_back(field);
}

inline std::vector<const clang::FieldDecl *> fields(const clang::CXXRecordDecl *record) {
  std::vector<const clang::FieldDecl *> result;
  collect(record, result);
  return result;
}

// A named member of `record` or one of its bases; null when absent or when
// the name is declared more than once.
inline const clang::FieldDecl *find(const clang::CXXRecordDecl *record,
                                    llvm::StringRef name) {
  const clang::FieldDecl *found = nullptr;
  for (const auto *field : fields(record)) {
    if (field->getName() != name)
      continue;
    if (found)
      return nullptr;
    found = field;
  }
  return found;
}


// The record a std::unique_ptr (default deleter) or std::shared_ptr owns.
inline const clang::CXXRecordDecl *smart_record(clang::QualType type) {
  const auto *record = type.getCanonicalType()->getAsCXXRecordDecl();
  const auto *special =
      llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(record);
  if (!special || !special->isInStdNamespace())
    return nullptr;
  const auto name = special->getName();
  if (name != "unique_ptr" && name != "shared_ptr")
    return nullptr;
  const auto &args = special->getTemplateArgs();
  if (!args.size() || args[0].getKind() != clang::TemplateArgument::Type)
    return nullptr;
  if (name == "unique_ptr") {
    if (args.size() != 2 || args[1].getKind() != clang::TemplateArgument::Type)
      return nullptr;
    const auto *deleter = llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
        args[1].getAsType()->getAsCXXRecordDecl());
    if (!deleter || !deleter->isInStdNamespace() || deleter->getName() != "default_delete")
      return nullptr;
  }
  const auto *pointee = args[0].getAsType()->getAsCXXRecordDecl();
  pointee = pointee ? pointee->getDefinition() : nullptr;
  return pointee && !pointee->isInStdNamespace() && !pointee->isUnion() ? pointee : nullptr;
}

// One hop along a member path. `deref` means the field is a smart pointer and
// the next hop continues in its pointee.
struct Step {
  const clang::FieldDecl *field = nullptr;
  bool deref = false;
};

// Resolves "a", "nested.a" or "owned.*.a" (a smart pointer's pointee) from
// `root`. The last hop must be a followable pointer or reference member.
inline bool resolve(const clang::CXXRecordDecl *root, llvm::StringRef path,
                    std::vector<Step> &steps) {
  steps.clear();
  llvm::SmallVector<llvm::StringRef, 6> parts;
  path.split(parts, '.');
  const clang::CXXRecordDecl *record = root;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    const auto *field = record ? find(record, parts[i]) : nullptr;
    if (!field)
      return false;
    if (i + 1 == parts.size()) {
      steps.push_back({field, false});
      return followable(field->getType());
    }
    if (parts[i + 1] == "*") {
      record = smart_record(field->getType());
      if (!record || i + 2 >= parts.size())
        return false;
      steps.push_back({field, true});
      ++i;
      continue;
    }
    record = field->getType().getCanonicalType()->getAsCXXRecordDecl();
    record = record ? record->getDefinition() : nullptr;
    if (!record || record->isInStdNamespace() || record->isUnion())
      return false;
    steps.push_back({field, false});
  }
  return false;
}

// The name a followed member's snapshot is stored under in the event.
inline std::string wire_path(llvm::StringRef path) {
  std::string result;
  llvm::SmallVector<llvm::StringRef, 6> parts;
  path.split(parts, '.');
  for (const auto part : parts) {
    if (!result.empty())
      result += '.';
    result += part == "*" ? "value" : part.str();
  }
  return result + "_snapshot";
}

// True when every byte the capture codecs read for `type` lies in the object
// itself or in storage the checked codecs verify first (strings, contiguous
// vectors, smart pointer pointees). Node-based containers, std::any,
// std::function, weak_ptr and unknown standard types read memory that cannot
// be verified, so an object holding them is only followed with evidence that
// it is a live object (a matching RTTI) or when the user names it.
inline bool checked_safe(clang::QualType type, std::set<const clang::CXXRecordDecl *> &seen) {
  type = type.getCanonicalType().getUnqualifiedType();
  if (type->isPointerType() || type->isReferenceType() || type->isArithmeticType() ||
      type->isEnumeralType())
    return true;
  if (const auto *array = type->getAsArrayTypeUnsafe())
    return checked_safe(array->getElementType(), seen);
  const auto *record = type->getAsCXXRecordDecl();
  record = record ? record->getDefinition() : nullptr;
  if (!record || record->isUnion())
    return false;
  if (!seen.insert(record->getCanonicalDecl()).second)
    return true;
  if (record->isInStdNamespace()) {
    const auto *special = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record);
    const auto name = record->getName();
    if (!special)
      return false;
    const auto &args = special->getTemplateArgs();
    const auto type_arg = [&](unsigned index) {
      return index < args.size() && args[index].getKind() == clang::TemplateArgument::Type
                 ? args[index].getAsType()
                 : clang::QualType();
    };
    if (name == "basic_string" || name == "basic_string_view")
      return !type_arg(0).isNull() && type_arg(0)->isCharType();
    if (name == "vector") {
      const auto element = type_arg(0);
      return !element.isNull() && !element.getCanonicalType()->isBooleanType() &&
             checked_safe(element, seen);
    }
    if (name == "array" || name == "optional" || name == "unique_ptr" || name == "shared_ptr") {
      const auto element = type_arg(0);
      return !element.isNull() && checked_safe(element, seen);
    }
    if (name == "pair" || name == "tuple" || name == "variant") {
      // An element that is a reference is encoded through it.
      for (const auto &argument : args.asArray()) {
        if (argument.getKind() == clang::TemplateArgument::Type) {
          if (argument.getAsType()->isReferenceType() || !checked_safe(argument.getAsType(), seen))
            return false;
        } else if (argument.getKind() == clang::TemplateArgument::Pack) {
          for (const auto &item : argument.pack_elements())
            if (item.getKind() != clang::TemplateArgument::Type ||
                item.getAsType()->isReferenceType() || !checked_safe(item.getAsType(), seen))
              return false;
        }
      }
      return true;
    }
    return false;
  }
  // Encoding a virtual base loads its offset through the object's vptr.
  if (record->getNumVBases() != 0)
    return false;
  for (const auto &base : record->bases())
    if (!checked_safe(base.getType(), seen))
      return false;
  for (const auto *field : record->fields())
    if (!checked_safe(field->getType(), seen))
      return false;
  return true;
}
inline bool checked_safe(clang::QualType type) {
  std::set<const clang::CXXRecordDecl *> seen;
  return checked_safe(type, seen);
}

// Pointer members whose pointee is not a record: C strings and single scalars.
enum class Referent { record, text, scalar, other };

inline Referent classify(clang::QualType type) {
  const auto pointee = referent(type).getUnqualifiedType().getCanonicalType();
  if (referent_record(type))
    return Referent::record;
  if (type->isPointerType() && pointee->isCharType() &&
      !pointee->isSpecificBuiltinType(clang::BuiltinType::UChar))
    return Referent::text;
  if (const auto *enumeration = pointee->getAs<clang::EnumType>()) {
    // An enum that cannot be named from generated code cannot be followed.
    const auto *decl = enumeration->getDecl();
    const bool nameable = decl->getIdentifier() && !decl->isInAnonymousNamespace() &&
                          !decl->getDeclContext()->isFunctionOrMethod() &&
                          (decl->getAccess() == clang::AS_public || decl->getAccess() == clang::AS_none);
    return nameable ? Referent::scalar : Referent::other;
  }
  if (pointee->isArithmeticType())
    return Referent::scalar;
  return Referent::other;
}

} // namespace tracer::snapshot_members

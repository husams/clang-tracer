#include "value_snapshot_codegen.hpp"
#include "context_paths.hpp"
#include "context_schema.hpp"
#include "tracer.pb.h"
#include "prepared_contract.hpp"
#include "prepared_types.hpp"
#include "snapshot_encoder.hpp"
#include "snapshot_limits.hpp"
#include "snapshot_members.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/AST/CXXInheritance.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Type.h>
#include <clang/AST/VTableBuilder.h>
#include <clang/Basic/TargetInfo.h>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <google/protobuf/descriptor.pb.h>
#include <limits>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Triple.h>

namespace tracer {
namespace {
std::string literal(const std::string &text) {
  std::ostringstream out;
  out << '"';
  for (unsigned char byte : text)
    out << '\\' << std::oct << std::setw(3) << std::setfill('0')
        << unsigned(byte);
  out << '"';
  return out.str();
}
std::string message_name(const std::string &name) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (unsigned char byte : name) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  std::ostringstream out;
  out << "M_" << std::hex << hash;
  return out.str();
}
clang::QualType alias(clang::ASTContext &ast, llvm::StringRef namespace_name,
                      llvm::StringRef alias_name) {
  for (const auto *decl : ast.getTranslationUnitDecl()->decls()) {
    const auto *space = llvm::dyn_cast<clang::NamespaceDecl>(decl);
    if (!space || space->getName() != namespace_name)
      continue;
    for (const auto *member : space->decls()) {
      const auto *alias = llvm::dyn_cast<clang::TypeAliasDecl>(member);
      if (alias && alias->getName() == alias_name)
        return alias->getUnderlyingType();
    }
  }
  return {};
}
clang::QualType tracer_alias(clang::ASTContext &ast, llvm::StringRef name) {
  return alias(ast, "tracer", name);
}
clang::QualType prepared_alias(clang::ASTContext &ast, llvm::StringRef name) {
  return alias(ast, "tracer_prepared", name);
}
bool supports_itanium_dynamic_rtti(clang::ASTContext &ast) {
  const auto &target = ast.getTargetInfo();
  const auto *vtable = llvm::dyn_cast<clang::ItaniumVTableContext>(
      ast.getVTableContext());
  return ast.getLangOpts().RTTI &&
         target.getTriple().getArch() == llvm::Triple::x86_64 &&
         target.getTriple().isOSLinux() && target.isLittleEndian() &&
         target.getPointerWidth(clang::LangAS::Default) == 64 &&
         target.getLongWidth() == 64 && target.getCXXABI().isItaniumFamily() &&
         vtable && !vtable->isRelativeLayout();
}
std::string itanium_rtti_type_name(clang::ASTContext &ast,
                                   clang::QualType type) {
  auto mangler = std::unique_ptr<clang::MangleContext>(
      ast.createMangleContext());
  std::string name;
  llvm::raw_string_ostream out(name);
  mangler->mangleCXXRTTIName(type.getCanonicalType(), out);
  out.flush();
  llvm::StringRef rtti_name(name);
  if (rtti_name.consume_front("_ZTS"))
    return rtti_name.str();
  return {};
}
bool codec_types_supported(clang::QualType type,
                           std::set<const clang::CXXRecordDecl *> &seen) {
  if (type.getNonReferenceType().isVolatileQualified())
    return false;
  if (const auto *array = type->getAsArrayTypeUnsafe())
    return codec_types_supported(array->getElementType(), seen);
  const auto *record = type.getNonReferenceType()->getAsCXXRecordDecl();
  if (!record || !seen.insert(record->getCanonicalDecl()).second)
    return true;
  // The any codec reads the active payload through the public interface.
  if (record->isInStdNamespace() && record->getName() == "any")
    return true;
  if (const auto *special =
          llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record)) {
    const auto name =
        special->getSpecializedTemplate()->getQualifiedNameAsString();
    const auto &args = special->getTemplateArgs();
    if (special->getSpecializedTemplate()->getName() == "basic_string" &&
        name.rfind("std::", 0) == 0) {
      if (args.size() == 0 ||
          args[0].getKind() != clang::TemplateArgument::Type)
        return false;
      const auto *character = args[0].getAsType()->getAs<clang::BuiltinType>();
      if (!character || (character->getKind() != clang::BuiltinType::Char_S &&
                         character->getKind() != clang::BuiltinType::Char_U))
        return false;
      if (args.size() < 3)
        return false;
      for (unsigned i = 1; i < 3; ++i) {
        if (args[i].getKind() != clang::TemplateArgument::Type)
          return false;
        const auto *parameter =
            llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
                args[i].getAsType()->getAsCXXRecordDecl());
        if (!parameter)
          return false;
        const auto primary =
            parameter->getSpecializedTemplate()->getQualifiedNameAsString();
        if (primary.rfind("std::", 0) != 0 ||
            primary.find(i == 1 ? "char_traits" : "allocator") ==
                std::string::npos)
          return false;
      }
    }
    const auto primary = special->getSpecializedTemplate()->getName();
    if (name.rfind("std::", 0) == 0 &&
        (primary == "unique_ptr" || primary == "shared_ptr" || primary == "weak_ptr")) {
      const auto pointee = args[0].getAsType();
      if (primary == "weak_ptr" || !pointee->isObjectType() || pointee->isArrayType() ||
          pointee->isIncompleteType() || pointee.isVolatileQualified())
        return true;
      return codec_types_supported(pointee, seen);
    }
    if (name.rfind("std::", 0) == 0) {
      const auto supported = [&](auto &&check, const clang::TemplateArgument &argument) -> bool {
        if (argument.getKind() == clang::TemplateArgument::Pack)
          return std::all_of(argument.pack_begin(), argument.pack_end(),
                             [&](const auto &item) { return check(check, item); });
        return argument.getKind() != clang::TemplateArgument::Type ||
               codec_types_supported(argument.getAsType(), seen);
      };
      for (const auto &argument : args.asArray())
        if (!supported(supported, argument))
          return false;
      return true;
    }
  }
  record = record->getDefinition();
  if (!record)
    return false;
  for (const auto *member : record->fields())
    if (!codec_types_supported(member->getType(), seen))
      return false;
  for (const auto &base : record->bases())
    if (!codec_types_supported(base.getType(), seen))
      return false;
  return true;
}
std::string schema_bytes(const context_engine::SchemaRecord &schema,
                         std::vector<std::string> &types) {
  google::protobuf::FileDescriptorSet files;
  if (!files.ParseFromString(schema.descriptor_set))
    return {};
  std::map<std::string, std::string> names;
  std::set<const context_engine::SchemaMessage *> visited;
  std::function<void(const context_engine::SchemaField &)> field;
  std::function<void(const context_engine::SchemaMessage &)> message;
  message = [&](const auto &value) {
    if (!visited.insert(&value).second)
      return;
    names[message_name(value.cpp_type)] = value.cpp_type;
    for (const auto &item : value.fields)
      field(item);
  };
  field = [&](const auto &value) {
    if (value.message)
      message(*value.message);
    if (value.value)
      field(*value.value);
    if (value.key)
      field(*value.key);
  };
  message(*schema.root);
  for (auto &file : *files.mutable_file()) {
    for (auto &record : *file.mutable_message_type()) {
      const auto found = names.find(record.name());
      if (found != names.end()) {
        types.push_back(file.package() + "." + record.name() + "=" +
                        found->second);
      }
      for (auto &member : *record.mutable_field()) {
        // std::string owns arbitrary bytes, including invalid UTF-8. The codec
        // has the same length-delimited wire form for protobuf bytes/string.
        if (member.type() ==
                google::protobuf::FieldDescriptorProto::TYPE_STRING &&
            !(record.options().map_entry() && member.number() == 1))
          member.set_type(google::protobuf::FieldDescriptorProto::TYPE_BYTES);
      }
    }
  }
  return files.SerializeAsString();
}
} // namespace

ValueSnapshotCode
make_value_snapshot_code(clang::ASTContext &ast,
                         const clangtracer::v1::ProbeSpec &spec, bool follow_members) {
  ValueSnapshotCode code;
  const auto snapshot_encoders = tracer::find_snapshot_encoders(ast);
  code.has_custom_encoder = snapshot_encoders.requested;
  code.error = snapshot_encoders.error;
  if (!spec.has_debug_info()) {
    if (snapshot_encoders.requested && code.error.empty())
      code.error = "tracer_snapshot::encode requires a prepared result snapshot";
    return code;
  }
  const auto signature = tracer_alias(ast, "ResolvedSignature");
  if (signature.isNull()) {
    if (snapshot_encoders.requested && code.error.empty())
      code.error = "tracer_snapshot::encode requires a prepared callable signature";
    return code;
  }
  const auto *function = signature->getAs<clang::FunctionProtoType>();
  if (!function) {
    if (snapshot_encoders.requested && code.error.empty())
      code.error = "tracer_snapshot::encode requires a function result";
    return code;
  }
  enum class Role { argument, result, receiver };
  struct SnapshotProjection {
    clang::QualType type;
    clang::QualType payload_type;
    std::string cpp_type;
    std::string payload_cpp_type;
    std::string rtti_name;
    context_engine::SchemaResult schema;
    std::string error;
    bool custom_encoder = false;
  };
  struct FollowedMember {
    const clang::FieldDecl *field = nullptr;
    std::string name; // Member path: "field", "nested.field" or "owned.*.field".
    std::size_t accessor_id = 0;
    std::vector<SnapshotProjection> projections;
    bool polymorphic = false;
    bool automatic = false;
    snapshot_members::Referent kind = snapshot_members::Referent::record;
  };
  struct Entry {
    unsigned index;
    Role role;
    bool result_pointee;
    clang::QualType type;
    std::string cpp_type;
    context_engine::SchemaResult schema;
    std::string error;
    std::vector<SnapshotProjection> dynamic_projections;
    std::vector<FollowedMember> members;
  };
  std::vector<Entry> entries;
  const auto &info = spec.debug_info();
  const auto access = tracer::prepared::is_typed_version(spec.prepared().format_version())
      ? context_engine::CodecAccess::snapshot_members
      : context_engine::CodecAccess::public_members;
  const auto prepare_schema = [&](Entry &entry, clang::QualType type,
                                  bool is_volatile,
                                  llvm::StringRef error_subject) {
    context_engine::StateFamilyKey family;
    family.source_lineage_id[0] = 1;
    if (const auto *record = type->getAsCXXRecordDecl();
        record && !llvm::isa<clang::ClassTemplateSpecializationDecl>(record))
      family.cpp_state_type = record->getQualifiedNameAsString();
    else
      family.cpp_state_type = type.getAsString();
    family.cpp_scope_type = "tracer::EventValueSnapshot";
    context_engine::SchemaLimits limits;
    limits.descriptor_bytes = 4096;
    limits.fields = 128;
    limits.depth = 16;
    entry.schema = context_engine::build_payload_schema(
        ast, type, std::move(family), limits, access);
    const auto codec =
        context_engine::render_payload_codec_source(ast, type, access);
    if (!entry.schema)
      entry.error = entry.schema.error;
    else if (!codec)
      entry.error = codec.error;
    std::set<const clang::CXXRecordDecl *> seen;
    if (!codec_types_supported(type, seen))
      entry.error = "unsupported string character type in " +
                    error_subject.str();
    if (is_volatile)
      entry.error = "volatile values cannot be snapshotted automatically";
    if (entry.schema) {
      google::protobuf::FileDescriptorSet descriptors;
      descriptors.ParseFromString(entry.schema.record->descriptor_set);
      for (const auto &file : descriptors.file())
        for (const auto &message : file.message_type())
          for (const auto &field : message.field())
            if (field.options().uninterpreted_option_size())
              entry.error = "snapshot field name cannot be represented by this schema";
    }
  };
  const auto add = [&](unsigned index, Role role, clang::QualType type,
                       bool result_pointee = false) {
    const bool is_volatile = type.getNonReferenceType().isVolatileQualified();
    type = type.getNonReferenceType().getUnqualifiedType().getCanonicalType();
    if (!result_pointee && (type->isPointerType() || type->isVoidType()))
      return;
    clang::PrintingPolicy policy(ast.getLangOpts());
    policy.SuppressTagKeyword = true;
    Entry entry{};
    entry.index = index;
    entry.role = role;
    entry.result_pointee = result_pointee;
    entry.type = type;
    entry.cpp_type = type.getAsString(policy);
    if (result_pointee) {
      const auto *record = type->getAsCXXRecordDecl();
      if (!record || !record->getDefinition()) {
        entry.error = "return-pointee snapshots require a complete record type";
        entries.push_back(std::move(entry));
        return;
      }
      if (!info.has_result_object_type()) {
        entry.error =
            "return-pointee snapshots require recovered pointee record metadata";
        entries.push_back(std::move(entry));
        return;
      }
    }
    prepare_schema(entry, type, is_volatile, "snapshot");
    entries.push_back(std::move(entry));
  };
  clang::PrintingPolicy snapshot_policy(ast.getLangOpts());
  snapshot_policy.SuppressTagKeyword = true;
  const auto make_projection = [&](clang::QualType type,
                                   const clangtracer::v1::DebugInfo &metadata,
                                   clang::QualType static_type,
                                   bool dynamic) {
    SnapshotProjection projection;
    if (type.isNull()) {
      projection.error = "prepared snapshot type alias is unavailable";
      return projection;
    }
    projection.type = type.getCanonicalType().getUnqualifiedType();
    projection.cpp_type = type.getAsString(snapshot_policy);
    const auto *record = type->getAsCXXRecordDecl();
    record = record ? record->getDefinition() : nullptr;
    if (!record || record->isUnion() || record->isAbstract()) {
      projection.error = "prepared snapshot type is not a complete concrete record";
      return projection;
    }
    clangtracer::v1::DebugInfo actual_layout;
    tracer::prepared::describe_record(ast, *record, actual_layout);
    if (actual_layout.SerializeAsString() != metadata.SerializeAsString()) {
      projection.error = "prepared snapshot record layout does not match metadata";
      return projection;
    }
    const auto *base = static_type->getAsCXXRecordDecl();
    base = base ? base->getDefinition() : nullptr;
    if (!base) {
      projection.error = "snapshot pointer does not point to a complete record";
      return projection;
    }
    if (record->getCanonicalDecl() != base->getCanonicalDecl()) {
      if (!dynamic || !base->isPolymorphic()) {
        projection.error = "prepared snapshot type does not match its static pointee";
        return projection;
      }
      clang::CXXBasePaths paths;
      if (!record->isDerivedFrom(base, paths) ||
          paths.isAmbiguous(ast.getCanonicalType(ast.getRecordType(base)))) {
        projection.error = "prepared snapshot type is not an unambiguous derived pointee";
        return projection;
      }
      const bool public_path = std::any_of(
          paths.begin(), paths.end(), [](const auto &path) {
            return path.Access == clang::AS_public;
          });
      if (!public_path) {
        projection.error = "prepared snapshot type has no public base path";
        return projection;
      }
      projection.rtti_name = itanium_rtti_type_name(ast, type);
      if (projection.rtti_name.empty()) {
        projection.error = "cannot mangle prepared snapshot RTTI type";
        return projection;
      }
      if (!supports_itanium_dynamic_rtti(ast)) {
        projection.error = "runtime snapshot dispatch is unavailable for this target ABI";
        return projection;
      }
    } else if (dynamic && base->isPolymorphic()) {
      projection.rtti_name = itanium_rtti_type_name(ast, type);
      if (projection.rtti_name.empty() ||
          !supports_itanium_dynamic_rtti(ast)) {
        projection.error = "runtime snapshot dispatch is unavailable for this target ABI";
        return projection;
      }
    }
    projection.payload_type = projection.type;
    projection.payload_cpp_type = projection.cpp_type;
    for (const auto &encoder : snapshot_encoders.encoders) {
      if (encoder.input.getCanonicalType().getUnqualifiedType() ==
          projection.type) {
        projection.custom_encoder = true;
        projection.payload_type = encoder.output.getCanonicalType();
        projection.payload_cpp_type =
            encoder.output.getAsString(snapshot_policy);
        break;
      }
    }
    std::set<const clang::CXXRecordDecl *> seen;
    if (!codec_types_supported(projection.payload_type, seen)) {
      projection.error = "unsupported type in prepared snapshot payload";
      return projection;
    }
    Entry schema_entry{};
    schema_entry.cpp_type = projection.payload_cpp_type;
    prepare_schema(schema_entry, projection.payload_type, false,
                   "prepared snapshot payload");
    projection.schema = std::move(schema_entry.schema);
    projection.error = std::move(schema_entry.error);
    return projection;
  };
  const auto registry_type = [&](unsigned index) {
    return prepared_alias(ast, "SnapshotType" + std::to_string(index));
  };
  const auto record_metadata = [&](unsigned type_index)
      -> const clangtracer::v1::DebugInfo * {
    if (type_index >= static_cast<unsigned>(spec.prepared().snapshot_types_size()))
      return nullptr;
    return &spec.prepared().snapshot_types(static_cast<int>(type_index));
  };
  // The matching prepared policy for a record, after confirming its layout.
  const auto find_policy = [&](const clang::CXXRecordDecl *static_record)
      -> const clangtracer::v1::PreparedSnapshotRecord * {
    static_record = static_record ? static_record->getDefinition() : nullptr;
    if (!static_record)
      return nullptr;
    for (const auto &policy : spec.prepared().snapshot_records()) {
      const auto *metadata = record_metadata(policy.type_index());
      const auto alias_type = registry_type(policy.type_index());
      const auto *candidate = alias_type.isNull()
                                  ? nullptr
                                  : alias_type->getAsCXXRecordDecl();
      candidate = candidate ? candidate->getDefinition() : nullptr;
      if (!metadata || !candidate ||
          candidate->getCanonicalDecl() != static_record->getCanonicalDecl())
        continue;
      clangtracer::v1::DebugInfo actual_root_layout;
      tracer::prepared::describe_record(ast, *static_record, actual_root_layout);
      if (actual_root_layout.SerializeAsString() != metadata->SerializeAsString())
        continue;
      return &policy;
    }
    return nullptr;
  };
  // std::string_view reads a captured C string without allocating; std::string
  // is the fallback when the former is not in the translation unit.
  const auto text_type = [&]() -> clang::QualType {
    clang::QualType fallback;
    for (const auto *decl : ast.getTranslationUnitDecl()->decls()) {
      const auto *space = llvm::dyn_cast<clang::NamespaceDecl>(decl);
      if (!space || space->getName() != "std")
        continue;
      for (const auto *member : space->decls())
        if (const auto *alias = llvm::dyn_cast<clang::TypedefNameDecl>(member)) {
          if (alias->getName() == "string_view")
            return alias->getUnderlyingType();
          if (alias->getName() == "string")
            fallback = alias->getUnderlyingType();
        }
    }
    return fallback;
  };
  const auto make_leaf_projection = [&](clang::QualType type, const std::string &cpp_type) {
    SnapshotProjection projection;
    if (type.isNull()) {
      projection.error = "snapshot value type is unavailable";
      return projection;
    }
    projection.type = type.getCanonicalType().getUnqualifiedType();
    projection.payload_type = projection.type;
    projection.cpp_type = cpp_type;
    projection.payload_cpp_type = cpp_type;
    Entry schema_entry{};
    schema_entry.cpp_type = cpp_type;
    prepare_schema(schema_entry, projection.payload_type, false, "snapshot value");
    projection.schema = std::move(schema_entry.schema);
    projection.error = std::move(schema_entry.error);
    return projection;
  };
  const auto add_member_policy = [&](const clang::CXXRecordDecl *static_record,
                                     const auto &member_policy, bool automatic,
                                     std::vector<FollowedMember> &out) {
    std::vector<snapshot_members::Step> steps;
    if (!snapshot_members::resolve(static_record, member_policy.field(), steps))
      return;
    const auto *field = steps.back().field;
    // The snapshot is stored beside the field, inside the record that owns it.
    const auto *container = llvm::cast<clang::CXXRecordDecl>(field->getParent());
    const auto output_name = field->getNameAsString() + "_snapshot";
    // The decoded record carries the fields of every base, whatever its access.
    std::vector<const clang::FieldDecl *> container_fields;
    const auto gather = [&](auto &&self, const clang::CXXRecordDecl *record) -> void {
      record = record ? record->getDefinition() : nullptr;
      if (!record)
        return;
      for (const auto &base : record->bases())
        self(self, base.getType()->getAsCXXRecordDecl());
      for (const auto *decl : record->fields())
        container_fields.push_back(decl);
    };
    gather(gather, container);
    if (std::any_of(container_fields.begin(), container_fields.end(), [&](const auto *decl) {
          return decl->getNameAsString() == output_name;
        }))
      return;
    FollowedMember followed;
    followed.field = field;
    followed.name = member_policy.field();
    followed.automatic = automatic;
    const auto kind = snapshot_members::classify(field->getType());
    followed.kind = kind;
    if (kind == snapshot_members::Referent::text) {
      const auto type = text_type();
      auto projection = make_leaf_projection(
          type, type.isNull() ? "" : type.getCanonicalType().getAsString(snapshot_policy));
      if (projection.error.empty())
        followed.projections.push_back(std::move(projection));
    } else if (kind == snapshot_members::Referent::scalar) {
      const auto type = snapshot_members::referent(field->getType())
                            .getUnqualifiedType().getCanonicalType();
      auto projection = make_leaf_projection(type, type.getAsString(snapshot_policy));
      if (projection.error.empty())
        followed.projections.push_back(std::move(projection));
    } else if (kind == snapshot_members::Referent::record) {
      auto base_type = snapshot_members::referent(field->getType())
                           .getUnqualifiedType().getCanonicalType();
      const auto *base_record = base_type->getAsCXXRecordDecl();
      base_record = base_record ? base_record->getDefinition() : nullptr;
      if (!base_record)
        return;
      followed.polymorphic = base_record->isPolymorphic();
      if (automatic && !followed.polymorphic && !snapshot_members::checked_safe(base_type))
        return;
      for (const auto index : member_policy.dynamic_types()) {
        const auto *layout = record_metadata(index);
        if (!layout)
          continue;
        auto projection = make_projection(registry_type(index), *layout, base_type,
                                          followed.polymorphic);
        if (projection.error.empty())
          followed.projections.push_back(std::move(projection));
      }
      if (!followed.polymorphic && followed.projections.empty()) {
        // Non-polymorphic policies contain the static pointee in the registry.
        const auto index = member_policy.dynamic_types_size()
                               ? member_policy.dynamic_types(0)
                               : std::numeric_limits<unsigned>::max();
        if (const auto *layout = record_metadata(index)) {
          auto projection = make_projection(registry_type(index), *layout, base_type, false);
          if (projection.error.empty())
            followed.projections.push_back(std::move(projection));
        }
      }
    }
    // A custom encoder runs user code, which an object that was only checked
    // readable must not meet: automatic following needs RTTI evidence.
    if (automatic && !followed.polymorphic &&
        std::any_of(followed.projections.begin(), followed.projections.end(),
                    [](const auto &projection) { return projection.custom_encoder; }))
      return;
    if (!followed.projections.empty())
      out.push_back(std::move(followed));
  };
  // The members a record's prepared policy selects. An explicit policy
  // replaces the automatic members.
  const auto members_of = [&](const clang::CXXRecordDecl *record,
                              std::vector<FollowedMember> &out) {
    const auto *policy = find_policy(record);
    if (!policy)
      return;
    const auto *definition = record->getDefinition();
    for (const auto &member_policy : policy->members())
      add_member_policy(definition, member_policy, false, out);
    if (policy->members().empty())
      for (const auto &member_policy : policy->automatic_members())
        add_member_policy(definition, member_policy, true, out);
  };
  const auto configure_record_policy = [&](Entry &entry) {
    const auto *static_record = entry.type->getAsCXXRecordDecl();
    static_record = static_record ? static_record->getDefinition() : nullptr;
    const auto *policy = find_policy(static_record);
    if (!policy)
      return;
    if (entry.result_pointee) {
      for (const auto index : policy->dynamic_types()) {
        if (const auto *layout = record_metadata(index)) {
          auto projection = make_projection(registry_type(index), *layout,
                                            entry.type, true);
          if (projection.error.empty())
            entry.dynamic_projections.push_back(std::move(projection));
        }
      }
    }
    members_of(static_record, entry.members);
  };
  const auto configure_legacy_result = [&](Entry &entry) {
    if (spec.prepared().has_snapshot_return_type()) {
      const auto type = prepared_alias(ast, "SnapshotReturn");
      auto projection = make_projection(type, spec.prepared().snapshot_return_type(),
                                        entry.type, true);
      if (projection.error.empty())
        entry.dynamic_projections.push_back(std::move(projection));
    }
    const auto member_name = spec.prepared().snapshot_return_member();
    if (member_name.empty())
      return;
    const auto *root = entry.type->getAsCXXRecordDecl();
    root = root ? root->getDefinition() : nullptr;
    if (!root)
      return;
    const clang::FieldDecl *field = nullptr;
    for (const auto *decl : root->fields())
      if (decl->getNameAsString() == member_name) {
        field = decl;
        break;
      }
    if (!field || !field->getType()->isPointerType() ||
        field->getType().isVolatileQualified() ||
        field->getType()->getPointeeType().isVolatileQualified())
      return;
    FollowedMember followed;
    followed.field = field;
    followed.name = member_name;
    const auto base_type = field->getType()->getPointeeType()
                               .getUnqualifiedType().getCanonicalType();
    const auto *base = base_type->getAsCXXRecordDecl();
    base = base ? base->getDefinition() : nullptr;
    followed.polymorphic = base && base->isPolymorphic();
    const auto count = spec.prepared().snapshot_return_types_size();
    for (int i = 0; i < count; ++i) {
      const auto type = prepared_alias(ast, "SnapshotReturn" + std::to_string(i));
      auto projection = make_projection(type,
          spec.prepared().snapshot_return_types(i), base_type,
          followed.polymorphic);
      if (projection.error.empty())
        followed.projections.push_back(std::move(projection));
    }
    if (count == 0 && spec.prepared().has_snapshot_return_type()) {
      auto projection = make_projection(prepared_alias(ast, "SnapshotReturn"),
          spec.prepared().snapshot_return_type(), base_type,
          followed.polymorphic);
      if (projection.error.empty())
        followed.projections.push_back(std::move(projection));
    }
    if (!followed.projections.empty())
      entry.members.push_back(std::move(followed));
  };
  bool custom_encoder_matched = false;
  for (int i = 0; i < info.arguments_size() &&
                  unsigned(i) < function->getNumParams();
       ++i) {
    const auto &argument = info.arguments(i);
    if (argument.is_reference() || argument.is_object_value())
      add(unsigned(i), Role::argument, function->getParamType(unsigned(i)));
  }
  if (info.result_is_reference() || info.result_is_object_value()) {
    add(0, Role::result, function->getReturnType());
  } else if (spec.snapshot_return_pointee() && function->getReturnType()->isPointerType()) {
    add(0, Role::result, function->getReturnType()->getPointeeType(), true);
  }
  if (access == context_engine::CodecAccess::snapshot_members &&
      spec.signature().kind() == clangtracer::v1::METHOD) {
    const auto receiver = tracer_alias(ast, "ResolvedTarget");
    // A method can destroy its receiver. Both events retain the owned
    // entry snapshot instead of accessing the object after the call.
    if (!receiver.isNull())
      add(0, Role::receiver, receiver);
  }
  for (auto &entry : entries) {
    if (entry.error.empty()) {
      configure_record_policy(entry);
      if (entry.result_pointee) {
        if (entry.dynamic_projections.empty() &&
            spec.prepared().has_snapshot_return_type())
          configure_legacy_result(entry);
      } else if (entry.role == Role::result &&
                 !spec.prepared().snapshot_return_member().empty()) {
        configure_legacy_result(entry);
      }
      for (const auto &member : entry.members)
        for (const auto &projection : member.projections)
          custom_encoder_matched |= projection.custom_encoder;
      for (const auto &projection : entry.dynamic_projections)
        custom_encoder_matched |= projection.custom_encoder;
    }
  }
  // Followed members form a tree: the members of a record, then the members
  // of each pointee layout they may capture, to a fixed depth.
  struct Node {
    int parent = -1;
    std::size_t via = 0; // The parent's projection this node continues from.
    unsigned depth = 1;
    std::size_t entry = 0;
    FollowedMember member;
    const clang::CXXRecordDecl *container = nullptr;
    std::size_t accessor_id = 0;
    std::size_t slot = 0;
    std::size_t children = 0;
    std::string cut_note; // Set when a limit left this node's own members out.
  };
  std::vector<Node> nodes;
  const auto add_node = [&](int parent, std::size_t via, unsigned depth, std::size_t entry,
                            FollowedMember member, const clang::CXXRecordDecl *container) {
    Node node;
    node.parent = parent;
    node.via = via;
    node.depth = depth;
    node.entry = entry;
    node.member = std::move(member);
    node.container = container;
    nodes.push_back(std::move(node));
  };
  const auto limits = tracer::resolve_limits(
      spec.prepared().has_snapshot_limits() ? &spec.prepared().snapshot_limits() : nullptr,
      spec.has_snapshot_limits() ? &spec.snapshot_limits() : nullptr);
  const unsigned follow_depth = limits.follow_depth;
  const std::size_t max_nodes = limits.follow_members;
  for (std::size_t e = 0; follow_members && max_nodes && follow_depth && e < entries.size(); ++e) {
    if (!entries[e].error.empty())
      continue;
    const auto *container = entries[e].type->getAsCXXRecordDecl();
    container = container ? container->getDefinition() : nullptr;
    for (const auto &member : entries[e].members)
      add_node(-1, 0, 1, e, member, container);
  }
  // Members the policy names come first, then the automatic ones; the total
  // for one probe is capped (about 160 bytes of wrapper stack each).
  std::stable_partition(nodes.begin(), nodes.end(),
                        [](const auto &node) { return !node.member.automatic; });
  // Members dropped here are counted per probe argument and reported on its root.
  std::map<std::size_t, std::size_t> dropped_at_entry;
  for (std::size_t n = max_nodes; n < nodes.size(); ++n)
    ++dropped_at_entry[nodes[n].entry];
  if (nodes.size() > max_nodes)
    nodes.resize(max_nodes);
  for (std::size_t n = 0; n < nodes.size(); ++n) {
    for (std::size_t j = 0; j < nodes[n].member.projections.size(); ++j) {
      const auto projection = nodes[n].member.projections[j];
      const auto *record = projection.type.isNull() ? nullptr : projection.type->getAsCXXRecordDecl();
      record = record ? record->getDefinition() : nullptr;
      if (!record || projection.custom_encoder || !projection.error.empty())
        continue;
      std::vector<FollowedMember> deeper;
      members_of(record, deeper);
      if (deeper.empty())
        continue;
      if (nodes[n].depth >= follow_depth) {
        nodes[n].cut_note = "its pointer members were not followed: raise --follow-depth";
        continue;
      }
      for (auto &member : deeper) {
        if (nodes.size() >= max_nodes) {
          nodes[n].cut_note = "some of its pointer members were not followed: raise --follow-members";
          break;
        }
        add_node(static_cast<int>(n), j, nodes[n].depth + 1, nodes[n].entry,
                 std::move(member), record);
      }
    }
  }
  {
    std::map<std::pair<int, std::size_t>, std::size_t> siblings;
    for (auto &node : nodes) {
      node.slot = siblings[{node.parent, node.parent < 0 ? node.entry : 0}]++;
      if (node.parent >= 0)
        ++nodes[static_cast<std::size_t>(node.parent)].children;
      for (const auto &projection : node.member.projections)
        custom_encoder_matched |= projection.custom_encoder;
    }
  }
  if (snapshot_encoders.requested && code.error.empty() &&
      !custom_encoder_matched)
    code.error = "tracer_snapshot::encode input does not match a selected snapshot type";
  if (entries.empty())
    return code;
  std::string scope = R"(
extern "C" void tracer_snapshot_configure(unsigned, unsigned, unsigned, unsigned) noexcept;
extern "C" std::size_t tracer_snapshot_mark() noexcept;
extern "C" void tracer_snapshot_reset(std::size_t) noexcept;
extern "C" std::byte* tracer_snapshot_storage(std::size_t*) noexcept;
extern "C" void tracer_snapshot_commit(std::size_t,std::size_t) noexcept;
extern "C" bool tracer_snapshot_readable(const void*, std::size_t, std::size_t) noexcept;
extern "C" const char* tracer_snapshot_read_cstring(const char*, std::size_t, std::size_t*) noexcept;
extern "C" const char* tracer_snapshot_copy_bytes(const void*, std::size_t) noexcept;
extern "C" bool tracer_snapshot_is_scratch(const void*, std::size_t) noexcept;
extern "C" std::byte* tracer_snapshot_storage_limited(std::size_t*, std::size_t) noexcept;
namespace tracer_snapshot_detail {
struct Scope { std::size_t mark = (tracer_snapshot_configure(@ARENA@u, @SLICE@u, @MEMBERS@u, @DEPTH@u), tracer_snapshot_mark()); bool active=true;
  void release() noexcept { if(active){tracer_snapshot_reset(mark); active=false;} }
  ~Scope(){release();} };
}
)";
  scope.replace(scope.find("@ARENA@"), 7, std::to_string(limits.arena_bytes));
  scope.replace(scope.find("@SLICE@"), 7, std::to_string(limits.slice_bytes));
  scope.replace(scope.find("@MEMBERS@"), 9, std::to_string(limits.follow_members));
  // The root counts as a level of the event's snapshot nesting.
  scope.replace(scope.find("@DEPTH@"), 7, std::to_string(limits.follow_depth + 1));
  code.arguments = "tracer_snapshot_detail::Scope snapshot_scope;\n";
  std::vector<clang::QualType> types;
  std::vector<context_engine::SnapshotMemberAccessor> member_accessors;
  for (auto &entry : entries)
    if (entry.error.empty()) {
      types.push_back(entry.type);
      for (const auto &projection : entry.dynamic_projections)
        if (projection.error.empty())
          types.push_back(projection.payload_type);
    }
  for (auto &node : nodes) {
    node.accessor_id = member_accessors.size();
    member_accessors.push_back({node.container, node.member.name});
    for (const auto &projection : node.member.projections)
      if (projection.error.empty())
        types.push_back(projection.payload_type);
  }
  if (!types.empty()) {
    const auto codecs = member_accessors.empty()
        ? context_engine::render_context_codec_sources(ast, {}, types, access)
        : context_engine::render_snapshot_member_accessors(
              ast, types, member_accessors, access);
    if (!codecs) {
      for (auto &entry : entries)
        if (entry.error.empty())
          entry.error = codecs.error;
      if (snapshot_encoders.requested && code.error.empty())
        code.error = "tracer_snapshot::encode codec generation failed: " +
                     codecs.error;
      types.clear();
      nodes.clear();
    } else {
      try {
        const auto root = context_header_directory();
        if (root.find_first_of("\"\\\r\n") != std::string::npos)
          throw std::runtime_error(
              "unsupported snapshot support header directory");
        code.declarations = "\n#include \"" + root +
                            "/context_codec.hpp\"\n#include \"" + root +
                            "/bounded_proto.cpp\"\n" + codecs.source;
      } catch (const std::exception &error) {
        for (auto &entry : entries)
          if (entry.error.empty())
            entry.error = error.what();
        if (snapshot_encoders.requested && code.error.empty())
          code.error = "tracer_snapshot::encode support generation failed: " +
                       std::string(error.what());
        types.clear();
        nodes.clear();
      }
    }
  }
  code.declarations = scope + code.declarations;
  const bool has_dynamic_capture =
      std::any_of(entries.begin(), entries.end(), [](const auto &entry) {
        return entry.error.empty() && !entry.dynamic_projections.empty();
      }) ||
      std::any_of(nodes.begin(), nodes.end(), [](const auto &node) {
        return node.member.polymorphic && !node.member.automatic &&
               !node.member.projections.empty();
      });
  code.has_followed_members = !nodes.empty();
  // Test-only switch: makes the code that follows members fail to compile so the
  // fallback that drops them can be exercised.
  if (code.has_followed_members && std::getenv("TRACER_TEST_BREAK_FOLLOWED_MEMBERS"))
    code.declarations += "\nstatic_assert(false, \"TRACER_TEST_BREAK_FOLLOWED_MEMBERS\");\n";
  const bool has_automatic_capture =
      std::any_of(nodes.begin(), nodes.end(), [](const auto &node) {
        return node.member.polymorphic && !node.member.projections.empty();
      });
  const bool has_custom_encoder =
      std::any_of(entries.begin(), entries.end(), [](const auto &entry) {
        return entry.error.empty() &&
               std::any_of(entry.dynamic_projections.begin(),
                           entry.dynamic_projections.end(), [](const auto &p) {
                             return p.error.empty() && p.custom_encoder;
                           });
      }) ||
      std::any_of(nodes.begin(), nodes.end(), [](const auto &node) {
        return std::any_of(node.member.projections.begin(),
                           node.member.projections.end(), [](const auto &p) {
                             return p.error.empty() && p.custom_encoder;
                           });
      });
  if (!types.empty())
    code.declarations += R"(
namespace tracer_snapshot_detail {
template<class T> void encode(tracer::ValueSnapshot& snapshot, const T& value) noexcept {
  const auto mark=tracer_snapshot_mark();
  std::size_t capacity = 0;
  std::byte* storage = tracer_snapshot_storage(&capacity);
  tracer::context_engine::CaptureLimits limits;
  limits.bytes=capacity; limits.string_bytes=4096;
  tracer::context_engine::BoundedProtoWriter writer({storage,capacity},limits);
  tracer::context_engine::EncodeBudget budget; budget.max_items=4096;
  const bool ok=tracer::context_engine::encode_value(writer,1,value,budget);
  auto bytes=writer.finish();
  if(ok && writer.status()==tracer::context_engine::CaptureStatus::ok){
    snapshot.status=tracer::ValueSnapshot::Complete; snapshot.payload=bytes.data();
    snapshot.payload_size=bytes.size(); tracer_snapshot_commit(mark,bytes.size());
  }else{tracer_snapshot_reset(mark); snapshot.status=tracer::ValueSnapshot::LimitExceeded; snapshot.detail="snapshot exceeds capture budget";}
}
// A followed member comes from a pointer the target may have left stale: its
// out-of-line storage is read through checked primitives, and it may only use
// the thread's storage down to `reserve`, which stays for the roots still to come.
template<class T> void encode_member(tracer::ValueSnapshot& snapshot, const T& value, std::size_t reserve,
                                      std::size_t string_bytes, std::size_t max_items, std::size_t max_checks) noexcept {
  const auto mark=tracer_snapshot_mark();
  std::size_t capacity = 0;
  std::byte* storage = tracer_snapshot_storage_limited(&capacity, reserve);
  if(capacity == 0){ tracer_snapshot_reset(mark); snapshot.status=tracer::ValueSnapshot::LimitExceeded; snapshot.detail="no snapshot storage was left for a followed member: raise --snapshot-arena or lower --snapshot-reserve"; return; }
  tracer::context_engine::CaptureLimits limits;
  limits.bytes=capacity; limits.string_bytes=string_bytes;
  limits.items=4*max_items+1024; limits.visits=4*max_items+1024;
  tracer::context_engine::BoundedProtoWriter writer({storage,capacity},limits);
  tracer::context_engine::EncodeBudget budget; budget.max_items=max_items; budget.item_cap=max_items;
  budget.string_cap=string_bytes;
  budget.limits.items=limits.items; budget.limits.visits=limits.visits;
  budget.copy=&tracer_snapshot_copy_bytes; budget.readable=&tracer_snapshot_readable; budget.in_scratch=&tracer_snapshot_is_scratch; budget.max_checks=max_checks;
  const bool ok=tracer::context_engine::encode_value(writer,1,value,budget);
  auto bytes=writer.finish();
  if(ok && writer.status()==tracer::context_engine::CaptureStatus::ok){
    snapshot.status=tracer::ValueSnapshot::Complete; snapshot.payload=bytes.data();
    snapshot.payload_size=bytes.size(); tracer_snapshot_commit(mark,bytes.size());
    if(budget.truncated) snapshot.detail=budget.cut;
  }else{
    tracer_snapshot_reset(mark); snapshot.status=tracer::ValueSnapshot::LimitExceeded;
    snapshot.detail = budget.cut ? budget.cut
        : writer.status()==tracer::context_engine::CaptureStatus::bytes_exceeded ? "a followed member does not fit the snapshot payload: raise --snapshot-slice or --snapshot-arena"
        : writer.status()==tracer::context_engine::CaptureStatus::string_exceeded ? "a string in a followed member is longer than the limit: raise --string-bytes"
        : (writer.status()==tracer::context_engine::CaptureStatus::items_exceeded || writer.status()==tracer::context_engine::CaptureStatus::visits_exceeded || writer.status()==tracer::context_engine::CaptureStatus::invalid_cap) ? "a followed member holds more elements than the limits allow: raise --vector-elements"
        : "a followed member could not be captured within the limits";
  }
}

}
)";
  if (has_custom_encoder)
    code.declarations += R"(
#include <cerrno>
#include <type_traits>
extern "C" bool tracer_callback_lifecycle_enter(unsigned long long) noexcept;
extern "C" void tracer_callback_lifecycle_leave() noexcept;
namespace tracer_snapshot_detail {
struct SnapshotEncoderScope {
  int saved_errno=errno;
  bool active=false;
  explicit SnapshotEncoderScope(unsigned long long probe) noexcept
      : active(tracer_callback_lifecycle_enter(probe)) {}
  SnapshotEncoderScope(const SnapshotEncoderScope&)=delete;
  ~SnapshotEncoderScope() noexcept {
    if(active) tracer_callback_lifecycle_leave();
    errno=saved_errno;
  }
  explicit operator bool() const noexcept { return active; }
};
}
)";
  if (has_dynamic_capture)
    code.declarations += R"(
#include <cstring>
#include <cstdint>
#include <typeinfo>
namespace tracer_snapshot_detail {
template<class T> bool has_itanium_rtti(const T* object) noexcept {
  const void* vtable=nullptr;
  std::memcpy(&vtable, static_cast<const void*>(object), sizeof(vtable));
  if(!vtable) return false;
  const auto address=reinterpret_cast<std::uintptr_t>(vtable);
  if(address < sizeof(void*)) return false;
  const std::type_info* rtti=nullptr;
  std::memcpy(&rtti, reinterpret_cast<const void*>(address-sizeof(void*)), sizeof(rtti));
  return rtti != nullptr;
}
}
)";
  if (has_automatic_capture)
    code.declarations += R"(
#include <cstddef>
extern "C" bool tracer_snapshot_read_rtti(const void*, const void**, const char**) noexcept;
extern "C" bool tracer_snapshot_rtti_matches(const char*, const char*, std::size_t, const void*, const void*, std::size_t, std::size_t) noexcept;
)";
  const auto entry_id = [](const Entry &entry) {
    return entry.role == Role::receiver ? std::string("receiver")
           : entry.role == Role::result ? std::string("result")
                                        : "arg" + std::to_string(entry.index);
  };
  const auto entry_root = [](const Entry &entry) {
    return entry.result_pointee
        ? std::string("*result_pointee")
        : entry.role == Role::receiver
              ? std::string("*reinterpret_cast<const tracer::ResolvedTarget*>(ctx.object_address)")
              : entry.role == Role::result
                    ? std::string("tracer::Call(ctx).result()")
                    : "tracer::Call(ctx).argument<" + std::to_string(entry.index) + ">()";
  };
  // Set while a followed member is emitted: its payload is then encoded with
  // the checked primitives and leaves this much storage for the roots.
  long member_reserve = -1;
  const auto emit_payload = [&](std::string &body, const std::string &id,
                                const context_engine::SchemaResult &schema,
                                const std::string &snapshot,
                                const std::string &value_expr,
                                const std::string &suffix,
                                const std::string &cpp_type) {
    std::vector<std::string> record_types;
    const auto bytes = schema_bytes(*schema.record, record_types);
    body += snapshot + ".cpp_type=" + literal(cpp_type) + ";\n";
    body += snapshot + ".root_type=" + literal(schema.record->root_type) +
            "; " + snapshot + ".descriptor=" + literal(bytes) +
            "; " + snapshot + ".descriptor_size=" +
            std::to_string(bytes.size()) + ";\n";
    if (!schema.record->container_roles.empty()) {
      body += "static const char* const roles_" + id + suffix + "[]={";
      for (const auto &role : schema.record->container_roles)
        body += literal(role) + ",";
      body += "}; " + snapshot + ".container_roles=roles_" + id + suffix +
              "; " + snapshot + ".container_role_count=" +
              std::to_string(schema.record->container_roles.size()) + ";\n";
    }
    if (!record_types.empty()) {
      body += "static const char* const types_" + id + suffix + "[]={";
      for (const auto &type : record_types)
        body += literal(type) + ",";
      body += "}; " + snapshot + ".record_types=types_" + id + suffix +
              "; " + snapshot + ".record_type_count=" +
              std::to_string(record_types.size()) + ";\n";
    }
    if (member_reserve >= 0)
      body += "tracer_snapshot_detail::encode_member(" + snapshot + "," + value_expr + "," +
              std::to_string(member_reserve) + "," + std::to_string(limits.string_bytes) + "," +
              std::to_string(limits.vector_elements) + "," + std::to_string(limits.checked_reads) +
              ");\n";
    else
      body += "tracer_snapshot_detail::encode(" + snapshot + "," + value_expr + ");\n";
  };
  const auto emit_projection = [&](std::string &body, const std::string &id,
                                   const Entry &entry,
                                   const SnapshotProjection &projection,
                                   const std::string &snapshot,
                                   const std::string &value_expr,
                                   const std::string &suffix,
                                   bool fallback_root) {
    if (!projection.error.empty()) {
      body += snapshot + ".status=tracer::ValueSnapshot::Unsupported; " +
              snapshot + ".detail=" + literal(projection.error) + ";\n";
      return;
    }
    if (projection.custom_encoder) {
      const auto output = "snapshot_projection_" + id + suffix;
      body += "if(tracer_snapshot_detail::SnapshotEncoderScope encoder_scope{tracer::Call(ctx).probe_id()}){\n"
              "static_assert(std::is_nothrow_destructible_v<" +
              projection.payload_cpp_type +
              ">, \"Snapshot encoder output must be noexcept destructible\");\n"
              "auto " + output + "=::tracer_snapshot::encode(" +
              value_expr + ");\n";
      emit_payload(body, id, projection.schema, snapshot, output, suffix,
                   projection.cpp_type);
      if (fallback_root) {
        body += "}else{\n";
        emit_payload(body, id, entry.schema, snapshot, entry_root(entry),
                     suffix + "_fallback", entry.cpp_type);
        body += "}\n";
      } else {
        body += "}\n";
      }
    } else {
      emit_payload(body, id, projection.schema, snapshot, value_expr, suffix,
                   projection.cpp_type);
    }
  };

  // Level-one members of each entry; deeper members hang off captured ones.
  std::map<std::size_t, std::size_t> top_members;
  for (const auto &node : nodes)
    if (node.parent < 0)
      ++top_members[node.entry];

  // The variables later phases and the dispatch after the call still read.
  const auto declare_node = [&](std::string &body, std::size_t index) {
    const auto &node = nodes[index];
    const auto nid = entry_id(entries[node.entry]) + "_n" + std::to_string(index);
    body += "const void* nptr_" + nid + "=nullptr; int nsel_" + nid + "=-1;\n";
    if (node.children)
      body += "tracer::ValueSnapshot nsnaps_" + nid + "[" + std::to_string(node.children) + "]{};\n"
              "tracer::ValueSnapshotMember nmembers_" + nid + "[" + std::to_string(node.children) + "]{};\n"
              "std::size_t ncount_" + nid + "=0;\n";
  };
  // One followed member: read the pointer or reference, skip links that lead
  // back to an object already captured above it, then capture the layout the
  // pointee turns out to have. Nothing is dereferenced before it is checked.
  const auto emit_node = [&](std::string &body, std::size_t index, bool declare = true) {
    const auto &node = nodes[index];
    // Members captured at entry leave half of the thread's storage for the
    // results and nested calls that are encoded later.
    member_reserve = entries[node.entry].role == Role::result ? 0 : limits.member_reserve;
    const auto &entry = entries[node.entry];
    const auto &member = node.member;
    const auto id = entry_id(entry);
    const auto nid = id + "_n" + std::to_string(index);
    std::string snapshots, members_array, count, parent_value, condition = "true";
    std::vector<std::string> avoided;
    const auto root = entry_root(entry);
    if (node.parent < 0) {
      snapshots = "child_snapshots_" + id;
      members_array = "child_members_" + id;
      count = "child_member_count_" + id;
      parent_value = root;
    } else {
      const auto parent = static_cast<std::size_t>(node.parent);
      const auto pid = id + "_n" + std::to_string(parent);
      snapshots = "nsnaps_" + pid;
      members_array = "nmembers_" + pid;
      count = "ncount_" + pid;
      const auto &chosen = nodes[parent].member.projections[node.via];
      parent_value = "(*static_cast<const " + chosen.cpp_type + "*>(nptr_" + pid + "))";
      condition = "nsel_" + pid + "==" + std::to_string(node.via) + " && nptr_" + pid;
    }
    for (auto ancestor = node.parent; ancestor >= 0;
         ancestor = nodes[static_cast<std::size_t>(ancestor)].parent)
      avoided.push_back("nptr_" + id + "_n" + std::to_string(ancestor));
    avoided.push_back("static_cast<const void*>(__builtin_addressof(" + root + "))");
    const auto child = snapshots + "[" + std::to_string(node.slot) + "]";
    const auto pointer = "pointer_" + nid;
    const auto helper = "tracer::context_engine::snapshot_access::snapshot_member_" +
                        std::to_string(node.accessor_id);
    if (declare)
      declare_node(body, index);
    std::string distinct;
    for (const auto &address : avoided)
      distinct += " && static_cast<const void*>(" + pointer + ")!=" + address;
    body += "if(" + condition + "){\nauto " + pointer + "=" + helper + "(" +
            parent_value + ");\nif(" + pointer + distinct + "){\n";
    const auto suffix_of = [&](std::size_t i) {
      return "_member_" + std::to_string(node.accessor_id) + "_" + std::to_string(index) +
             "_" + std::to_string(i);
    };
    const auto attach = [&](const std::string &selected, std::size_t i) {
      const auto name = literal(snapshot_members::wire_path(member.name));
      const auto note = node.cut_note.empty()
          ? std::string() : child + ".detail=" + literal(node.cut_note) + ";\n";
      body += "if(" + child + ".status==tracer::ValueSnapshot::Complete){\n" + note +
              members_array + "[" + count + "]={" + name + ",&" + child +
              "}; ++" + count + "; nptr_" + nid + "=" + selected + "; nsel_" + nid +
              "=" + std::to_string(i) + ";\n}else if(" + child + ".detail){\n" +
              members_array + "[" + count + "]={" + name + ",&" + child + "}; ++" + count +
              ";\n}\n";
    };
    if (member.kind == snapshot_members::Referent::text) {
      // The text is staged in thread-local scratch, not on the wrapper's stack.
      // 4000 bytes keep the tag and length within the writer's 4096-byte slice.
      const auto &projection = member.projections.front();
      const auto cap = std::to_string(limits.string_bytes);
      body += "std::size_t text_size_" + nid + "=0;\n"
              "if(const char* text_" + nid + "=tracer_snapshot_read_cstring(reinterpret_cast<const char*>(" +
              pointer + ")," + cap + "u+1,&text_size_" + nid + ")){\n"
              "const bool cut_" + nid + "=text_size_" + nid + ">" + cap + "u; if(cut_" + nid + ") text_size_" +
              nid + "=" + cap + "u;\n" + projection.cpp_type + " view_" + nid +
              "(text_" + nid + ",text_size_" + nid + ");\n";
      emit_projection(body, id, entry, projection, child, "view_" + nid, suffix_of(0), false);
      body += "if(cut_" + nid + ") " + child + ".detail=" +
                          literal("a C string was shortened: raise --string-bytes (and --snapshot-slice, which also bounds it)") + ";\n";
      attach("nullptr", 0);
      body += "}\n";
    } else if (member.kind == snapshot_members::Referent::scalar) {
      const auto &projection = member.projections.front();
      body += "if(tracer_snapshot_readable(static_cast<const void*>(" + pointer +
              "),sizeof(*" + pointer + "),alignof(decltype(*" + pointer + ")))){\n";
      emit_projection(body, id, entry, projection, child, "*" + pointer, suffix_of(0), false);
      attach("nullptr", 0);
      body += "}\n";
    } else if (!member.polymorphic) {
      // Without RTTI the static layout cannot be confirmed, so only the
      // readability of the whole object is checked.
      const auto &projection = member.projections.front();
      body += "if(tracer_snapshot_readable(static_cast<const void*>(" + pointer +
              "),sizeof(" + projection.cpp_type + "),alignof(" + projection.cpp_type + "))){\n";
      emit_projection(body, id, entry, projection, child, "*" + pointer, suffix_of(0), false);
      attach("static_cast<const void*>(" + pointer + ")", 0);
      body += "}\n";
    } else {
      // A polymorphic pointee is captured only when its RTTI matches.
      const auto complete = "automatic_complete_" + nid;
      const auto type_name = "automatic_type_name_" + nid;
      body += "const void* " + complete + "=nullptr; const char* " + type_name +
              "=nullptr;\nif(tracer_snapshot_read_rtti(static_cast<const void*>(" + pointer +
              "),&" + complete + ",&" + type_name + ")){\n";
      bool first = true;
      for (std::size_t i = 0; i < member.projections.size(); ++i) {
        const auto &projection = member.projections[i];
        if (projection.rtti_name.empty())
          continue;
        body += first ? "if(" : "else if(";
        body += "tracer_snapshot_rtti_matches(" + type_name + "," +
                literal(projection.rtti_name) + "," +
                std::to_string(projection.rtti_name.size()) + "," + complete +
                ",static_cast<const void*>(" + pointer + "),sizeof(" +
                projection.cpp_type + "),alignof(" + projection.cpp_type + "))){\n";
        emit_projection(body, id, entry, projection, child,
                        "*static_cast<const " + projection.cpp_type + "*>(" + complete + ")",
                        suffix_of(i), false);
        attach(complete, i);
        body += "}\n";
        first = false;
      }
      body += "}\n";
    }
    body += "}\n}\n";
    member_reserve = -1;
  };
  // Point each parent snapshot at the children that were attached to it.
  const auto link_members = [&](std::string &body, const std::vector<std::size_t> &selected_nodes,
                                const Entry *entry_only) {
    for (const auto n : selected_nodes) {
      const auto &node = nodes[n];
      if (!node.children)
        continue;
      const auto id = entry_id(entries[node.entry]);
      const auto nid = id + "_n" + std::to_string(n);
      const auto slot = (node.parent < 0 ? "child_snapshots_" + id
                                          : "nsnaps_" + id + "_n" + std::to_string(node.parent)) +
                        "[" + std::to_string(node.slot) + "]";
      body += slot + ".members=nmembers_" + nid + "; " + slot + ".member_count=ncount_" + nid + ";\n";
    }
    (void)entry_only;
  };
  const auto link_roots = [&](std::string &body, std::size_t entry_index) {
    if (!top_members.count(entry_index))
      return;
    const auto id = entry_id(entries[entry_index]);
    body += "snapshot_" + id + ".members=child_members_" + id + "; snapshot_" + id +
            ".member_count=child_member_count_" + id + ";\n";
  };

  std::string deferred_arguments, deferred_result;
  for (std::size_t entry_index = 0; entry_index < entries.size(); ++entry_index) {
    const auto &entry = entries[entry_index];
    const bool receiver = entry.role == Role::receiver;
    const bool result = entry.role == Role::result;
    const auto id = entry_id(entry);
    const auto slot = receiver ? "ctx.receiver_snapshot"
                               : result ? "ctx.result.snapshot"
                                        : "ctx.arguments[" + std::to_string(entry.index) + "].snapshot";
    auto &body = result ? code.result : code.arguments;
    body += "tracer::ValueSnapshot snapshot_" + id + ";\n";
    if (top_members.count(entry_index))
      body += "tracer::ValueSnapshot child_snapshots_" + id + "[" +
              std::to_string(top_members[entry_index]) + "]{};\n"
              "tracer::ValueSnapshotMember child_members_" + id + "[" +
              std::to_string(top_members[entry_index]) + "]{};\n"
              "std::size_t child_member_count_" + id + "=0;\n";
    if (entry.result_pointee) {
      // These are read again after the block closes, when the call returns.
      for (std::size_t n = 0; n < nodes.size(); ++n)
        if (nodes[n].entry == entry_index)
          declare_node(body, n);
      body += "if (auto *result_pointee=tracer::Call(ctx).result()) {\n";
    }
    body += slot + "=&snapshot_" + id + ";\n";
    if (!entry.error.empty()) {
      body += "snapshot_" + id +
              ".cpp_type=" + literal(entry.cpp_type) +
              "; snapshot_" + id +
              ".status=tracer::ValueSnapshot::Unsupported; snapshot_" + id +
              ".detail=" + literal(entry.error) + ";\n";
      if (entry.result_pointee)
        body += "}\n";
      continue;
    }
    const auto expr = entry_root(entry);
    const auto payload = [&](const context_engine::SchemaResult &schema,
                             const std::string &snapshot, const std::string &value_expr,
                             const std::string &suffix, const std::string &cpp_type) {
      emit_payload(body, id, schema, snapshot, value_expr, suffix, cpp_type);
    };
    const auto projection_of = [&](const SnapshotProjection &projection,
                                   const std::string &snapshot, const std::string &value_expr,
                                   const std::string &suffix, bool fallback_root) {
      emit_projection(body, id, entry, projection, snapshot, value_expr, suffix, fallback_root);
    };

    // Keep null pointer results address-only. Non-null unknown dynamic types
    // retain the prior static-pointee snapshot behavior.
    if (entry.result_pointee) {
      if (entry.dynamic_projections.empty()) {
        payload(entry.schema, "snapshot_" + id, expr, "", entry.cpp_type);
      } else {
        const bool runtime_dispatch = std::any_of(
            entry.dynamic_projections.begin(), entry.dynamic_projections.end(),
            [](const auto &p) { return !p.rtti_name.empty(); });
        if (!runtime_dispatch) {
          const auto &projection = entry.dynamic_projections.front();
          projection_of(projection, "snapshot_" + id, expr, "_root_static", false);
        } else {
        bool first = true;
        for (std::size_t i = 0; i < entry.dynamic_projections.size(); ++i) {
          const auto &projection = entry.dynamic_projections[i];
          if (projection.rtti_name.empty())
            continue;
          body += first ? "if(" : "else if(";
          body += "tracer_snapshot_detail::has_itanium_rtti(result_pointee) && std::strcmp(typeid(*result_pointee).name()," +
                  literal(projection.rtti_name) + ")==0){\n";
          const auto actual = "*static_cast<const " + projection.cpp_type +
                              "*>(dynamic_cast<const void*>(result_pointee))";
          projection_of(projection, "snapshot_" + id, actual,
                        "_root_" + std::to_string(i), true);
          body += "}\n";
          first = false;
        }
        if (first) {
          payload(entry.schema, "snapshot_" + id, expr, "", entry.cpp_type);
        } else {
          body += "else{\n";
          payload(entry.schema, "snapshot_" + id, expr, "_root_fallback", entry.cpp_type);
          body += "}\n";
        }
        }
      }
    } else {
      payload(entry.schema, "snapshot_" + id, expr, "", entry.cpp_type);
    }

    if (const auto dropped = dropped_at_entry.find(entry_index); dropped != dropped_at_entry.end())
      body += "if(snapshot_" + id + ".status==tracer::ValueSnapshot::Complete) snapshot_" + id +
              ".detail=" +
              literal(std::to_string(dropped->second) +
                      " pointer members were not followed: raise --follow-members") + ";\n";
    if (entry.result_pointee) {
      // The pointee only exists inside this block, so its members are captured here.
      std::vector<std::size_t> own;
      for (std::size_t n = 0; n < nodes.size(); ++n)
        if (nodes[n].entry == entry_index)
          own.push_back(n);
      for (const auto n : own)
        emit_node(body, n, false);
      link_members(body, own, &entry);
      link_roots(body, entry_index);
      body += "}\n";
    }
  }
  // Every root has its share of the thread's snapshot storage before any
  // followed member is encoded, shallowest members first. A member that no
  // longer fits stays address-only; a root is never given up for one.
  for (const bool result_body : {false, true}) {
    std::vector<std::size_t> selected_nodes;
    for (std::size_t n = 0; n < nodes.size(); ++n) {
      const auto &entry = entries[nodes[n].entry];
      if (!entry.result_pointee && (entry.role == Role::result) == result_body)
        selected_nodes.push_back(n);
    }
    auto &deferred = result_body ? deferred_result : deferred_arguments;
    for (const auto n : selected_nodes)
      emit_node(deferred, n);
    link_members(deferred, selected_nodes, nullptr);
    for (std::size_t e = 0; e < entries.size(); ++e)
      if (!entries[e].result_pointee && (entries[e].role == Role::result) == result_body)
        link_roots(deferred, e);
  }
  code.arguments += deferred_arguments;
  code.result += deferred_result;
  return code;
}
} // namespace tracer

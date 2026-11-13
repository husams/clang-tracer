#include "value_snapshot_codegen.hpp"
#include "context_paths.hpp"
#include "context_schema.hpp"
#include "tracer.pb.h"
#include "prepared_contract.hpp"
#include "prepared_types.hpp"
#include "snapshot_encoder.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/AST/CXXInheritance.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Type.h>
#include <clang/AST/VTableBuilder.h>
#include <clang/Basic/TargetInfo.h>
#include <algorithm>
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
         (target.getTriple().getArch() == llvm::Triple::x86_64 || target.getTriple().isAArch64()) &&
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
                         const clangtracer::v1::ProbeSpec &spec) {
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
    std::string name;
    std::size_t accessor_id = 0;
    std::vector<SnapshotProjection> projections;
    bool polymorphic = false;
    bool automatic = false;
    bool erased_any = false;
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
  const bool report_unavailable = spec.snapshot_report_unavailable();
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
          paths.isAmbiguous(ast.getCanonicalType(tracer::prepared::record_type(ast, base)))) {
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
  const auto configure_record_policy = [&](Entry &entry) {
    const auto *static_record = entry.type->getAsCXXRecordDecl();
    static_record = static_record ? static_record->getDefinition() : nullptr;
    if (!static_record)
      return;
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
      if (entry.result_pointee) {
        for (const auto index : policy.dynamic_types()) {
          if (const auto *layout = record_metadata(index)) {
            auto projection = make_projection(registry_type(index), *layout,
                                              entry.type, true);
            if (projection.error.empty())
              entry.dynamic_projections.push_back(std::move(projection));
          }
        }
      }
      const auto add_member_policy = [&](const auto &member_policy,
                                         bool automatic) {
        if (automatic && entry.role != Role::result)
          return;
        const clang::FieldDecl *field = nullptr;
        for (const auto *decl : static_record->fields())
          if (decl->getNameAsString() == member_policy.field()) {
            field = decl;
            break;
          }
        if (!field || field->getType().isVolatileQualified())
          return;
        const auto output_name = member_policy.field() + "_snapshot";
        const bool name_collision = std::any_of(
            static_record->fields().begin(), static_record->fields().end(),
            [&](const auto *decl) {
              return decl->getNameAsString() == output_name;
            });
        if (name_collision)
          return;
        if (member_policy.erased_any()) {
          const auto *erased = field->getType()->getAsCXXRecordDecl();
          if (automatic || !erased || erased->getQualifiedNameAsString() != "std::any")
            return;
          FollowedMember followed;
          followed.field = field;
          followed.name = member_policy.field();
          followed.erased_any = true;
          for (const auto index : member_policy.dynamic_types())
            if (const auto *layout = record_metadata(index)) {
              const auto payload = registry_type(index);
              if (payload.isNull())
                continue;
              auto projection = make_projection(payload, *layout, payload, false);
              if (projection.error.empty())
                followed.projections.push_back(std::move(projection));
            }
          if (!followed.projections.empty())
            entry.members.push_back(std::move(followed));
          return;
        }
        if (!field->getType()->isPointerType() ||
            field->getType()->getPointeeType().isVolatileQualified())
          return;
        auto base_type = field->getType()->getPointeeType()
                             .getUnqualifiedType().getCanonicalType();
        const auto *base_record = base_type->getAsCXXRecordDecl();
        base_record = base_record ? base_record->getDefinition() : nullptr;
        if (!base_record)
          return;
        if (automatic && !base_record->isPolymorphic())
          return;
        FollowedMember followed;
        followed.field = field;
        followed.name = member_policy.field();
        followed.polymorphic = base_record->isPolymorphic();
        followed.automatic = automatic;
        for (const auto index : member_policy.dynamic_types()) {
          const auto *layout = record_metadata(index);
          if (!layout)
            continue;
          auto projection = make_projection(registry_type(index), *layout,
                                            base_type, followed.polymorphic);
          if (projection.error.empty())
            followed.projections.push_back(std::move(projection));
        }
        if (!followed.polymorphic && followed.projections.empty()) {
          // Non-polymorphic policies contain the static pointee in the registry.
          const auto index = member_policy.dynamic_types_size()
                                 ? member_policy.dynamic_types(0)
                                 : std::numeric_limits<unsigned>::max();
          if (const auto *layout = record_metadata(index)) {
            auto projection = make_projection(registry_type(index), *layout,
                                              base_type, false);
            if (projection.error.empty())
              followed.projections.push_back(std::move(projection));
          }
        }
        if (!followed.projections.empty())
          entry.members.push_back(std::move(followed));
      };
      for (const auto &member_policy : policy.members()) {
        add_member_policy(member_policy, false);
      }
      if (policy.members().empty())
        for (const auto &member_policy : policy.automatic_members())
          add_member_policy(member_policy, true);
      break;
    }
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
  if (snapshot_encoders.requested && code.error.empty() &&
      !custom_encoder_matched)
    code.error = "tracer_snapshot::encode input does not match a selected snapshot type";
  if (entries.empty())
    return code;
  const std::string scope = R"(
extern "C" std::size_t tracer_snapshot_mark() noexcept;
extern "C" void tracer_snapshot_reset(std::size_t) noexcept;
extern "C" std::byte* tracer_snapshot_storage(std::size_t*) noexcept;
extern "C" void tracer_snapshot_commit(std::size_t,std::size_t) noexcept;
namespace tracer_snapshot_detail {
struct Scope { std::size_t mark = tracer_snapshot_mark(); bool active=true;
  void release() noexcept { if(active){tracer_snapshot_reset(mark); active=false;} }
  ~Scope(){release();} };
}
)";
  code.arguments = "tracer_snapshot_detail::Scope snapshot_scope;\n";
  std::vector<clang::QualType> types;
  std::vector<context_engine::SnapshotMemberAccessor> member_accessors;
  for (auto &entry : entries)
    if (entry.error.empty()) {
      types.push_back(entry.type);
      for (const auto &projection : entry.dynamic_projections)
        if (projection.error.empty())
          types.push_back(projection.payload_type);
      for (auto &member : entry.members) {
        member.accessor_id = member_accessors.size();
        member_accessors.push_back({entry.type->getAsCXXRecordDecl(),
                                    member.name});
        for (const auto &projection : member.projections)
          if (projection.error.empty())
            types.push_back(projection.payload_type);
      }
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
      }
    }
  }
  code.declarations = scope + code.declarations;
  const bool has_dynamic_capture = std::any_of(
      entries.begin(), entries.end(), [](const auto &entry) {
        return entry.error.empty() &&
               (!entry.dynamic_projections.empty() ||
                std::any_of(entry.members.begin(), entry.members.end(),
                            [](const auto &member) {
                              return member.polymorphic && !member.automatic &&
                                     !member.projections.empty();
                            }));
      });
  const bool has_automatic_capture = std::any_of(
      entries.begin(), entries.end(), [report_unavailable](const auto &entry) {
        return std::any_of(entry.members.begin(), entry.members.end(),
                           [report_unavailable](const auto &member) {
                             return (member.automatic || (report_unavailable && member.polymorphic)) &&
                                    !member.projections.empty();
                           });
      });
  const bool has_custom_encoder = std::any_of(
      entries.begin(), entries.end(), [](const auto &entry) {
        return entry.error.empty() &&
               (std::any_of(entry.dynamic_projections.begin(),
                            entry.dynamic_projections.end(), [](const auto &p) {
                              return p.error.empty() && p.custom_encoder;
                            }) ||
                std::any_of(entry.members.begin(), entry.members.end(),
                            [](const auto &m) {
                              return std::any_of(m.projections.begin(),
                                  m.projections.end(), [](const auto &p) {
                                    return p.error.empty() && p.custom_encoder;
                                  });
                            }));
      });
  if (!types.empty()) {
    const auto bound = [](std::uint32_t value, unsigned fallback) {
      return std::to_string(value ? std::min(value, fallback) : fallback);
    };
    code.declarations += "\nnamespace tracer_snapshot_detail {\n"
        "constexpr std::size_t snapshot_items=" + bound(spec.snapshot_max_items(), 4096) + ";\n"
        "constexpr std::size_t snapshot_strings=" + bound(spec.snapshot_max_string_bytes(), 4096) + ";\n"
        "constexpr std::size_t snapshot_depth=" + bound(spec.snapshot_max_depth(), 32) + ";\n"
        "constexpr std::size_t snapshot_bytes=" + bound(spec.snapshot_max_bytes(), 4096) + ";\n"
        "}\n";
    code.declarations += R"(
namespace tracer_snapshot_detail {
template<class T> void encode(tracer::ValueSnapshot& snapshot, const T& value) noexcept {
  const auto mark=tracer_snapshot_mark();
  std::size_t capacity = 0;
  std::byte* storage = tracer_snapshot_storage(&capacity);
  capacity=std::min(capacity,snapshot_bytes);
  tracer::context_engine::CaptureLimits limits;
  limits.bytes=capacity; limits.string_bytes=snapshot_strings;
  limits.depth=snapshot_depth; limits.items=snapshot_items;
  tracer::context_engine::BoundedProtoWriter writer({storage,capacity},limits);
  tracer::context_engine::EncodeBudget budget;
)";
    if (report_unavailable)
      code.declarations += "  tracer::context_engine::EncodeBudget::PointerGraph pointer_graph(snapshot_items);\n"
                           "  budget.pointer_graph=&pointer_graph;\n";
    code.declarations += R"(
  budget.max_items=snapshot_items; budget.limits=limits;
  const bool ok=tracer::context_engine::encode_value(writer,1,value,budget);
  auto bytes=writer.finish();
  if(ok && writer.status()==tracer::context_engine::CaptureStatus::ok){
    snapshot.status=tracer::ValueSnapshot::Complete; snapshot.payload=bytes.data();
    snapshot.payload_size=bytes.size(); tracer_snapshot_commit(mark,bytes.size());
  }else{tracer_snapshot_reset(mark); snapshot.status=tracer::ValueSnapshot::LimitExceeded; snapshot.detail="snapshot exceeds capture budget";}
}

}
)";
  }
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
  if (report_unavailable) {
    code.declarations += R"(
#include <cstddef>
#include <cstdint>
extern "C" bool tracer_snapshot_read_rtti_provider(const void*, const void**, const char**, const void**, const void**) noexcept;
extern "C" bool tracer_snapshot_readable(const void*, std::size_t, std::size_t) noexcept;
namespace tracer_snapshot_detail {
inline bool verified_rtti_storage(const void* address, std::size_t size) noexcept {
  const auto value=reinterpret_cast<std::uintptr_t>(address);
)";
    for (const auto &range : spec.snapshot_rtti_ranges())
      if (range.begin() && range.end() > range.begin())
        code.declarations += "  if(value>=" + std::to_string(range.begin()) +
            "ULL && value<" + std::to_string(range.end()) + "ULL && size<=" +
            std::to_string(range.end()) + "ULL-value) return true;\n";
    code.declarations += R"(  return false;
}
inline bool verified_vtable(const void* address) noexcept {
  const auto value=reinterpret_cast<std::uintptr_t>(address);
  return value>=2*sizeof(void*) && verified_rtti_storage(
      reinterpret_cast<const void*>(value-2*sizeof(void*)),3*sizeof(void*));
}
}
)";
  }
  for (const auto &entry : entries) {
    const bool receiver = entry.role == Role::receiver;
    const bool result = entry.role == Role::result;
    const auto id = receiver ? "receiver"
                             : result ? "result" : "arg" + std::to_string(entry.index);
    const auto slot = receiver ? "ctx.receiver_snapshot"
                               : result ? "ctx.result.snapshot"
                                        : "ctx.arguments[" + std::to_string(entry.index) + "].snapshot";
    auto &body = receiver ? code.receiver : result ? code.result : code.arguments;
    body += "tracer::ValueSnapshot snapshot_" + id + ";\n";
    if (!entry.members.empty())
      body += "tracer::ValueSnapshot child_snapshots_" + id + "[" +
              std::to_string(entry.members.size()) + "]{};\n"
              "tracer::ValueSnapshotMember child_members_" + id + "[" +
              std::to_string(entry.members.size()) + "]{};\n"
              "std::size_t child_member_count_" + id + "=0;\n";
    if (report_unavailable && !entry.members.empty() && entry.error.empty())
      body += "tracer::context_engine::EncodeBudget::PointerGraph child_graph_" + id +
              "(tracer_snapshot_detail::snapshot_items);\n";
    if (entry.result_pointee)
      body += "if (auto *result_pointee=tracer::Call(ctx).result()) {\n";
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
    const auto expr = receiver ? "*reinterpret_cast<const tracer::ResolvedTarget*>(ctx.object_address)"
                               : entry.result_pointee ? "*result_pointee"
                               : result ? "tracer::Call(ctx).result()"
                                        : "tracer::Call(ctx).argument<" +
                                              std::to_string(entry.index) + ">()";
    const auto emit_payload = [&](const context_engine::SchemaResult &schema,
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
      body += "tracer_snapshot_detail::encode(" + snapshot + "," +
              value_expr + ");\n";
    };
    const auto emit_projection = [&](const SnapshotProjection &projection,
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
        emit_payload(projection.schema, snapshot, output, suffix,
                     projection.cpp_type);
        if (fallback_root) {
          body += "}else{\n";
          emit_payload(entry.schema, snapshot, expr, suffix + "_fallback",
                       entry.cpp_type);
          body += "}\n";
        } else {
          body += "}\n";
        }
      } else {
        emit_payload(projection.schema, snapshot, value_expr, suffix,
                     projection.cpp_type);
      }
    };

    // Keep null pointer results address-only. Non-null unknown dynamic types
    // retain the prior static-pointee snapshot behavior.
    if (entry.result_pointee) {
      if (entry.dynamic_projections.empty()) {
        emit_payload(entry.schema, "snapshot_" + id, expr, "", entry.cpp_type);
      } else {
        const bool runtime_dispatch = std::any_of(
            entry.dynamic_projections.begin(), entry.dynamic_projections.end(),
            [](const auto &p) { return !p.rtti_name.empty(); });
        if (!runtime_dispatch) {
          const auto &projection = entry.dynamic_projections.front();
          emit_projection(projection, "snapshot_" + id, expr,
                          "_root_static", false);
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
          emit_projection(projection, "snapshot_" + id, actual,
                          "_root_" + std::to_string(i), true);
          body += "}\n";
          first = false;
        }
        if (first) {
          emit_payload(entry.schema, "snapshot_" + id, expr, "", entry.cpp_type);
        } else {
          body += "else{\n";
          emit_payload(entry.schema, "snapshot_" + id, expr, "_root_fallback",
                       entry.cpp_type);
          body += "}\n";
        }
        }
      }
    } else {
      emit_payload(entry.schema, "snapshot_" + id, expr, "", entry.cpp_type);
    }

    const std::string root_expr = entry.result_pointee
        ? "*result_pointee"
        : receiver
              ? "*reinterpret_cast<const tracer::ResolvedTarget*>(ctx.object_address)"
              : result
                    ? "tracer::Call(ctx).result()"
                    : "tracer::Call(ctx).argument<" +
                          std::to_string(entry.index) + ">()";
    for (std::size_t m = 0; m < entry.members.size(); ++m) {
      const auto &member = entry.members[m];
      const auto pointer = "followed_pointer_" + id + "_" + std::to_string(m);
      const auto child = "child_snapshots_" + id + "[" + std::to_string(m) + "]";
      const auto helper = "tracer::context_engine::snapshot_access::snapshot_member_" +
                          std::to_string(member.accessor_id);
      const auto attach_child = [&]() {
        if (!report_unavailable)
          body += "if(" + child + ".status==tracer::ValueSnapshot::Complete){\n";
        body += "child_members_" + id + "[child_member_count_" + id +
                "]={" + literal(member.name + "_snapshot") + ",&" + child +
                "}; ++child_member_count_" + id + ";\n";
        if (!report_unavailable)
          body += "}\n";
      };
      if (member.erased_any) {
        const auto erased = "erased_member_" + id + "_" + std::to_string(m);
        body += "const auto& " + erased + "=" + helper + "(" + root_expr + ");\n";
        for (std::size_t i = 0; i < member.projections.size(); ++i) {
          const auto &projection = member.projections[i];
          body += (i ? "else if" : "if") + std::string("(const auto* ") + pointer +
                  "=std::any_cast<" + projection.cpp_type + ">(&" + erased + ")){\n";
          if (report_unavailable)
            body += "if(tracer_snapshot_readable(" + pointer + ",sizeof(" + projection.cpp_type +
                    "),alignof(" + projection.cpp_type + "))){\n";
          emit_projection(projection, child, "*" + pointer,
                          "_member_" + std::to_string(member.accessor_id) +
                              "_" + std::to_string(i), false);
          if (report_unavailable)
            body += "}else{ " + child + ".detail=\"registered std::any payload storage unavailable\"; }\n";
          body += "}\n";
        }
        if (report_unavailable)
          body += "if(!" + child + ".detail && " + child +
                  ".status!=tracer::ValueSnapshot::Complete) " + child +
                  ".detail=\"unregistered std::any payload\";\n";
        attach_child();
        continue;
      }
      body += "auto " + pointer + "=" + helper + "(" + root_expr + ");\n"
              "if(" + pointer + "){\n";
      if (member.automatic || (report_unavailable && member.polymorphic)) {
        const auto complete = "automatic_complete_" + id + "_" +
                              std::to_string(m);
        const auto type_name = "automatic_type_name_" + id + "_" +
                               std::to_string(m);
        body += "const void* " + complete + "=nullptr; const char* " + type_name + "=nullptr;\n";
        if (report_unavailable) {
          const auto provider = "rtti_provider_" + id + "_" + std::to_string(m);
          const auto table = "rtti_table_" + id + "_" + std::to_string(m);
          body += "const void* " + provider + "=nullptr; const void* " + table + "=nullptr;\n"
                  "if(tracer_snapshot_read_rtti_provider(static_cast<const void*>(" + pointer +
                  "),&" + complete + ",&" + type_name + ",&" + provider + ",&" + table +
                  ") && tracer_snapshot_detail::verified_rtti_storage(" + provider +
                  ",2*sizeof(void*)) && tracer_snapshot_detail::verified_vtable(" + table +
                  ") && tracer_snapshot_detail::verified_rtti_storage(" + type_name +
                  ",1)){\n";
        } else {
          body += "if(tracer_snapshot_read_rtti(static_cast<const void*>(" + pointer +
                  "),&" + complete + ",&" + type_name + ")){\n";
        }
        bool first = true;
        for (std::size_t i = 0; i < member.projections.size(); ++i) {
          const auto &projection = member.projections[i];
          if (projection.rtti_name.empty())
            continue;
          body += first ? "if(" : "else if(";
          if (report_unavailable)
            body += "tracer_snapshot_detail::verified_rtti_storage(" + type_name + "," +
                    std::to_string(projection.rtti_name.size() + 2) + ") && ";
          body += "tracer_snapshot_rtti_matches(" + type_name + "," +
                  literal(projection.rtti_name) + "," +
                  std::to_string(projection.rtti_name.size()) + "," + complete +
                  ",static_cast<const void*>(" + pointer + "),sizeof(" +
                  projection.cpp_type + "),alignof(" + projection.cpp_type +"))){\n";
          const auto value = "*static_cast<const " + projection.cpp_type +
                             "*>(" + complete + ")";
          if (report_unavailable)
            body += "if(child_graph_" + id + ".admit(reinterpret_cast<std::uintptr_t>(" +
                    complete + "))){\n";
          emit_projection(projection, child, value,
                          "_member_" + std::to_string(member.accessor_id) +
                              "_" + std::to_string(i), false);
          if (report_unavailable)
            body += "}else{ " + child + ".detail=\"pointee alias or capture graph budget exhausted\"; }\n";
          body += "}\n";
          first = false;
        }
        body += "}\n";
      } else {
        bool first = true;
        for (std::size_t i = 0; i < member.projections.size(); ++i) {
          const auto &projection = member.projections[i];
          body += first ? "if(" : "else if(";
          if (member.polymorphic && !projection.rtti_name.empty())
            body += "tracer_snapshot_detail::has_itanium_rtti(" + pointer + ") && std::strcmp(typeid(*" +
                    pointer + ").name()," + literal(projection.rtti_name) + ")==0){\n";
          else if (report_unavailable)
            body += "tracer_snapshot_readable(" + pointer + ",sizeof(" + projection.cpp_type +
                    "),alignof(" + projection.cpp_type + "))){\n";
          else
            body += "true){\n";
          const auto value = member.polymorphic && !projection.rtti_name.empty()
              ? "*static_cast<const " + projection.cpp_type +
                    "*>(dynamic_cast<const void*>(" + pointer + "))"
              : "*" + pointer;
          if (report_unavailable)
            body += "if(child_graph_" + id + ".admit(reinterpret_cast<std::uintptr_t>(" +
                    pointer + "))){\n";
          emit_projection(projection, child, value,
                          "_member_" + std::to_string(member.accessor_id) +
                              "_" + std::to_string(i), false);
          if (report_unavailable)
            body += "}else{ " + child + ".detail=\"pointee alias or capture graph budget exhausted\"; }\n";
          body += "}\n";
          first = false;
        }
      }
      body += "}\n";
      if (report_unavailable)
        body += "if(!" + child + ".detail && " + child +
                ".status!=tracer::ValueSnapshot::Complete) " + child +
                ".detail=\"pointee type, provider or readable storage unavailable\";\n";
      attach_child();
    }
    if (!entry.members.empty())
      body += "snapshot_" + id + ".members=child_members_" + id +
              "; snapshot_" + id + ".member_count=child_member_count_" + id +
              ";\n";
    if (entry.result_pointee)
      body += "}\n";
  }
  return code;
}
} // namespace tracer

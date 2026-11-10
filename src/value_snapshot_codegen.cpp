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
    auto decoded_type = value.snapshot_cpp_type.empty()
                            ? value.cpp_type
                            : value.snapshot_cpp_type;
    names[message_name(value.cpp_type)] = std::move(decoded_type);
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
  struct PImplProjection {
    clang::QualType type;
    clang::QualType payload_type;
    std::string cpp_type;
    std::string payload_cpp_type;
    std::string rtti_name;
    context_engine::SchemaResult schema;
    std::string error;
    bool custom_encoder = false;
  };
  struct Entry {
    unsigned index;
    Role role;
    bool result_pointee;
    clang::QualType type;
    std::string cpp_type;
    context_engine::SchemaResult schema;
    std::string error;
    clang::QualType dynamic_type;
    std::string dynamic_cpp_type;
    context_engine::SchemaResult dynamic_schema;
    std::string dynamic_error;
    std::string dynamic_rtti_name;
    bool dynamic_capture = false;
    std::string pimpl_member;
    std::string pimpl_error;
    bool pimpl_capture = false;
    std::vector<PImplProjection> pimpl_projections;
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
  const auto configure_dynamic_result = [&](Entry &entry,
                                             clang::QualType type) {
    const auto fail = [&](const std::string &reason) {
      entry.error = reason;
    };
    type = type.getNonReferenceType().getUnqualifiedType().getCanonicalType();
    const auto *candidate = type->getAsCXXRecordDecl();
    if (!candidate || !candidate->getDefinition() || candidate->isUnion() ||
        candidate->isAbstract()) {
      fail("snapshot return projection is not a complete concrete record");
      return;
    }
    candidate = candidate->getDefinition();
    const auto *base = function->getReturnType()->getPointeeType()->getAsCXXRecordDecl();
    base = base ? base->getDefinition() : nullptr;
    if (!base || !base->isPolymorphic()) {
      fail("snapshot return projection requires a polymorphic record pointee");
      return;
    }
    if (candidate->getCanonicalDecl() == base->getCanonicalDecl()) {
      fail("snapshot return projection must name a derived record type");
      return;
    }
    clang::CXXBasePaths paths;
    if (!candidate->isDerivedFrom(base, paths) ||
        paths.isAmbiguous(ast.getCanonicalType(ast.getRecordType(base)))) {
      fail("snapshot return projection is not an unambiguous base conversion");
      return;
    }
    const bool has_public_path = std::any_of(
        paths.begin(), paths.end(), [](const auto &path) {
          return path.Access == clang::AS_public;
        });
    if (!has_public_path) {
      fail("snapshot return projection does not have a public base path");
      return;
    }

    clangtracer::v1::DebugInfo actual_layout;
    tracer::prepared::describe_record(ast, *candidate, actual_layout);
    if (actual_layout.SerializeAsString() !=
        spec.prepared().snapshot_return_type().SerializeAsString()) {
      fail("snapshot return projection layout does not match prepared metadata");
      return;
    }
    const auto rtti_name = itanium_rtti_type_name(ast, type);
    if (rtti_name.empty()) {
      fail("cannot mangle snapshot return projection RTTI type");
      return;
    }

    clang::PrintingPolicy policy(ast.getLangOpts());
    policy.SuppressTagKeyword = true;
    entry.dynamic_type = type;
    entry.dynamic_cpp_type = type.getAsString(policy);
    entry.dynamic_rtti_name = rtti_name;
    entry.dynamic_capture = supports_itanium_dynamic_rtti(ast);
    if (!entry.dynamic_capture)
      return;

    Entry derived{};
    derived.cpp_type = entry.dynamic_cpp_type;
    prepare_schema(derived, type, false, "snapshot projection");
    entry.dynamic_schema = std::move(derived.schema);
    entry.dynamic_error = std::move(derived.error);
  };
  bool pimpl_configured = false;
  const auto configure_pimpl_result = [&](Entry &entry) {
    const auto member_name = spec.prepared().snapshot_return_member();
    const auto fail = [&](const std::string &reason) {
      entry.pimpl_error = reason;
    };
    if (member_name.empty())
      return;
    pimpl_configured = true;
    const auto *root = entry.type->getAsCXXRecordDecl();
    root = root ? root->getDefinition() : nullptr;
    if (!root) {
      fail("prepared PImpl snapshot requires a complete root record type");
      return;
    }
    const clang::FieldDecl *member = nullptr;
    for (const auto *field : root->fields())
      if (field->getNameAsString() == member_name) {
        member = field;
        break;
      }
    if (!member || !member->getType()->isPointerType() ||
        member->getType().isVolatileQualified() ||
        member->getType()->getPointeeType().isVolatileQualified()) {
      fail("prepared PImpl member is not a non-volatile direct raw pointer field");
      return;
    }
    const auto *member_base =
        member->getType()->getPointeeType()->getAsCXXRecordDecl();
    member_base = member_base ? member_base->getDefinition() : nullptr;
    if (!member_base || !member_base->isPolymorphic()) {
      fail("prepared PImpl member must point to a polymorphic record");
      return;
    }
    if (!supports_itanium_dynamic_rtti(ast))
      return;

    clang::PrintingPolicy policy(ast.getLangOpts());
    policy.SuppressTagKeyword = true;
    entry.pimpl_member = member_name;
    entry.pimpl_capture = true;
    const auto add_projection = [&](clang::QualType implementation_type,
                                    const clangtracer::v1::DebugInfo &metadata) {
      PImplProjection projection;
      if (!implementation_type.isNull())
        projection.type = implementation_type.getCanonicalType();
      if (!projection.type.isNull()) {
        projection.cpp_type = implementation_type.getAsString(policy);
        projection.payload_type = implementation_type.getCanonicalType();
        projection.payload_cpp_type = projection.cpp_type;
        for (const auto &encoder : snapshot_encoders.encoders) {
          if (encoder.input.getCanonicalType().getUnqualifiedType() ==
              projection.type.getUnqualifiedType()) {
            projection.custom_encoder = true;
            projection.payload_type = encoder.output.getCanonicalType();
            projection.payload_cpp_type = encoder.output.getAsString(policy);
            break;
          }
        }
      }
      const auto *implementation = implementation_type.isNull()
                                       ? nullptr
                                       : implementation_type->getAsCXXRecordDecl();
      implementation = implementation ? implementation->getDefinition() : nullptr;
      if (!implementation || implementation->isUnion() ||
          implementation->isAbstract()) {
        projection.error = "prepared PImpl type is not a complete concrete record";
        entry.pimpl_projections.push_back(std::move(projection));
        return;
      }
      clang::CXXBasePaths paths;
      if (implementation->getCanonicalDecl() != member_base->getCanonicalDecl() &&
          (!implementation->isDerivedFrom(member_base, paths) ||
           paths.isAmbiguous(ast.getCanonicalType(ast.getRecordType(member_base))))) {
        projection.error = "prepared PImpl type is not a pointee-compatible implementation";
        entry.pimpl_projections.push_back(std::move(projection));
        return;
      }
      clangtracer::v1::DebugInfo actual_layout;
      tracer::prepared::describe_record(ast, *implementation, actual_layout);
      if (actual_layout.SerializeAsString() != metadata.SerializeAsString()) {
        projection.error = "prepared PImpl implementation layout does not match metadata";
        entry.pimpl_projections.push_back(std::move(projection));
        return;
      }
      projection.rtti_name = itanium_rtti_type_name(ast, implementation_type);
      if (projection.rtti_name.empty()) {
        projection.error = "cannot mangle prepared PImpl RTTI type";
        entry.pimpl_projections.push_back(std::move(projection));
        return;
      }
      std::set<const clang::CXXRecordDecl *> seen;
      if (!codec_types_supported(projection.payload_type, seen)) {
        projection.error = "unsupported string character type in PImpl snapshot";
        entry.pimpl_projections.push_back(std::move(projection));
        return;
      }
      context_engine::StateFamilyKey family;
      family.source_lineage_id[0] = 1;
      family.cpp_state_type = entry.type.getAsString(policy);
      family.cpp_scope_type = "tracer::EventValueSnapshot";
      context_engine::SchemaLimits limits;
      limits.descriptor_bytes = 4096;
      limits.fields = 128;
      limits.depth = 16;
      projection.schema = context_engine::build_pimpl_payload_schema(
          ast, entry.type, std::move(family), member_name,
          implementation_type, projection.payload_type, limits, access);
      projection.error = projection.schema.error;
      entry.pimpl_projections.push_back(std::move(projection));
    };
    for (int i = 0; i < spec.prepared().snapshot_return_types_size(); ++i) {
      const auto alias_name = "SnapshotReturn" + std::to_string(i);
      const auto type = prepared_alias(ast, alias_name);
      add_projection(type, spec.prepared().snapshot_return_types(i));
    }
    if (spec.prepared().snapshot_return_types_size() == 0 &&
        spec.prepared().has_snapshot_return_type()) {
      const auto type = prepared_alias(ast, "SnapshotReturn");
      add_projection(type, spec.prepared().snapshot_return_type());
    }
    if (snapshot_encoders.requested) {
      for (const auto &projection : entry.pimpl_projections)
        if (projection.custom_encoder && !projection.error.empty() &&
            code.error.empty()) {
          code.error = "tracer_snapshot::encode projection is unsupported: " +
                       projection.error;
          break;
        }
      for (const auto &encoder : snapshot_encoders.encoders) {
        if (!code.error.empty())
          break;
        const bool matched = std::any_of(
            entry.pimpl_projections.begin(), entry.pimpl_projections.end(),
            [&](const auto &projection) {
              return !projection.type.isNull() &&
                     projection.type.getCanonicalType().getUnqualifiedType() ==
                         encoder.input.getCanonicalType().getUnqualifiedType();
            });
        if (!matched) {
          code.error = "tracer_snapshot::encode input does not match a prepared PImpl return type";
          break;
        }
      }
    }
  };
  for (int i = 0; i < info.arguments_size() &&
                  unsigned(i) < function->getNumParams();
       ++i) {
    const auto &argument = info.arguments(i);
    if (argument.is_reference() || argument.is_object_value())
      add(unsigned(i), Role::argument, function->getParamType(unsigned(i)));
  }
  if (info.result_is_reference() || info.result_is_object_value()) {
    add(0, Role::result, function->getReturnType());
    if (!entries.empty() && entries.back().role == Role::result &&
        entries.back().error.empty())
      configure_pimpl_result(entries.back());
  } else if (spec.snapshot_return_pointee() && function->getReturnType()->isPointerType()) {
    add(0, Role::result, function->getReturnType()->getPointeeType(), true);
    auto &entry = entries.back();
    if (entry.error.empty() && !spec.prepared().snapshot_return_member().empty())
      configure_pimpl_result(entry);
    else if (entry.error.empty() && spec.prepared().has_snapshot_return_type()) {
      const auto dynamic_type = prepared_alias(ast, "SnapshotReturn");
      if (dynamic_type.isNull())
        entry.error = "prepared snapshot return projection type is unavailable";
      else
        configure_dynamic_result(entry, dynamic_type);
    }
  }
  if (snapshot_encoders.requested && !pimpl_configured && code.error.empty())
    code.error = "tracer_snapshot::encode requires --snapshot-return-member and a result snapshot";
  if (snapshot_encoders.requested && code.error.empty())
    for (const auto &entry : entries)
      if (entry.role == Role::result && !entry.pimpl_error.empty()) {
        code.error = entry.pimpl_error;
        break;
      }
  if (access == context_engine::CodecAccess::snapshot_members &&
      spec.signature().kind() == clangtracer::v1::METHOD) {
    const auto receiver = tracer_alias(ast, "ResolvedTarget");
    // A method can destroy its receiver. Both events retain the owned
    // entry snapshot instead of accessing the object after the call.
    if (!receiver.isNull())
      add(0, Role::receiver, receiver);
  }
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
  const Entry *pimpl_entry = nullptr;
  for (const auto &entry : entries)
    if (entry.error.empty()) {
      types.push_back(entry.type);
      if (entry.dynamic_capture && entry.dynamic_error.empty())
        types.push_back(entry.dynamic_type);
      if (entry.pimpl_capture) {
        pimpl_entry = &entry;
        for (const auto &projection : entry.pimpl_projections)
          if (projection.error.empty())
            types.push_back(projection.payload_type);
      }
    }
  if (!types.empty()) {
    const auto codecs = pimpl_entry
        ? context_engine::render_pimpl_context_codec_sources(
              ast, types, pimpl_entry->type, pimpl_entry->pimpl_member, access)
        : context_engine::render_context_codec_sources(ast, {}, types, access);
    if (!codecs) {
      for (auto &entry : entries)
        if (entry.error.empty())
          entry.error = codecs.error;
      if (pimpl_entry && code.error.empty() &&
          std::any_of(pimpl_entry->pimpl_projections.begin(),
                      pimpl_entry->pimpl_projections.end(),
                      [](const auto &projection) {
                        return projection.custom_encoder;
                      }))
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
        if (pimpl_entry && code.error.empty() &&
            std::any_of(pimpl_entry->pimpl_projections.begin(),
                        pimpl_entry->pimpl_projections.end(),
                        [](const auto &projection) {
                          return projection.custom_encoder;
                        }))
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
               (entry.dynamic_capture || entry.pimpl_capture);
      });
  const bool has_custom_pimpl_encoder = std::any_of(
      entries.begin(), entries.end(), [](const auto &entry) {
        return entry.error.empty() &&
               std::any_of(entry.pimpl_projections.begin(),
                           entry.pimpl_projections.end(), [](const auto &projection) {
                             return projection.error.empty() &&
                                    projection.custom_encoder;
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
template<class Root, class Impl> void encode_pimpl(tracer::ValueSnapshot& snapshot,
    const Root& root, const Impl& implementation) noexcept {
  const auto mark=tracer_snapshot_mark();
  std::size_t capacity=0;
  std::byte* storage=tracer_snapshot_storage(&capacity);
  tracer::context_engine::CaptureLimits limits;
  limits.bytes=capacity; limits.string_bytes=4096;
  tracer::context_engine::BoundedProtoWriter writer({storage,capacity},limits);
  tracer::context_engine::EncodeBudget budget; budget.max_items=4096;
  bool ok=writer.begin_message(1);
  std::uint32_t field_number=1;
  if(ok) ok=tracer::context_engine::Codec<Root>::write_fields(
      writer,root,budget,field_number);
  if(ok) ok=tracer::context_engine::encode_value(
      writer,field_number,implementation,budget);
  ok=writer.end_message() && ok;
  auto bytes=writer.finish();
  if(ok && writer.status()==tracer::context_engine::CaptureStatus::ok){
    snapshot.status=tracer::ValueSnapshot::Complete; snapshot.payload=bytes.data();
    snapshot.payload_size=bytes.size(); tracer_snapshot_commit(mark,bytes.size());
  }else{tracer_snapshot_reset(mark); snapshot.status=tracer::ValueSnapshot::LimitExceeded; snapshot.detail="snapshot exceeds capture budget";}
}
}
)";
  if (has_custom_pimpl_encoder)
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
  for (const auto &entry : entries) {
    const bool receiver = entry.role == Role::receiver;
    const bool result = entry.role == Role::result;
    const auto id = receiver ? "receiver"
                             : result ? "result" : "arg" + std::to_string(entry.index);
    const auto slot = receiver ? "ctx.receiver_snapshot"
                               : result ? "ctx.result.snapshot"
                                        : "ctx.arguments[" + std::to_string(entry.index) + "].snapshot";
    auto &body = result ? code.result : code.arguments;
    body += "tracer::ValueSnapshot snapshot_" + id + ";\n";
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
                                  const std::string &cpp_type,
                                  bool pimpl_payload = false,
                                  const std::string &root_expr = std::string{}) {
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
      if (pimpl_payload)
        body += "tracer_snapshot_detail::encode_pimpl(" + snapshot + "," +
                root_expr + "," + value_expr + ");\n";
      else
        body += "tracer_snapshot_detail::encode(" + snapshot + "," +
                value_expr + ");\n";
    };
    if (result && entry.pimpl_capture) {
      const std::string root_expr = entry.result_pointee
                                        ? "*result_pointee"
                                        : "tracer::Call(ctx).result()";
      body += "auto pimpl_pointer=tracer::context_engine::snapshot_access::pimpl_member(" +
              root_expr + ");\n";
      body += "if(pimpl_pointer && tracer_snapshot_detail::has_itanium_rtti(pimpl_pointer)){\n"
              "const char* pimpl_runtime_type=typeid(*pimpl_pointer).name();\n";
      bool first_projection = true;
      for (std::size_t index = 0; index < entry.pimpl_projections.size(); ++index) {
        const auto &projection = entry.pimpl_projections[index];
        if (projection.type.isNull() || projection.rtti_name.empty())
          continue;
        body += first_projection ? "if(" : "else if(";
        body += "std::strcmp(pimpl_runtime_type," +
                literal(projection.rtti_name) + ")==0){\n";
        first_projection = false;
        if (!projection.error.empty()) {
          body += "snapshot_" + id + ".cpp_type=" + literal(entry.cpp_type) +
                  "; snapshot_" + id +
                  ".status=tracer::ValueSnapshot::Unsupported; snapshot_" + id +
                  ".detail=" + literal(projection.error) + ";\n";
        } else {
          const auto complete = "dynamic_cast<const void*>(pimpl_pointer)";
          const auto impl_expr = "*static_cast<const " + projection.cpp_type +
                                 "*>(" + complete + ")";
          if (projection.custom_encoder) {
            const auto output_var = "snapshot_projection_" +
                                    std::to_string(index);
            body += "if(tracer_snapshot_detail::SnapshotEncoderScope encoder_scope{tracer::Call(ctx).probe_id()}){\n"
                    "static_assert(std::is_nothrow_destructible_v<" +
                    projection.payload_cpp_type +
                    ">, \"Snapshot encoder output must be noexcept destructible\");\n"
                    "auto " + output_var + "=::tracer_snapshot::encode(" +
                    impl_expr + ");\n";
            emit_payload(projection.schema, "snapshot_" + id, output_var,
                         "_pimpl_" + std::to_string(index), entry.cpp_type,
                         true, root_expr);
            body += "}else{\n";
            emit_payload(entry.schema, "snapshot_" + id, expr, "_fallback",
                         entry.cpp_type);
            body += "}\n";
          } else {
            emit_payload(projection.schema, "snapshot_" + id, impl_expr,
                         "_pimpl_" + std::to_string(index), entry.cpp_type,
                         true, root_expr);
          }
        }
        body += "}\n";
      }
      if (!first_projection)
        body += "else{\n";
      emit_payload(entry.schema, "snapshot_" + id, expr, "_fallback",
                   entry.cpp_type);
      if (!first_projection)
        body += "}\n";
      body += "}else{\n";
      emit_payload(entry.schema, "snapshot_" + id, expr, "", entry.cpp_type);
      body += "}\n";
    } else if (result && entry.dynamic_capture) {
      body += "if(tracer_snapshot_detail::has_itanium_rtti(result_pointee) && "
              "std::strcmp(typeid(*result_pointee).name()," +
              literal(entry.dynamic_rtti_name) + ")==0){\n";
      const auto complete = "dynamic_cast<const void*>(result_pointee)";
      const auto derived_expr = "*static_cast<const " + entry.dynamic_cpp_type +
                                "*>(" + complete + ")";
      if (entry.dynamic_error.empty()) {
        emit_payload(entry.dynamic_schema, "snapshot_" + id, derived_expr,
                     "_derived", entry.dynamic_cpp_type);
      } else {
        body += "snapshot_" + id + ".cpp_type=" +
                literal(entry.dynamic_cpp_type) + "; snapshot_" + id +
                ".status=tracer::ValueSnapshot::Unsupported; snapshot_" + id +
                ".detail=" + literal(entry.dynamic_error) + ";\n";
      }
      body += "}else{\n";
      emit_payload(entry.schema, "snapshot_" + id, expr, "", entry.cpp_type);
      body += "}\n";
    } else {
      emit_payload(entry.schema, "snapshot_" + id, expr, "", entry.cpp_type);
    }
    if (entry.result_pointee)
      body += "}\n";
  }
  return code;
}
} // namespace tracer

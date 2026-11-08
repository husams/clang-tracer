#include "value_snapshot_codegen.hpp"
#include "context_paths.hpp"
#include "context_schema.hpp"
#include "tracer.pb.h"
#include "prepared_contract.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Type.h>
#include <algorithm>
#include <functional>
#include <google/protobuf/descriptor.pb.h>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

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
clang::QualType tracer_alias(clang::ASTContext &ast, llvm::StringRef name) {
  for (const auto *decl : ast.getTranslationUnitDecl()->decls()) {
    const auto *space = llvm::dyn_cast<clang::NamespaceDecl>(decl);
    if (!space || space->getName() != "tracer")
      continue;
    for (const auto *member : space->decls()) {
      const auto *alias = llvm::dyn_cast<clang::TypeAliasDecl>(member);
      if (alias && alias->getName() == name)
        return alias->getUnderlyingType();
    }
  }
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
  if (!spec.has_debug_info())
    return code;
  const auto signature = tracer_alias(ast, "ResolvedSignature");
  if (signature.isNull())
    return code;
  const auto *function = signature->getAs<clang::FunctionProtoType>();
  if (!function)
    return code;
  enum class Role { argument, result, receiver };
  struct Entry {
    unsigned index;
    Role role;
    clang::QualType type;
    std::string cpp_type;
    context_engine::SchemaResult schema;
    std::string error;
  };
  std::vector<Entry> entries;
  const auto access = tracer::prepared::is_typed_version(spec.prepared().format_version())
      ? context_engine::CodecAccess::snapshot_members
      : context_engine::CodecAccess::public_members;
  const auto add = [&](unsigned index, Role role, clang::QualType type) {
    const bool is_volatile = type.getNonReferenceType().isVolatileQualified();
    type = type.getNonReferenceType().getUnqualifiedType().getCanonicalType();
    if (type->isPointerType() || type->isVoidType())
      return;
    clang::PrintingPolicy policy(ast.getLangOpts());
    policy.SuppressTagKeyword = true;
    Entry entry{index, role, type, type.getAsString(policy), {}, {}};
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
    const auto codec = context_engine::render_payload_codec_source(ast, type, access);
    if (!entry.schema)
      entry.error = entry.schema.error;
    else if (!codec)
      entry.error = codec.error;
    std::set<const clang::CXXRecordDecl *> seen;
    if (!codec_types_supported(type, seen))
      entry.error = "unsupported string character type in snapshot";
    if (is_volatile)
      entry.error = "volatile values cannot be snapshotted automatically";
    if (entry.schema) {
      google::protobuf::FileDescriptorSet descriptors;
      descriptors.ParseFromString(entry.schema.record->descriptor_set);
      for (const auto &file : descriptors.file())
        for (const auto &message : file.message_type())
          for (const auto &field : message.field())
            if (field.options().uninterpreted_option_size())
              entry.error =
                  "snapshot field name cannot be represented by this schema";
    }
    entries.push_back(std::move(entry));
  };
  for (int i = 0; i < spec.debug_info().arguments_size() &&
                  unsigned(i) < function->getNumParams();
       ++i) {
    const auto &argument = spec.debug_info().arguments(i);
    if (argument.is_reference() || argument.is_object_value())
      add(unsigned(i), Role::argument, function->getParamType(unsigned(i)));
  }
  const auto &info = spec.debug_info();
  if (info.result_is_reference() || info.result_is_object_value())
    add(0, Role::result, function->getReturnType());
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
  for (const auto &entry : entries)
    if (entry.error.empty())
      types.push_back(entry.type);
  if (!types.empty()) {
    const auto codecs =
        context_engine::render_context_codec_sources(ast, {}, types, access);
    if (!codecs) {
      for (auto &entry : entries)
        if (entry.error.empty())
          entry.error = codecs.error;
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
        types.clear();
      }
    }
  }
  code.declarations = scope + code.declarations;
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
    body += "tracer::ValueSnapshot snapshot_" + id + "; " + slot +
            "=&snapshot_" + id + ";\n";
    body += "snapshot_" + id + ".cpp_type=" + literal(entry.cpp_type) + ";\n";
    if (!entry.error.empty()) {
      body += "snapshot_" + id +
              ".status=tracer::ValueSnapshot::Unsupported; snapshot_" + id +
              ".detail=" + literal(entry.error) + ";\n";
      continue;
    }
    std::vector<std::string> record_types;
    const auto bytes = schema_bytes(*entry.schema.record, record_types);
    body += "snapshot_" + id +
            ".root_type=" + literal(entry.schema.record->root_type) +
            "; snapshot_" + id + ".descriptor=" + literal(bytes) +
            "; snapshot_" + id +
            ".descriptor_size=" + std::to_string(bytes.size()) + ";\n";
    if (!entry.schema.record->container_roles.empty()) {
      body += "static const char* const roles_" + id + "[]={";
      for (const auto &role : entry.schema.record->container_roles)
        body += literal(role) + ",";
      body += "}; snapshot_" + id + ".container_roles=roles_" + id +
              "; snapshot_" + id + ".container_role_count=" +
              std::to_string(entry.schema.record->container_roles.size()) +
              ";\n";
    }
    if (!record_types.empty()) {
      body += "static const char* const types_" + id + "[]={";
      for (const auto &type : record_types)
        body += literal(type) + ",";
      body += "}; snapshot_" + id + ".record_types=types_" + id +
              "; snapshot_" + id +
              ".record_type_count=" + std::to_string(record_types.size()) +
              ";\n";
    }
    const auto expr = receiver ? "*reinterpret_cast<const tracer::ResolvedTarget*>(ctx.object_address)"
                               : result ? "tracer::Call(ctx).result()"
                                        : "tracer::Call(ctx).argument<" +
                                              std::to_string(entry.index) + ">()";
    body +=
        "tracer_snapshot_detail::encode(snapshot_" + id + "," + expr + ");\n";
  }
  return code;
}
} // namespace tracer

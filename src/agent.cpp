#include "debug_resolver.hpp"
#include "event_transport.hpp"
#include "native_stack.hpp"
#include "runtime_paths.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <clang/AST/ASTImporter.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/Version.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Interpreter/Interpreter.h>
#include <clang/Parse/ParseAST.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <dlfcn.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCDisassembler/MCDisassembler.h>
#include <llvm/MC/MCInst.h>
#include <llvm/MC/MCInstrInfo.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/MC/MCTargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "callback_source.hpp"
#include "tracer.pb.h"
#include "tracer/protocol.hpp"
#include "tracer/receiver_abi.hpp"

#if !defined(__linux__) || !defined(__x86_64__)
#error "clang-tracer's native agent currently supports Linux x86_64 only"
#endif

namespace {
constexpr std::size_t kJumpBytes = 14;
constexpr std::size_t kMaxDecodedBytes = tracer::kMaxPatchBytes - kJumpBytes;
thread_local unsigned callback_depth = 0;

struct DebugRecord {
  clang::CXXRecordDecl *decl;
  clangtracer::v1::ReceiverContext layout;
};
using DebugRecords = std::vector<DebugRecord>;

struct Probe {
  tracer::Request request{};
  std::unique_ptr<clang::Interpreter> interpreter;
  std::unique_ptr<clang::ASTUnit> receiver_ast;
  DebugRecords debug_records;
  void *trampoline = MAP_FAILED;
  std::size_t mapping_size = 0;
  std::atomic<std::uint64_t> sequence{0};
  tracer::EventDelivery delivery;
  std::atomic<std::uint64_t> callback_errors{0};
  std::atomic<std::uint64_t> call_ids{0};
  std::atomic<std::uint64_t> in_flight{0};
  std::atomic<bool> callback_enabled{true};
  std::atomic<bool> active{false};
  std::string session_id;
  long long (*callback)(const tracer::TraceContext *) = nullptr;
  std::vector<std::string> argument_names;
  bool class_callback = false;
  bool has_pre = false;
  bool has_post = false;
  bool transport_active = false;
  void *wrapper = nullptr;

  ~Probe() {
    if (delivery.socket_fd >= 0) {
      ::close(delivery.socket_fd);
    }
    if (trampoline != MAP_FAILED) {
      ::munmap(trampoline, mapping_size);
    }
  }
};

struct Agent {
  std::array<std::unique_ptr<Probe>, tracer::kMaxProbes> probes{};
  std::array<std::atomic<Probe *>, tracer::kMaxProbes> probe_slots{};
  std::size_t retained = 0;
  pid_t pid = 0;
  tracer::EventTransport transport{[] { ++callback_depth; }};

  Agent() {
    for (auto &slot : probe_slots) {
      slot.store(nullptr, std::memory_order_relaxed);
    }
  }
};

class DiagnosticCapture final : public clang::DiagnosticConsumer {
public:
  explicit DiagnosticCapture(clang::DiagnosticConsumer *forward)
      : forward_(forward) {}

  void HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                        const clang::Diagnostic &info) override {
    clang::DiagnosticConsumer::HandleDiagnostic(level, info);
    if (level >= clang::DiagnosticsEngine::Error) {
      had_error_ = true;
    }
    llvm::SmallString<256> formatted;
    info.FormatDiagnostic(formatted);
    if (!formatted.empty()) {
      if (!message_.empty()) {
        message_ += '\n';
      }
      message_.append(formatted.begin(), formatted.end());
    }
    if (forward_) {
      forward_->HandleDiagnostic(level, info);
    }
  }

  const std::string &message() const { return message_; }

  bool had_error() const { return had_error_; }

private:
  clang::DiagnosticConsumer *forward_;
  std::string message_;
  bool had_error_ = false;
};

extern "C" void tracer_probe_enter(unsigned long long) noexcept;
extern "C" void tracer_probe_leave(unsigned long long) noexcept;
extern "C" unsigned long long tracer_next_call_id(unsigned long long) noexcept;
extern "C" void tracer_capture_stack(const tracer::TraceContext *, unsigned, bool) noexcept;
extern "C" void tracer_capture_entry_stack(const tracer::TraceContext *, unsigned) noexcept;
extern "C" long long
tracer_callback_dispatch(unsigned long long,
                         const tracer::TraceContext *) noexcept;
extern "C" void tracer_event_dispatch(unsigned long long,
                                      const tracer::TraceContext *,
                                      long long) noexcept;

std::mutex agent_mutex;
std::atomic<Agent *> agent{nullptr};

Probe *find_probe(std::uint64_t id) noexcept {
  Agent *current = agent.load(std::memory_order_acquire);
  if (!current) {
    return nullptr;
  }
  for (auto &slot : current->probe_slots) {
    Probe *probe = slot.load(std::memory_order_acquire);
    if (probe && probe->request.probe_id == id) {
      return probe;
    }
  }
  return nullptr;
}

std::uint64_t address_value(auto address) {
  if constexpr (requires { address.getValue(); }) {
    return address.getValue();
  } else {
    return static_cast<std::uint64_t>(address);
  }
}

void set_error(tracer::Request *request, const char *text) noexcept {
  if (request) {
    std::snprintf(request->error, sizeof(request->error), "%s", text);
  }
}

constexpr std::size_t kMaxSerializedProbeSpec = 256 * 1024;
constexpr std::size_t kMaxCallbackSource = 64 * 1024;
constexpr std::size_t kMaxReceiverFields = 128;

bool read_probe_spec(const tracer::Request &request,
                     clangtracer::v1::ProbeSpec &spec, std::string &error) {
  std::ifstream input(request.source_path, std::ios::binary);
  if (!input) {
    error = "cannot open serialized probe specification";
    return false;
  }
  std::string bytes(kMaxSerializedProbeSpec + 1, '\0');
  input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  const auto count = static_cast<std::size_t>(input.gcount());
  if (count > kMaxSerializedProbeSpec) {
    error = "serialized probe specification exceeds 256 KiB";
    return false;
  }
  bytes.resize(count);
  if (!spec.ParseFromArray(bytes.data(), static_cast<int>(bytes.size()))) {
    error = "cannot parse serialized probe specification";
    return false;
  }
  if (spec.symbol() != request.symbol || !spec.has_signature() ||
      static_cast<unsigned>(spec.signature().kind()) !=
          static_cast<unsigned>(request.kind) ||
      static_cast<unsigned>(spec.signature().result()) !=
          static_cast<unsigned>(request.result) ||
      spec.signature().arguments_size() !=
          static_cast<int>(request.argument_count) ||
      request.argument_count > tracer::kMaxArguments) {
    error =
        "serialized probe specification does not match the prepared request";
    return false;
  }
  for (unsigned i = 0; i < request.argument_count; ++i) {
    if (static_cast<unsigned>(spec.signature().arguments(static_cast<int>(
            i))) != static_cast<unsigned>(request.arguments[i])) {
      error = "serialized probe signature does not match the prepared request";
      return false;
    }
  }
  if (spec.callback_source().size() > kMaxCallbackSource) {
    error = "callback source exceeds 64 KiB";
    return false;
  }
  if (spec.stack_depth() > tracer::kMaxStackDepth) {
    error = "stack depth must be between 0 and 64";
    return false;
  }
  if (spec.has_receiver()) {
    const auto &receiver = spec.receiver();
    if (receiver.ByteSizeLong() > kMaxCallbackSource) {
      error = "receiver compilation context exceeds 64 KiB";
      return false;
    }
    if (receiver.translation_unit().empty() ||
        receiver.working_directory().empty() ||
        receiver.compiler_arguments().empty() ||
        receiver.record_name().empty() ||
        receiver.fields_size() > static_cast<int>(kMaxReceiverFields)) {
      error =
          "receiver compilation context is incomplete or exceeds its limits";
      return false;
    }
    std::size_t arguments_size = 0;
    for (const auto &argument : receiver.compiler_arguments()) {
      arguments_size += argument.size();
      if (arguments_size > kMaxCallbackSource) {
        error = "receiver compiler arguments exceed 64 KiB";
        return false;
      }
    }
  }
  return true;
}

bool supported_receiver_field(clang::QualType type) {
  type = type.getCanonicalType();
  if (const auto *array =
          llvm::dyn_cast<clang::ConstantArrayType>(type.getTypePtr())) {
    return supported_receiver_field(array->getElementType());
  }
  if (type->isPointerType()) {
    const auto pointee = type->getPointeeType().getCanonicalType();
    return pointee->isVoidType() || pointee->isBuiltinType();
  }
  return type->isBuiltinType() && type->isScalarType();
}

bool matches_receiver_value_type(clang::QualType type,
                                 clang::ASTContext &context,
                                 tracer::ValueType expected, bool result) {
  if (type->isReferenceType()) {
    return false;
  }
  type = type.getCanonicalType().getUnqualifiedType();
  if (type->isVoidType()) {
    return result && expected == tracer::ValueType::Void;
  }
  if (type->isPointerType()) {
    return expected == tracer::ValueType::Pointer &&
           context.getTypeSize(type) == 64;
  }
  if (type->isBooleanType() || type->isCharType() || type->isBitIntType()) {
    return false;
  }
  if (type->isFloatingType()) {
    return (context.getTypeSize(type) == 32 &&
            expected == tracer::ValueType::Float32) ||
           (context.getTypeSize(type) == 64 &&
            expected == tracer::ValueType::Float64);
  }
  if (type->isIntegerType() || type->isEnumeralType()) {
    const auto bits = context.getTypeSize(type);
    const bool is_signed = type->isSignedIntegerOrEnumerationType();
    return (bits == 32 && is_signed && expected == tracer::ValueType::Int32) ||
           (bits == 64 && is_signed && expected == tracer::ValueType::Int64) ||
           (bits == 64 && !is_signed && expected == tracer::ValueType::UInt64);
  }
  return false;
}

const char *field_access_name(clang::AccessSpecifier access, bool is_class) {
  switch (access) {
  case clang::AS_public:
    return "public";
  case clang::AS_protected:
    return "protected";
  case clang::AS_private:
    return "private";
  case clang::AS_none:
    return is_class ? "private" : "public";
  }
  return "";
}

bool validate_receiver_layout(clang::ASTContext &context,
                              clang::CXXRecordDecl *record,
                              const clangtracer::v1::ReceiverContext &receiver,
                              std::string &error) {
  if (!record || !record->isCompleteDefinition() || record->isUnion() ||
      record->isDependentType() || record->getDescribedClassTemplate() ||
      llvm::isa<clang::ClassTemplateSpecializationDecl>(record) ||
      record->isDynamicClass() || record->getNumBases() != 0 ||
      std::distance(record->fields().begin(), record->fields().end()) !=
          static_cast<std::ptrdiff_t>(receiver.fields_size())) {
    error = "receiver class has an unsupported or changed layout";
    return false;
  }
  const auto &layout = context.getASTRecordLayout(record);
  const auto record_size = static_cast<std::uint64_t>(
      context
          .getTypeSizeInChars(context.getTypeDeclType(
              static_cast<const clang::TypeDecl *>(record)))
          .getQuantity());
  const auto record_alignment =
      static_cast<std::uint64_t>(layout.getAlignment().getQuantity());
  if (record_size != receiver.size() ||
      record_alignment != receiver.alignment()) {
    error = "receiver size or alignment changed since source selection";
    return false;
  }
  clang::PrintingPolicy policy(context.getLangOpts());
  unsigned index = 0;
  for (clang::FieldDecl *field : record->fields()) {
    const auto &expected = receiver.fields(static_cast<int>(index));
    const auto type = field->getType();
    const auto bit_width = field->isBitField() ? field->getBitWidthValue() : 0;
    if (!supported_receiver_field(type) ||
        field->getNameAsString() != expected.name() ||
        type.getCanonicalType().getAsString(policy) !=
            expected.canonical_type() ||
        layout.getFieldOffset(index) != expected.bit_offset() ||
        bit_width != expected.bit_width() ||
        context.getTypeSize(type) != expected.bit_size() ||
        context.getTypeAlign(type) != expected.bit_alignment() ||
        tracer::receiver_abi::type_identity(type, context) !=
            expected.abi_identity() ||
        field->isMutable() != expected.is_mutable() ||
        field_access_name(field->getAccess(), record->isClass()) !=
            expected.access()) {
      error = "receiver fields changed since source selection";
      return false;
    }
    ++index;
  }
  return true;
}

std::string mangle_function(const clang::FunctionDecl *declaration,
                            clang::ASTContext &context) {
  auto mangle =
      std::unique_ptr<clang::MangleContext>(context.createMangleContext());
  if (!mangle->shouldMangleDeclName(declaration)) {
    return declaration->getNameAsString();
  }
  std::string symbol;
  llvm::raw_string_ostream output(symbol);
  mangle->mangleName(clang::GlobalDecl(declaration), output);
  output.flush();
  return symbol;
}

bool debug_identifier(llvm::StringRef text) {
  if (text.empty() || text.size() > 63)
    return false;
  for (std::size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
          (i && c >= '0' && c <= '9')))
      return false;
  }
  return true;
}

bool debug_path_identifier(llvm::StringRef text) {
  if (text.size() > 63 || text.empty())
    return false;
  while (true) {
    auto part = text.split('.');
    if (!debug_identifier(part.first))
      return false;
    if (part.second.empty())
      return !text.ends_with(".");
    text = part.second;
  }
}

clang::QualType debug_builtin(clang::ASTContext &context,
                              llvm::StringRef type) {
  if (type.ends_with(" const")) {
    auto t = debug_builtin(context, type.drop_back(6));
    t.addConst();
    return t;
  }
  if (type.ends_with(" volatile")) {
    auto t = debug_builtin(context, type.drop_back(9));
    t.addVolatile();
    return t;
  }
  if (type.ends_with("*"))
    return context.getPointerType(debug_builtin(context, type.drop_back()));
  if (type.starts_with("const ")) {
    auto t = debug_builtin(context, type.drop_front(6));
    t.addConst();
    return t;
  }
  if (type == "void")
    return context.VoidTy;
  if (type == "bool")
    return context.BoolTy;
  if (type == "char")
    return context.CharTy;
  if (type == "signed char")
    return context.SignedCharTy;
  if (type == "unsigned char")
    return context.UnsignedCharTy;
  if (type == "short" || type == "short int")
    return context.ShortTy;
  if (type == "unsigned short" || type == "unsigned short int")
    return context.UnsignedShortTy;
  if (type == "int")
    return context.IntTy;
  if (type == "unsigned int")
    return context.UnsignedIntTy;
  if (type == "long" || type == "long int")
    return context.LongTy;
  if (type == "unsigned long" || type == "unsigned long int" ||
      type == "long unsigned int")
    return context.UnsignedLongTy;
  if (type == "long long" || type == "long long int")
    return context.LongLongTy;
  if (type == "unsigned long long" || type == "unsigned long long int" ||
      type == "long long unsigned int")
    return context.UnsignedLongLongTy;
  if (type == "float")
    return context.FloatTy;
  if (type == "double")
    return context.DoubleTy;
  throw std::runtime_error("unsupported DWARF builtin: " + type.str());
}

DebugRecord construct_debug_record(clang::ASTContext &context,
                                   const clangtracer::v1::DebugInfo &info) {
  class Finder : public clang::RecursiveASTVisitor<Finder> {
  public:
    std::string selected;
    clang::CXXRecordDecl *found = nullptr;
    bool VisitCXXRecordDecl(clang::CXXRecordDecl *d) {
      if (d->isCompleteDefinition() &&
          d->getQualifiedNameAsString() == selected)
        found = d;
      return true;
    }
  } finder;
  finder.selected = info.record_name();
  finder.TraverseDecl(context.getTranslationUnitDecl());
  auto *record = finder.found;
  if (!record) {
    clang::DeclContext *scope = context.getTranslationUnitDecl();
    llvm::StringRef remaining = info.record_name();
    while (remaining.contains("::")) {
      auto part = remaining.split("::");
      if (!debug_identifier(part.first))
        throw std::runtime_error("unsupported debug receiver scope");
      auto &id = context.Idents.get(part.first);
      clang::NamespaceDecl *ns = nullptr;
      for (auto *named : scope->lookup(&id))
        if ((ns = llvm::dyn_cast<clang::NamespaceDecl>(named)))
          break;
      if (!ns) {
        ns = clang::NamespaceDecl::Create(context, scope, false, {}, {}, &id,
                                          nullptr, false);
        scope->addDecl(ns);
      }
      scope = ns;
      remaining = part.second;
    }
    if (!debug_identifier(remaining))
      throw std::runtime_error("unsupported debug receiver name");
    record =
        clang::CXXRecordDecl::Create(context, clang::TagTypeKind::Class, scope,
                                     {}, {}, &context.Idents.get(remaining));
    scope->addDecl(record);
    record->startDefinition();
    for (const auto &field : info.fields()) {
      if (!debug_identifier(field.name()))
        throw std::runtime_error("unsupported debug field name");
      auto type = debug_builtin(context, field.builtin_type());
      auto *decl = clang::FieldDecl::Create(
          context, record, {}, {}, &context.Idents.get(field.name()), type,
          context.getTrivialTypeSourceInfo(type), nullptr, false,
          clang::ICIS_NoInit);
      decl->setAccess(field.access() == "private"     ? clang::AS_private
                      : field.access() == "protected" ? clang::AS_protected
                                                      : clang::AS_public);
      record->addDecl(decl);
    }
    record->completeDefinition();
    record->setArgPassingRestrictions(
        clang::RecordArgPassingKind::CanPassInRegs);
  }
  if (record->isUnion() || record->isDynamicClass() || record->getNumBases() ||
      std::distance(record->field_begin(), record->field_end()) !=
          info.fields_size())
    throw std::runtime_error("unsupported DWARF receiver layout");
  const auto &layout = context.getASTRecordLayout(record);
  if (static_cast<std::uint64_t>(layout.getSize().getQuantity()) !=
      info.record_size())
    throw std::runtime_error("Clang receiver size does not match DWARF");
  clangtracer::v1::ReceiverContext metadata;
  auto *receiver = &metadata;
  receiver->Clear();
  receiver->set_record_name(info.record_name());
  receiver->set_size(info.record_size());
  receiver->set_alignment(layout.getAlignment().getQuantity());
  receiver->set_is_volatile(info.receiver_volatile());
  unsigned i = 0;
  clang::PrintingPolicy policy(context.getLangOpts());
  for (auto *field : record->fields()) {
    const auto &expected = info.fields(i);
    if (field->getNameAsString() != expected.name() || field->isBitField() ||
        !context.hasSameType(field->getType(),
                             debug_builtin(context, expected.builtin_type())) ||
        layout.getFieldOffset(i) != expected.byte_offset() * 8 ||
        context.getTypeSize(field->getType()) != expected.byte_size() * 8 ||
        field_access_name(field->getAccess(), record->isClass()) !=
            expected.access())
      throw std::runtime_error(
          "Clang field layout/access does not match DWARF");
    auto *f = receiver->add_fields();
    f->set_name(expected.name());
    f->set_canonical_type(
        field->getType().getCanonicalType().getAsString(policy));
    f->set_bit_offset(layout.getFieldOffset(i++));
    f->set_bit_size(context.getTypeSize(field->getType()));
    f->set_bit_alignment(context.getTypeAlign(field->getType()));
    f->set_access(expected.access());
    f->set_is_mutable(field->isMutable());
    f->set_abi_identity(
        tracer::receiver_abi::type_identity(field->getType(), context));
  }

  std::string error;
  if (!validate_receiver_layout(context, record, metadata, error))
    throw std::runtime_error(error);
  return {record, std::move(metadata)};
}

std::uint64_t validate_debug_entry(const clangtracer::v1::ProbeSpec &spec) {
  const auto &info = spec.debug_info();
  const auto checked =
      tracer::client::resolve_debug_callable(::getpid(), info.selector());
  if (!info.runtime_entry() || !info.entry_size() ||
      checked.spec.debug_info().SerializeAsString() != info.SerializeAsString() ||
      checked.spec.symbol() != spec.symbol() ||
      checked.spec.signature().SerializeAsString() !=
          spec.signature().SerializeAsString() ||
      checked.spec.argument_names().size() != spec.argument_names().size() ||
      !std::equal(spec.argument_names().begin(), spec.argument_names().end(),
                  checked.spec.argument_names().begin())) {
    throw std::runtime_error(
        "mapped debug image, entry or callable metadata changed before injection");
  }
  return checked.spec.debug_info().runtime_entry();
}

bool build_debug_receiver(clangtracer::v1::ProbeSpec &spec,
                          std::unique_ptr<clang::ASTUnit> &output,
                          clang::CXXRecordDecl *&record, std::string &error,
                          DebugRecords &records) {
  try {
    const auto &info = spec.debug_info();
    // Re-read mapped debug information in the target and compare the complete
    // image identity, signature and type metadata before constructing any AST.
    validate_debug_entry(spec);
    bool has_record = !info.record_name().empty();
    for (const auto &argument : info.arguments())
      has_record |= argument.has_object_type();
    if (!has_record)
      return true;
    std::string headers;
    if (spec.include_headers_size() > 16)
      throw std::runtime_error("at most 16 optional includes");
    for (const auto &header : spec.include_headers()) {
      if (header.empty() || header.size() > 4095 || header.front() != '/' ||
          header.find_first_of("\"\r\n") != std::string::npos ||
          header.find('\0') != std::string::npos)
        throw std::runtime_error("invalid optional include path");
      headers += "#include \"" + header + "\"\n";
    }
    DiagnosticCapture capture(nullptr);
    if (spec.has_header_context()) {
      const auto &header_context = spec.header_context();
      const auto &directory = header_context.working_directory();
      if (spec.include_headers().empty() || directory.empty() ||
          directory.front() != '/' || directory.size() > 4095 ||
          directory.find('\0') != std::string::npos ||
          header_context.ByteSizeLong() > 64 * 1024 ||
          header_context.compiler_arguments_size() > 512)
        throw std::runtime_error("invalid header compilation context");
      auto arguments = tracer::runtime::jit_arguments();
      arguments.insert(arguments.end(),
                       header_context.compiler_arguments().begin(),
                       header_context.compiler_arguments().end());
      bool resource_dir = false;
      for (const auto &argument : arguments) {
        if (argument.find('\0') != std::string::npos)
          throw std::runtime_error("invalid header compiler argument");
        resource_dir |= argument == "-resource-dir" ||
                        argument.starts_with("-resource-dir=");
      }
      if (!resource_dir)
        arguments.push_back(std::string("-resource-dir=") +
                            tracer::runtime::clang_resource_dir());
      arguments.insert(arguments.end(), {"-x", "c++", "-fsyntax-only"});
      clang::tooling::FixedCompilationDatabase database(directory, arguments);
      auto physical_fs = llvm::vfs::createPhysicalFileSystem();
      llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> private_fs(
          physical_fs.release());
      if (auto ec = private_fs->setCurrentWorkingDirectory(directory))
        throw std::runtime_error("cannot use header working directory: " +
                                 ec.message());
      // Map a synthetic TU, never the compilation database's donor .cpp.
      // The private filesystem keeps the target's process-wide cwd unchanged.
      const auto input =
          (std::filesystem::path(directory) / "clang-tracer-debug-types.cc")
              .string();
      clang::tooling::ClangTool tool(
          database, {input}, std::make_shared<clang::PCHContainerOperations>(),
          private_fs);
      tool.mapVirtualFile(input, headers);
      tool.setDiagnosticConsumer(&capture);
      tool.appendArgumentsAdjuster(clang::tooling::getClangStripOutputAdjuster());
      tool.appendArgumentsAdjuster(
          clang::tooling::getClangStripDependencyFileAdjuster());
      std::vector<std::unique_ptr<clang::ASTUnit>> units;
      if (tool.buildASTs(units) != 0 || units.size() != 1)
        throw std::runtime_error("cannot parse optional headers: " +
                                 capture.message());
      output = std::move(units.front());
    } else {
      auto arguments = tracer::runtime::jit_arguments();
      arguments.insert(arguments.end(),
                       {"-std=c++20", "-x", "c++",
                        std::string("-resource-dir=") +
                            tracer::runtime::clang_resource_dir()});
      output = clang::tooling::buildASTFromCodeWithArgs(
          headers, arguments,
          "clang-tracer-debug-types.cc", "clang-tracer",
          std::make_shared<clang::PCHContainerOperations>(),
          clang::tooling::getClangStripOutputAdjuster(), {}, &capture);
    }
    if (!output || capture.had_error())
      throw std::runtime_error("cannot create debug type AST: " +
                               capture.message());
    auto &context = output->getASTContext();
    const auto &triple = context.getTargetInfo().getTriple();
    if (triple.getArch() != llvm::Triple::x86_64 || !triple.isOSLinux() ||
        context.getTypeSize(context.VoidPtrTy) != 64)
      throw std::runtime_error("header target does not match Linux x86_64");
    if (!info.record_name().empty())
      records.push_back(construct_debug_record(context, info));
    for (const auto &argument : info.arguments()) {
      if (!argument.has_object_type())
        continue;
      const auto &object = argument.object_type();
      bool duplicate = false;
      for (const auto &previous : records)
        if (previous.layout.record_name() == object.record_name())
          duplicate = true;
      auto checked_record = construct_debug_record(context, object);
      if (argument.is_object_value() &&
          !checked_record.decl->canPassInRegisters())
        throw std::runtime_error(
            "nontrivial header type disagrees with by-value DWARF ABI");
      if (!duplicate)
        records.push_back(std::move(checked_record));
    }
    record = records.front().decl;
    *spec.mutable_receiver() = records.front().layout;
    return true;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}

bool build_and_validate_receiver_ast(clangtracer::v1::ProbeSpec &spec,
                                     const tracer::Request &request,
                                     std::unique_ptr<clang::ASTUnit> &output,
                                     clang::CXXRecordDecl *&output_record,
                                     std::string &error,
                                     DebugRecords &records) {
  if (spec.has_debug_info())
    return build_debug_receiver(spec, output, output_record, error, records);
  if (!spec.has_receiver()) {
    return true;
  }
  if (request.kind != tracer::CallableKind::Method) {
    error = "receiver metadata is valid only for a non-static method";
    return false;
  }
  const auto &receiver = spec.receiver();
  if (receiver.clang_version() != CLANG_VERSION_STRING) {
    error = "receiver Clang version does not match the tracing agent";
    return false;
  }
  auto arguments = tracer::runtime::jit_arguments();
  arguments.reserve(
      arguments.size() +
      static_cast<std::size_t>(receiver.compiler_arguments_size()));
  const auto source_path =
      std::filesystem::path(receiver.translation_unit()).is_absolute()
          ? std::filesystem::path(receiver.translation_unit())
          : std::filesystem::path(receiver.working_directory()) /
                receiver.translation_unit();
  const auto normalized_source = source_path.lexically_normal();
  bool first = true;
  for (const auto &argument : receiver.compiler_arguments()) {
    if (first) {
      first = false; // The frozen invocation includes argv[0].
      continue;
    }
    const std::filesystem::path candidate(argument);
    const auto normalized_candidate =
        (candidate.is_absolute()
             ? candidate
             : std::filesystem::path(receiver.working_directory()) / candidate)
            .lexically_normal();
    if (!argument.empty() && argument.front() != '-' &&
        normalized_candidate == normalized_source) {
      continue;
    }
    arguments.push_back(argument);
  }
  if (arguments.empty()) {
    error = "receiver compilation context has no compiler options";
    return false;
  }
  clang::tooling::FixedCompilationDatabase database(
      receiver.working_directory(), arguments);
  auto physical_fs = llvm::vfs::createPhysicalFileSystem();
  llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> private_fs(
      physical_fs.release());
  clang::tooling::ClangTool tool(
      database, {receiver.translation_unit()},
      std::make_shared<clang::PCHContainerOperations>(), private_fs);
  DiagnosticCapture diagnostics(nullptr);
  tool.setDiagnosticConsumer(&diagnostics);
  tool.appendArgumentsAdjuster(clang::tooling::getClangStripOutputAdjuster());
  std::vector<std::unique_ptr<clang::ASTUnit>> units;
  if (tool.buildASTs(units) != 0 || diagnostics.had_error() ||
      units.size() != 1) {
    error = diagnostics.message();
    if (error.empty()) {
      error = "cannot parse receiver translation unit with its frozen compiler "
              "context";
    }
    return false;
  }
  auto &context = units.front()->getASTContext();
  const auto triple = context.getTargetInfo().getTriple();
  if (triple.getArch() != llvm::Triple::x86_64 || !triple.isOSLinux() ||
      context.getTypeSize(context.VoidPtrTy) != 64 ||
      triple.str() != receiver.target_triple()) {
    error =
        "receiver target triple does not match the Linux x86_64 tracing agent";
    return false;
  }

  class TargetFinder final : public clang::RecursiveASTVisitor<TargetFinder> {
  public:
    TargetFinder(clang::ASTContext &context, llvm::StringRef symbol,
                 llvm::StringRef record_name)
        : context_(context), symbol_(symbol), record_name_(record_name) {}

    bool VisitFunctionDecl(clang::FunctionDecl *function) {
      auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(function);
      if (!method || llvm::isa<clang::CXXConstructorDecl>(method) ||
          llvm::isa<clang::CXXDestructorDecl>(method) ||
          method->getParent()->getQualifiedNameAsString() != record_name_ ||
          function->getType()->isDependentType()) {
        return true;
      }
      if (mangle_function(function, context_) == symbol_) {
        target = method;
        record = method->getParent()->getDefinition();
      }
      return true;
    }

    clang::CXXRecordDecl *record = nullptr;
    clang::CXXMethodDecl *target = nullptr;

  private:
    clang::ASTContext &context_;
    std::string symbol_;
    std::string record_name_;
  };

  TargetFinder finder(context, request.symbol, receiver.record_name());
  finder.TraverseDecl(context.getTranslationUnitDecl());
  auto *record = finder.record;
  auto *target = finder.target;
  if (!record || !target || target->isStatic() ||
      target->getParent()->getCanonicalDecl() != record->getCanonicalDecl()) {
    error = "frozen source does not contain the selected non-static method and "
            "receiver";
    return false;
  }
  if (!validate_receiver_layout(context, record, receiver, error)) {
    return false;
  }
  auto *method = target;
  if (method->isVolatile() != receiver.is_volatile() ||
      !matches_receiver_value_type(method->getReturnType(), context,
                                   request.result, true) ||
      method->getNumParams() != request.argument_count) {
    error = "selected method signature changed since source selection";
    return false;
  }
  for (unsigned i = 0; i < request.argument_count; ++i) {
    if (!matches_receiver_value_type(method->getParamDecl(i)->getType(),
                                     context, request.arguments[i], false)) {
      error = "selected method arguments do not match the prepared request";
      return false;
    }
  }
  output_record = record;
  output = std::move(units.front());
  return true;
}

bool valid_type(tracer::ValueType type, bool result) {
  switch (type) {
  case tracer::ValueType::Void:
    return result;
  case tracer::ValueType::Int32:
  case tracer::ValueType::Int64:
  case tracer::ValueType::UInt64:
  case tracer::ValueType::Float32:
  case tracer::ValueType::Float64:
  case tracer::ValueType::Pointer:
    return true;
  case tracer::ValueType::Object:
    return !result;
  default:
    return false;
  }
}

const char *type_name(tracer::ValueType type) {
  switch (type) {
  case tracer::ValueType::Void:
    return "void";
  case tracer::ValueType::Int32:
    return "int32_t";
  case tracer::ValueType::Int64:
    return "int64_t";
  case tracer::ValueType::UInt64:
    return "uint64_t";
  case tracer::ValueType::Float32:
    return "float";
  case tracer::ValueType::Float64:
    return "double";
  case tracer::ValueType::Pointer:
    return "void*";
  default:
    return "void";
  }
}

void fill_value(std::string &code, unsigned i, tracer::ValueType type) {
  code += "ctx.arguments[" + std::to_string(i) +
          "].type=static_cast<tracer::ValueType>(" +
          std::to_string(static_cast<unsigned>(type)) + ");\n";
  switch (type) {
  case tracer::ValueType::Int32:
  case tracer::ValueType::Int64:
    code += "ctx.arguments[" + std::to_string(i) +
            "].signed_value=static_cast<int64_t>(a" + std::to_string(i) +
            ");\n";
    break;
  case tracer::ValueType::UInt64:
    code += "ctx.arguments[" + std::to_string(i) +
            "].unsigned_value=static_cast<uint64_t>(a" + std::to_string(i) +
            ");\n";
    break;
  case tracer::ValueType::Float32:
  case tracer::ValueType::Float64:
    code += "ctx.arguments[" + std::to_string(i) +
            "].real_value=static_cast<double>(a" + std::to_string(i) + ");\n";
    break;
  case tracer::ValueType::Pointer:
    code += "ctx.arguments[" + std::to_string(i) +
            "].unsigned_value=reinterpret_cast<uintptr_t>(a" +
            std::to_string(i) + ");\n";
    break;
  case tracer::ValueType::Object:
    code += "ctx.arguments[" + std::to_string(i) +
            "].unsigned_value=reinterpret_cast<uintptr_t>(&a" +
            std::to_string(i) + ");\n";
    break;
  default:
    break;
  }
}

class DiagnosticCaptureScope {
public:
  explicit DiagnosticCaptureScope(clang::DiagnosticsEngine &diagnostics)
      : diagnostics_(diagnostics), old_owner_(diagnostics_.takeClient()),
        consumer_(old_owner_.get()) {
    diagnostics_.setClient(&consumer_, false);
  }

  ~DiagnosticCaptureScope() {
    diagnostics_.setClient(old_owner_.release(), true);
  }

  const std::string &message() const { return consumer_.message(); }

  bool had_error() const { return consumer_.had_error(); }

private:
  clang::DiagnosticsEngine &diagnostics_;
  std::unique_ptr<clang::DiagnosticConsumer> old_owner_;
  DiagnosticCapture consumer_;
};

bool add_jump(std::uint8_t *at, const void *destination) {
  at[0] = 0xff;
  at[1] = 0x25;
  std::memset(at + 2, 0, 4);
  const auto address = reinterpret_cast<std::uintptr_t>(destination);
  std::memcpy(at + 6, &address, sizeof(address));
  return true;
}

bool mapping(const void *address, std::uintptr_t &begin, std::uintptr_t &end,
             int &protection) {
  std::ifstream maps("/proc/self/maps");
  std::string line;
  const auto needle = reinterpret_cast<std::uintptr_t>(address);
  while (std::getline(maps, line)) {
    unsigned long long lo = 0, hi = 0;
    char perms[5]{};
    if (std::sscanf(line.c_str(), "%llx-%llx %4s", &lo, &hi, perms) == 3 &&
        needle >= lo && needle < hi) {
      begin = lo;
      end = hi;
      protection = 0;
      if (perms[0] == 'r') {
        protection |= PROT_READ;
      }
      if (perms[1] == 'w') {
        protection |= PROT_WRITE;
      }
      if (perms[2] == 'x') {
        protection |= PROT_EXEC;
      }
      return true;
    }
  }
  return false;
}

bool initialize_llvm(std::string &error) {
  static std::once_flag once;
  static std::string init_error;
  std::call_once(once, [] {
    if (llvm::InitializeNativeTarget() ||
        llvm::InitializeNativeTargetAsmPrinter() ||
        llvm::InitializeNativeTargetDisassembler()) {
      init_error = "LLVM native target initialization failed";
    }
  });
  if (!init_error.empty()) {
    error = init_error;
    return false;
  }
  return true;
}

struct Decoder {
  std::unique_ptr<llvm::MCRegisterInfo> regs;
  std::unique_ptr<llvm::MCAsmInfo> asm_info;
  std::unique_ptr<llvm::MCSubtargetInfo> sub;
  std::unique_ptr<llvm::MCInstrInfo> ins;
  std::unique_ptr<llvm::MCContext> context;
  std::unique_ptr<llvm::MCDisassembler> dis;
};

bool make_decoder(Decoder &d, std::string &error) {
  if (!initialize_llvm(error)) {
    return false;
  }
  auto triple = llvm::sys::getDefaultTargetTriple();
  std::string lookup_error;
  auto *target = llvm::TargetRegistry::lookupTarget(triple, lookup_error);
  if (!target) {
    error = "LLVM target lookup failed: " + lookup_error;
    return false;
  }
  llvm::MCTargetOptions options;
  llvm::Triple parsed(triple);
  d.regs.reset(target->createMCRegInfo(triple));
  d.ins.reset(target->createMCInstrInfo());
  d.sub.reset(target->createMCSubtargetInfo(triple, "generic", ""));
  if (!d.regs || !d.ins || !d.sub) {
    error = "LLVM MC metadata unavailable";
    return false;
  }
  d.asm_info.reset(target->createMCAsmInfo(*d.regs, triple, options));
  if (!d.asm_info) {
    error = "LLVM MC assembly metadata unavailable";
    return false;
  }
  d.context = std::make_unique<llvm::MCContext>(parsed, d.asm_info.get(),
                                                d.regs.get(), d.sub.get());
  d.dis.reset(target->createMCDisassembler(*d.sub, *d.context));
  if (!d.dis) {
    error = "LLVM MC disassembler unavailable";
    return false;
  }
  return true;
}

bool safe_instruction(const llvm::MCInst &inst, const llvm::MCInstrInfo &ins,
                      const llvm::MCRegisterInfo &regs) {
  const auto &desc = ins.get(inst.getOpcode());
  if (desc.isBranch() || desc.isCall() || desc.isReturn() ||
      desc.isIndirectBranch()) {
    return false;
  }
  const std::string name = ins.getName(inst.getOpcode()).str();
  static constexpr std::string_view allowed[] = {
      "PUSH64r",   "POP64r",   "MOV64rr",    "MOV32rr",   "MOV16rr",
      "MOV8rr",    "MOV32ri",  "MOV64rm",    "MOV32rm",   "MOV16rm",
      "MOV8rm",    "MOV64mr",  "MOV32mr",    "MOV16mr",   "MOV8mr",
      "MOVSS",     "MOVSD",    "MOVAPS",     "MOVUPS",    "LEA64r",
      "LEA32r",    "SUB64ri8", "SUB64ri32",  "ADD64ri8",  "ADD64ri32",
      "ADD32ri8",  "ADD32ri",  "IMUL32rri8", "IMUL32rri", "IMUL32rmi8",
      "IMUL32rmi", "XOR64rr",  "XOR32rr",    "TEST64rr",  "TEST32rr",
      "CMP64rr",   "CMP32rr",  "AND64ri8",   "AND64ri32", "NOP",
      "NOOP"};
  bool known = false;
  for (auto p : allowed) {
    if (name == p || (name.size() > p.size() && name.starts_with(p))) {
      known = true;
      break;
    }
  }
  if (!known) {
    return false;
  }
  for (unsigned i = 0; i < inst.getNumOperands(); ++i) {
    if (inst.getOperand(i).isReg()) {
      auto r = inst.getOperand(i).getReg();
      const char *n = r ? regs.getName(r) : nullptr;
      if (n && (!std::strcmp(n, "RIP") || !std::strcmp(n, "EIP"))) {
        return false;
      }
    }
  }
  return true;
}

bool decode_patch(const std::uint8_t *entry, tracer::Request &req,
                  std::string &error) {
  std::uintptr_t lo = 0, hi = 0;
  int prot = 0;
  if (!mapping(entry, lo, hi, prot) ||
      hi - reinterpret_cast<std::uintptr_t>(entry) < kJumpBytes) {
    error = "target entry has insufficient mapped bytes";
    return false;
  }
  if (entry[0] == 0xf3 && entry[1] == 0x0f && entry[2] == 0x1e &&
      entry[3] == 0xfa) {
    error = "target entry has an ENDBR64 CET landing instruction";
    return false;
  }
  Decoder d;
  if (!make_decoder(d, error)) {
    return false;
  }
  const auto address = reinterpret_cast<std::uintptr_t>(entry);
  std::size_t used = 0;
  while (used < kJumpBytes) {
    llvm::MCInst inst;
    std::uint64_t size = 0;
    auto status = d.dis->getInstruction(
        inst, size,
        llvm::ArrayRef<std::uint8_t>(
            entry + used, std::min<std::size_t>(kMaxDecodedBytes - used,
                                                hi - address - used)),
        address + used, llvm::nulls());
    if (status != llvm::MCDisassembler::Success || !size ||
        !safe_instruction(inst, *d.ins, *d.regs)) {
      error = "target entry contains an unsupported or PC-relative instruction";
      return false;
    }
    used += size;
    if (used > kMaxDecodedBytes) {
      error = "target entry exceeds safe patch window";
      return false;
    }
  }
  req.patch_size = static_cast<std::uint32_t>(used);
  std::memcpy(req.original, entry, used);
  return true;
}

std::string make_wrapper(const tracer::Request &r,
                         std::uintptr_t original_trampoline, bool has_pre,
                         bool has_post,
                         const clangtracer::v1::ProbeSpec *spec = nullptr) {
  auto argument_type = [&](unsigned i) -> std::string {
    if (r.arguments[i] != tracer::ValueType::Object)
      return type_name(r.arguments[i]);
    if (!spec || !spec->has_debug_info() ||
        i >= static_cast<unsigned>(spec->debug_info().arguments_size()))
      throw std::runtime_error("missing object argument ABI metadata");
    const auto &argument = spec->debug_info().arguments(i);
    if (!argument.is_object_value() || !argument.has_object_type())
      throw std::runtime_error("invalid by-value argument metadata");
    return "::" + argument.object_type().record_name();
  };
  std::string code =
      "extern \"C\" long long tracer_callback_dispatch(unsigned long "
      "long,const tracer::TraceContext*) noexcept;\nextern \"C\" void "
      "tracer_event_dispatch(unsigned long long,const "
      "tracer::TraceContext*,long long) noexcept;\nextern \"C\" void "
      "tracer_probe_enter(unsigned long long) noexcept;\nextern \"C\" void "
      "tracer_probe_leave(unsigned long long) noexcept;\nextern \"C\" "
      "unsigned long long tracer_next_call_id(unsigned long long) noexcept;\n";
  code += "extern \"C\" void tracer_stack_enter(tracer::TraceContext*,void*) noexcept;\n"
          "extern \"C\" void tracer_stack_leave(tracer::TraceContext*) noexcept;\n"
          "extern \"C\" void tracer_capture_entry_stack(const tracer::TraceContext*,unsigned) noexcept;\n"
          "extern \"C\" void tracer_capture_stack(const tracer::TraceContext*,unsigned,bool) noexcept;\n";
  code += "using Original= ";
  code += type_name(r.result);
  code += " (*) (";
  bool comma = false;
  if (r.kind == tracer::CallableKind::Method) {
    code += "void*";
    comma = true;
  }
  for (unsigned i = 0; i < r.argument_count; ++i) {
    if (comma) {
      code += ",";
    }
    code += argument_type(i);
    comma = true;
  }
  if (!comma) {
    code += "void";
  }
  code += ");\nextern \"C\" ";
  code += type_name(r.result);
  code += " clangtracer_wrapper(";
  comma = false;
  if (r.kind == tracer::CallableKind::Method) {
    code += "void* self";
    comma = true;
  }
  for (unsigned i = 0; i < r.argument_count; ++i) {
    if (comma) {
      code += ",";
    }
    code += argument_type(i) + " a" + std::to_string(i);
    comma = true;
  }
  if (!comma) {
    code += "void";
  }
  code += "){\ntracer_probe_enter(" + std::to_string(r.probe_id) +
          "ULL); struct FlightGuard{unsigned long long id; "
          "~FlightGuard(){tracer_probe_leave(id);}} flight{" +
          std::to_string(r.probe_id) +
          "ULL};\nstatic constexpr tracer::TraceContext empty_context{}; "
          "tracer::TraceContext ctx=empty_context; ctx.probe_id=" +
          std::to_string(r.probe_id) + "ULL; ctx.call_id=tracer_next_call_id(" +
          std::to_string(r.probe_id) +
          "ULL); ctx.argument_count=" + std::to_string(r.argument_count) +
          "; ctx.phase=tracer::TracePhase::Pre;\n";
  code += "ctx.target_entry=" + std::to_string(r.original_entry) +
          "ULL; ctx.bridge_begin=reinterpret_cast<uintptr_t>(&&original_begin);"
          "ctx.bridge_end=reinterpret_cast<uintptr_t>(&&original_end);"
          "ctx.stack_capture=&tracer_capture_stack;"
          "tracer_stack_enter(&ctx,__builtin_frame_address(0));"
          "struct StackGuard { tracer::TraceContext *ctx;"
          "~StackGuard(){tracer_stack_leave(ctx);} } stack_guard{&ctx};\n";
  code += std::string("ctx.stack_application_only=") +
          (spec && spec->include_internal_stack_frames() ? "false;\n" : "true;\n");
  code += std::string("ctx.printing.arguments=") +
          (spec && spec->print_arguments() ? "true;\n" : "false;\n");
  code += std::string("ctx.printing.result=") +
          (spec && spec->print_result() ? "true;\n" : "false;\n");
  if (r.kind == tracer::CallableKind::Method) {
    code += "ctx.object_address=reinterpret_cast<uintptr_t>(self);\n";
  }
  for (unsigned i = 0; i < r.argument_count; ++i) {
    fill_value(code, i, r.arguments[i]);
  }
  if (spec && spec->stack_depth())
    code += "tracer_capture_entry_stack(&ctx," + std::to_string(spec->stack_depth()) + ");\n";
  if (has_pre) {
    code += "(void)tracer_callback_dispatch(" + std::to_string(r.probe_id) +
            "ULL,&ctx);\n";
  }
  code += "auto original=reinterpret_cast<Original>(" +
          std::to_string(original_trampoline) + "ULL);\n";
  std::string call = "original(";
  comma = false;
  if (r.kind == tracer::CallableKind::Method) {
    call += "self";
    comma = true;
  }
  for (unsigned i = 0; i < r.argument_count; ++i) {
    if (comma) {
      call += ",";
    }
    call += "a" + std::to_string(i);
    comma = true;
  }
  call += ")";
  if (r.result == tracer::ValueType::Void) {
    code += "original_begin: " + call + ";\noriginal_end: ctx.result.type=tracer::ValueType::Void;\n";
  } else {
    code += std::string(type_name(r.result)) + " result; original_begin: result=" + call + ";\noriginal_end: ;\n";
    code += "ctx.result.type=static_cast<tracer::ValueType>(" +
            std::to_string(static_cast<unsigned>(r.result)) + ");\n";
    switch (r.result) {
    case tracer::ValueType::Int32:
    case tracer::ValueType::Int64:
      code += "ctx.result.signed_value=static_cast<int64_t>(result);\n";
      break;
    case tracer::ValueType::UInt64:
      code += "ctx.result.unsigned_value=static_cast<uint64_t>(result);\n";
      break;
    case tracer::ValueType::Float32:
    case tracer::ValueType::Float64:
      code += "ctx.result.real_value=static_cast<double>(result);\n";
      break;
    case tracer::ValueType::Pointer:
      code +=
          "ctx.result.unsigned_value=reinterpret_cast<uintptr_t>(result);\n";
      break;
    default:
      break;
    }
  }
  if (has_post) {
    code +=
        "ctx.phase=tracer::TracePhase::Post; (void)tracer_callback_dispatch(" +
        std::to_string(r.probe_id) + "ULL,&ctx);\n";
  }
  if (r.result == tracer::ValueType::Void) {
    code += "return;\n}";
  } else {
    code += "return result;\n}";
  }
  return code;
}

struct CallbackShape {
  bool class_mode = false;
  bool has_pre = false;
  bool has_post = false;
  const clang::FunctionDecl *legacy = nullptr;
  const clang::CXXMethodDecl *pre = nullptr;
  const clang::CXXMethodDecl *post = nullptr;
};

bool is_long_long(clang::QualType type) {
  const auto *builtin = type->getAs<clang::BuiltinType>();
  return builtin && builtin->getKind() == clang::BuiltinType::LongLong;
}

bool is_call_reference(clang::QualType type, llvm::StringRef name) {
  const auto *reference = type->getAs<clang::LValueReferenceType>();
  if (!reference || !reference->getPointeeType().isConstQualified()) {
    return false;
  }
  const auto *record = reference->getPointeeType()->getAsCXXRecordDecl();
  return record && llvm::StringRef(record->getQualifiedNameAsString()) == name;
}

bool has_identifier(const clang::NamedDecl &declaration, llvm::StringRef name) {
  const clang::IdentifierInfo *identifier = declaration.getIdentifier();
  return identifier && identifier->getName() == name;
}

bool inspect_callback_shape(clang::ASTContext &context, CallbackShape &shape,
                            std::string &error) {
  const clang::TranslationUnitDecl *unit = context.getTranslationUnitDecl();
  const clang::CXXRecordDecl *interceptor = nullptr;
  const clang::FunctionDecl *legacy = nullptr;
  std::vector<const clang::FunctionDecl *> legacy_declarations;
  bool saw_interceptor = false;
  bool saw_legacy = false;
  auto inspect_function = [&](const clang::Decl *declaration) {
    const auto *function = llvm::dyn_cast<clang::FunctionDecl>(declaration);
    if (!function || !has_identifier(*function, "trace_callback")) {
      return;
    }
    saw_legacy = true;
    const auto *canonical = function->getCanonicalDecl();
    if (std::find(legacy_declarations.begin(), legacy_declarations.end(),
                  canonical) == legacy_declarations.end()) {
      legacy_declarations.push_back(canonical);
    }
    if (function->doesThisDeclarationHaveABody()) {
      if (legacy) {
        error = "callback source defines trace_callback more than once";
        return;
      }
      legacy = function;
    }
  };
  for (const clang::Decl *decl : unit->decls()) {
    if (const auto *record = llvm::dyn_cast<clang::CXXRecordDecl>(decl);
        record && has_identifier(*record, "Interceptor")) {
      saw_interceptor = true;
      if (record->isThisDeclarationADefinition()) {
        if (interceptor) {
          error = "callback source defines more than one top-level Interceptor";
          return false;
        }
        interceptor = record;
      }
    }
    if (const auto *linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(decl)) {
      for (const clang::Decl *nested : linkage->decls()) {
        inspect_function(nested);
        if (!error.empty()) {
          return false;
        }
      }
    } else {
      inspect_function(decl);
      if (!error.empty()) {
        return false;
      }
    }
  }
  if (saw_interceptor && !interceptor) {
    error = "Interceptor must be a complete top-level class or struct";
    return false;
  }
  if (interceptor && saw_legacy) {
    error = "callback source cannot define both Interceptor and trace_callback";
    return false;
  }
  if (legacy_declarations.size() > 1) {
    error = "trace_callback must not be overloaded";
    return false;
  }
  if (interceptor) {
    shape.class_mode = true;
    for (const clang::Decl *member : interceptor->decls()) {
      const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(member);
      if (!method || (!has_identifier(*method, "pre") &&
                      !has_identifier(*method, "post"))) {
        continue;
      }
      const bool is_pre = has_identifier(*method, "pre");
      const clang::CXXMethodDecl *&selected = is_pre ? shape.pre : shape.post;
      if (selected) {
        error = std::string("Interceptor has an overloaded ") +
                (is_pre ? "pre" : "post") + " hook; exactly one is allowed";
        return false;
      }
      selected = method;
      const bool valid_return = method->getReturnType()->isVoidType() ||
                                is_long_long(method->getReturnType());
      if (method->isStatic() || method->getAccess() != clang::AS_public ||
          method->getNumParams() != 1 ||
          !is_call_reference(method->getParamDecl(0)->getType(),
                             "tracer::Call") ||
          !valid_return || !method->hasBody()) {
        error = std::string("Interceptor::") + (is_pre ? "pre" : "post") +
                " must be a public, defined instance method taking const Call& "
                "and returning void or long long";
        return false;
      }
    }
    shape.has_pre = shape.pre != nullptr;
    shape.has_post = shape.post != nullptr;
    if (!shape.has_pre && !shape.has_post) {
      error =
          "Interceptor must define at least one public pre() or post() hook";
      return false;
    }
    return true;
  }
  if (!legacy) {
    error = "callback source must define Interceptor or trace_callback";
    return false;
  }
  shape.legacy = legacy;
  shape.has_pre = true;
  if (legacy->getNumParams() != 1 || !is_long_long(legacy->getReturnType()) ||
      !is_call_reference(legacy->getParamDecl(0)->getType(),
                         "tracer::TraceContext")) {
    error = "trace_callback must be defined as long long(const TraceContext&)";
    return false;
  }
  return true;
}

clang::TypeAliasDecl *
create_receiver_alias(clang::ASTContext &context,
                      const clang::CXXRecordDecl *imported_record,
                      const clangtracer::v1::ReceiverContext &receiver) {
  auto &tracer_identifier = context.Idents.get("tracer");
  clang::NamespaceDecl *tracer_namespace = nullptr;
  for (clang::NamedDecl *named :
       context.getTranslationUnitDecl()->lookup(&tracer_identifier)) {
    if ((tracer_namespace = llvm::dyn_cast<clang::NamespaceDecl>(named))) {
      break;
    }
  }
  if (!tracer_namespace) {
    tracer_namespace = clang::NamespaceDecl::Create(
        context, context.getTranslationUnitDecl(), false,
        clang::SourceLocation(), clang::SourceLocation(), &tracer_identifier,
        nullptr, false);
    context.getTranslationUnitDecl()->addDecl(tracer_namespace);
  }

  auto &alias_identifier = context.Idents.get("ResolvedTarget");
  for (clang::NamedDecl *named : tracer_namespace->lookup(&alias_identifier)) {
    if (auto *existing = llvm::dyn_cast<clang::TypeAliasDecl>(named)) {
      return existing;
    }
  }
  clang::QualType receiver_type = context.getTypeDeclType(
      static_cast<const clang::TypeDecl *>(imported_record));
  receiver_type.addConst();
  if (receiver.is_volatile()) {
    receiver_type.addVolatile();
  }
  auto *type_info = context.CreateTypeSourceInfo(receiver_type);
  auto *alias = clang::TypeAliasDecl::Create(
      context, tracer_namespace, clang::SourceLocation(),
      clang::SourceLocation(), &alias_identifier, type_info);
  tracer_namespace->addDecl(alias);
  return alias;
}

struct ReceiverCallbackShape {
  bool valid = false;
  bool class_mode = false;
  bool has_pre = false;
  bool has_post = false;
  std::string bridge;
  std::string error;
  std::vector<std::pair<unsigned, std::string>> inferred_fields;
  std::vector<std::string> argument_names;
};

std::string make_class_bridge(const CallbackShape &shape);

class ReceiverShapeConsumer final : public clang::ASTConsumer {
public:
  explicit ReceiverShapeConsumer(ReceiverCallbackShape &result)
      : result_(result) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    CallbackShape shape;
    if (!inspect_callback_shape(context, shape, result_.error)) {
      return;
    }
    class Fields : public clang::RecursiveASTVisitor<Fields> {
    public:
      Fields(clang::ASTContext &c, ReceiverCallbackShape &r)
          : context(c), result(r) {}
      int origin(clang::Expr *e, unsigned depth = 0) {
        if (!e || depth > 16)
          return -1;
        e = e->IgnoreParenImpCasts();
        if (auto *ref = llvm::dyn_cast<clang::DeclRefExpr>(e)) {
          if (auto *var = llvm::dyn_cast<clang::VarDecl>(ref->getDecl()))
            return origin(var->getInit(), depth + 1);
        }
        if (auto *clean = llvm::dyn_cast<clang::ExprWithCleanups>(e))
          return origin(clean->getSubExpr(), depth + 1);
        if (auto *call = llvm::dyn_cast<clang::CXXMemberCallExpr>(e)) {
          auto *method = call->getMethodDecl();
          if (method && method->getNameAsString() == "argument" &&
              method->getParent()->getQualifiedNameAsString() ==
                  "tracer::Call" &&
              call->getNumArgs() == 0) {
            if (const auto *args = method->getTemplateSpecializationArgs();
                args && args->size() == 1 &&
                args->get(0).getKind() == clang::TemplateArgument::Integral)
              return args->get(0).getAsIntegral().getLimitedValue(8);
          }
        }
        return -1;
      }
      bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *call) {
        const auto *method = call->getMethodDecl();
        if (!method || method->getNameAsString() != "field" ||
            method->getParent()->getQualifiedNameAsString() != "tracer::Call" ||
            call->getNumArgs() != 1)
          return true;
        auto *member = llvm::dyn_cast<clang::MemberExpr>(
            call->getArg(0)->IgnoreParenImpCasts());
        auto loc = call->getArg(0)->getBeginLoc();
        auto *local = llvm::dyn_cast<clang::DeclRefExpr>(
            call->getArg(0)->IgnoreParenImpCasts());
        if ((!member ||
             !llvm::isa<clang::FieldDecl>(member->getMemberDecl())) &&
            !local) {
          result.error = "cannot infer value name: use a member/local variable "
                         "or field(name, value)";
          return false;
        }
        if (loc.isMacroID() ||
            !context.getSourceManager().isWrittenInMainFile(loc)) {
          result.error = "cannot infer value names inside macros or headers";
          return false;
        }
        std::string label = member ? member->getMemberDecl()->getNameAsString()
                                   : local->getDecl()->getNameAsString();
        const int index = origin(member ? member->getBase() : local);
        if (index >= 0 &&
            static_cast<std::size_t>(index) < result.argument_names.size())
          label = member ? result.argument_names[index] + "." + label
                         : result.argument_names[index];
        result.inferred_fields.emplace_back(
            context.getSourceManager().getFileOffset(loc), label);
        return true;
      }
      clang::ASTContext &context;
      ReceiverCallbackShape &result;
    } fields(context, result_);
    fields.TraverseDecl(context.getTranslationUnitDecl());
    if (!result_.error.empty())
      return;
    result_.class_mode = shape.class_mode;
    result_.has_pre = shape.has_pre;
    result_.has_post = shape.has_post;
    if (shape.class_mode) {
      result_.bridge = make_class_bridge(shape);
    } else {
      result_.bridge =
          "extern \"C\" long long tracer_user_dispatch(const "
          "tracer::TraceContext *raw){ return trace_callback(*raw); }\n";
    }
    result_.valid = true;
  }

private:
  ReceiverCallbackShape &result_;
};

class ReceiverASTImporter : public clang::ASTImporter {
public:
  using clang::ASTImporter::ASTImporter;

protected:
  llvm::Expected<clang::Decl *> ImportImpl(clang::Decl *from) override {
    if (auto *function = llvm::dyn_cast<clang::FunctionDecl>(from);
        function && !function->isDefaulted() && !function->isDeleted() &&
        !function->isLateTemplateParsed()) {
      const clang::FunctionDecl *definition = nullptr;
      clang::Stmt *body = function->getBody(definition);
      // Never move a body from a sibling redeclaration onto this declaration.
      if (body && definition == function &&
          function->doesThisDeclarationHaveABody()) {
        class BodyGuard {
        public:
          BodyGuard(clang::FunctionDecl &function, clang::Stmt *body)
              : function_(function), body_(body),
                constructor_(
                    llvm::dyn_cast<clang::CXXConstructorDecl>(&function)),
                initializer_count_(
                    constructor_ ? constructor_->getNumCtorInitializers() : 0) {
            function_.setBody(nullptr);
            if (constructor_) {
              constructor_->setNumCtorInitializers(0);
            }
          }

          ~BodyGuard() {
            if (constructor_) {
              constructor_->setNumCtorInitializers(initializer_count_);
            }
            function_.setBody(body_);
          }

        private:
          clang::FunctionDecl &function_;
          clang::Stmt *body_;
          clang::CXXConstructorDecl *constructor_;
          unsigned initializer_count_;
        } guard(*function, body);

        return clang::ASTImporter::ImportImpl(from);
      }
    }
    if (auto *field = llvm::dyn_cast<clang::FieldDecl>(from)) {
      if (clang::Expr *initializer = field->getInClassInitializer()) {
        class InitializerGuard {
        public:
          InitializerGuard(clang::FieldDecl &field, clang::Expr *initializer)
              : field_(field), initializer_(initializer) {
            field_.setInClassInitializer(nullptr);
          }

          ~InitializerGuard() { field_.setInClassInitializer(initializer_); }

        private:
          clang::FieldDecl &field_;
          clang::Expr *initializer_;
        } guard(*field, initializer);

        return clang::ASTImporter::ImportImpl(from);
      }
    }
    return clang::ASTImporter::ImportImpl(from);
  }
};

class TrackingASTImporter final : public ReceiverASTImporter {
public:
  using ReceiverASTImporter::ReceiverASTImporter;

  void Imported(clang::Decl *, clang::Decl *imported) override {
    if (imported) {
      declarations.insert(imported->getCanonicalDecl());
    }
  }

  std::unordered_set<const clang::Decl *> declarations;
};

class ReceiverImportAction final : public clang::ASTFrontendAction {
public:
  ReceiverImportAction(clang::ASTContext &donor_context,
                       clang::FileManager &donor_files,
                       clang::CXXRecordDecl *donor_record,
                       ReceiverCallbackShape &shape,
                       const clangtracer::v1::ReceiverContext *receiver_context,
                       clang::CXXRecordDecl *target_record,
                       std::string *validation_error,
                       const DebugRecords *records = nullptr)
      : records_(records), donor_context_(donor_context),
        donor_files_(donor_files), donor_record_(donor_record), shape_(shape),
        receiver_context_(receiver_context), target_record_(target_record),
        validation_error_(validation_error) {}

  bool BeginSourceFileAction(clang::CompilerInstance &compiler) override {
    if (!clang::ASTFrontendAction::BeginSourceFileAction(compiler)) {
      return false;
    }
    diagnostic_capture_ =
        std::make_unique<DiagnosticCaptureScope>(compiler.getDiagnostics());
    return true;
  }

  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &, llvm::StringRef) override {
    return std::make_unique<ReceiverShapeConsumer>(shape_);
  }

  void ExecuteAction() override {
    auto &compiler = getCompilerInstance();
    compiler.createSema(getTranslationUnitKind(), nullptr);
    TrackingASTImporter importer(compiler.getASTContext(),
                                 compiler.getFileManager(), donor_context_,
                                 donor_files_, false);
    std::vector<clang::CXXRecordDecl *> imported_targets;
    if (records_)
      for (const auto &entry : *records_) {
        if (auto err = importer.ImportDefinition(entry.decl)) {
          *validation_error_ = llvm::toString(std::move(err));
          return;
        }
        auto imported = importer.Import(entry.decl);
        if (!imported) {
          *validation_error_ = llvm::toString(imported.takeError());
          return;
        }
        auto *target = llvm::cast<clang::CXXRecordDecl>(*imported);
        if (!validate_receiver_layout(compiler.getASTContext(), target,
                                      entry.layout, *validation_error_))
          return;
        imported_targets.push_back(target);
      }
    if (!donor_record_) {
      *validation_error_ = "imported receiver declaration is missing";
      return;
    }
    if (llvm::Error import_error = importer.ImportDefinition(donor_record_)) {
      *validation_error_ = llvm::toString(std::move(import_error));
      return;
    }
    auto imported_decl = importer.Import(donor_record_);
    if (!imported_decl) {
      *validation_error_ = llvm::toString(imported_decl.takeError());
      return;
    }
    auto *imported_record_const =
        llvm::dyn_cast<clang::CXXRecordDecl>(*imported_decl);
    auto *imported_record =
        const_cast<clang::CXXRecordDecl *>(imported_record_const);
    if (!imported_record || !receiver_context_ ||
        !validate_receiver_layout(compiler.getASTContext(), imported_record,
                                  *receiver_context_, *validation_error_)) {
      if (validation_error_->empty()) {
        *validation_error_ = "receiver declaration did not survive AST import";
      } else {
        *validation_error_ =
            "preflight receiver layout mismatch: " + *validation_error_;
      }
      return;
    }
    if (!create_receiver_alias(compiler.getASTContext(), imported_record,
                               *receiver_context_)) {
      *validation_error_ =
          "cannot create typed receiver alias in preflight AST";
      return;
    }
    if (target_record_) {
      // Skip imported receiver method bodies while checking callback and
      // included helper code for ODR uses of the selected target.
      visitor_ = std::make_unique<ForbiddenReceiverUseVisitor>(
          imported_record, importer.declarations, imported_targets);
    }
    clang::ParseAST(compiler.getSema());
    if (diagnostic_capture_ && diagnostic_capture_->had_error()) {
      *validation_error_ = diagnostic_capture_->message();
    }
    if (validation_error_->empty() && !shape_.error.empty()) {
      *validation_error_ = shape_.error;
    }
    if (visitor_) {
      visitor_->TraverseDecl(compiler.getASTContext().getTranslationUnitDecl());
      if (!visitor_->error().empty()) {
        *validation_error_ = visitor_->error();
      }
    }
  }

private:
  class ForbiddenReceiverUseVisitor final
      : public clang::RecursiveASTVisitor<ForbiddenReceiverUseVisitor> {
  public:
    ForbiddenReceiverUseVisitor(
        clang::CXXRecordDecl *target,
        const std::unordered_set<const clang::Decl *> &imported,
        const std::vector<clang::CXXRecordDecl *> &targets)
        : imported_(imported) {
      targets_.insert(target->getCanonicalDecl());
      for (auto *t : targets)
        targets_.insert(t->getCanonicalDecl());
    }
    bool is_target(const clang::CXXRecordDecl *record) const {
      return record && targets_.count(record->getCanonicalDecl());
    }

    bool shouldVisitTemplateInstantiations() const { return true; }

    bool shouldVisitImplicitCode() const { return true; }

    bool TraverseCXXRecordDecl(clang::CXXRecordDecl *record) {
      if (record && is_target(record)) {
        return true;
      }
      return clang::RecursiveASTVisitor<
          ForbiddenReceiverUseVisitor>::TraverseCXXRecordDecl(record);
    }

    bool TraverseFunctionDecl(clang::FunctionDecl *function) {
      if (function && function->getNameAsString() == "clangtracer_wrapper")
        return true;
      if (function && function->doesThisDeclarationHaveABody() &&
          imported_.count(function->getCanonicalDecl()) != 0) {
        return true;
      }
      return clang::RecursiveASTVisitor<
          ForbiddenReceiverUseVisitor>::TraverseFunctionDecl(function);
    }

    bool TraverseVarDecl(clang::VarDecl *variable) {
      if (variable && variable->hasGlobalStorage() && variable->hasInit() &&
          imported_.count(variable->getCanonicalDecl()) != 0) {
        return true;
      }
      return clang::RecursiveASTVisitor<
          ForbiddenReceiverUseVisitor>::TraverseVarDecl(variable);
    }

    bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *expression) {
      if (const auto *member =
              llvm::dyn_cast<clang::MemberExpr>(expression->getCallee())) {
        if (const auto *method =
                llvm::dyn_cast<clang::CXXMethodDecl>(member->getMemberDecl());
            method && is_target(method->getParent())) {
          error_ = "callback cannot call methods on the imported receiver";
        }
      }
      return true;
    }

    bool VisitMemberExpr(clang::MemberExpr *expression) {
      if (imported_.count(expression->getMemberDecl()->getCanonicalDecl()) !=
              0 &&
          llvm::isa<clang::CXXMethodDecl>(expression->getMemberDecl())) {
        error_ = "callback cannot call or take the address of imported target "
                 "methods";
      }
      if (const auto *method =
              llvm::dyn_cast<clang::CXXMethodDecl>(expression->getMemberDecl());
          method && is_target(method->getParent())) {
        error_ = "callback cannot call or take the address of imported "
                 "receiver methods";
      }
      return true;
    }

    bool VisitDeclRefExpr(clang::DeclRefExpr *expression) {
      if (imported_.count(expression->getDecl()->getCanonicalDecl()) != 0) {
        if (const auto *function =
                llvm::dyn_cast<clang::FunctionDecl>(expression->getDecl());
            function && function->getDefinition()) {
          error_ = "callback cannot call or take the address of imported "
                   "target functions";
        } else if (const auto *variable =
                       llvm::dyn_cast<clang::VarDecl>(expression->getDecl());
                   variable && variable->hasGlobalStorage()) {
          error_ = "callback cannot odr-use imported target globals";
        }
      }
      if (const auto *method =
              llvm::dyn_cast<clang::CXXMethodDecl>(expression->getDecl());
          method && is_target(method->getParent())) {
        error_ = "callback cannot call or take the address of imported "
                 "receiver methods";
      } else if (const auto *variable =
                     llvm::dyn_cast<clang::VarDecl>(expression->getDecl());
                 variable && variable->getDeclContext()->isRecord() &&
                 is_target(llvm::dyn_cast<clang::CXXRecordDecl>(
                     variable->getDeclContext()))) {
        error_ =
            "callback cannot odr-use imported receiver static data members";
      }
      return true;
    }

    bool VisitCXXConstructExpr(clang::CXXConstructExpr *expression) {
      if (auto *record = expression->getType()->getAsCXXRecordDecl();
          record && is_target(record)) {
        error_ =
            "callback cannot construct objects of the imported receiver type";
      }
      return true;
    }

    bool VisitInitListExpr(clang::InitListExpr *expression) {
      if (auto *record = expression->getType()->getAsCXXRecordDecl();
          record && is_target(record)) {
        error_ =
            "callback cannot construct objects of the imported receiver type";
      }
      return true;
    }

    bool VisitCXXNewExpr(clang::CXXNewExpr *expression) {
      if (auto *record = expression->getAllocatedType()->getAsCXXRecordDecl();
          record && is_target(record)) {
        error_ =
            "callback cannot allocate objects of the imported receiver type";
      }
      return true;
    }

    bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *expression) {
      const auto argument_type = expression->getArgument()->getType();
      if (argument_type->isPointerType()) {
        if (auto *record =
                argument_type->getPointeeType()->getAsCXXRecordDecl();
            record && is_target(record)) {
          error_ =
              "callback cannot destroy objects of the imported receiver type";
        }
      }
      return true;
    }

    const std::string &error() const { return error_; }

  private:
    std::unordered_set<const clang::CXXRecordDecl *> targets_;
    const std::unordered_set<const clang::Decl *> &imported_;
    std::string error_;
  };

  const DebugRecords *records_;
  clang::ASTContext &donor_context_;
  clang::FileManager &donor_files_;
  clang::CXXRecordDecl *donor_record_;
  ReceiverCallbackShape &shape_;
  const clangtracer::v1::ReceiverContext *receiver_context_;
  clang::CXXRecordDecl *target_record_;
  std::string *validation_error_;
  std::unique_ptr<ForbiddenReceiverUseVisitor> visitor_;
  std::unique_ptr<DiagnosticCaptureScope> diagnostic_capture_;
};

std::string make_class_bridge(const CallbackShape &shape) {
  std::string code = "static Interceptor tracer_interceptor;\n"
                     "extern \"C\" long long tracer_user_dispatch(const "
                     "tracer::TraceContext *raw){ tracer::Call call(*raw); ";
  auto append_hook = [&](const clang::CXXMethodDecl *method, const char *phase,
                         const char *name) {
    if (!method) {
      return;
    }
    code += "if(raw->phase==tracer::TracePhase::";
    code += phase;
    code += "){ ";
    if (method->getReturnType()->isVoidType()) {
      code +=
          std::string("tracer_interceptor.") + name + "(call); return 0; } ";
    } else {
      code += std::string("return tracer_interceptor.") + name + "(call); } ";
    }
  };
  append_hook(shape.pre, "Pre", "pre");
  append_hook(shape.post, "Post", "post");
  code += "return 0; }\n";
  return code;
}

std::string argument_aliases(const clangtracer::v1::ProbeSpec &spec) {
  if (!spec.has_debug_info())
    return {};
  std::string code =
      "namespace tracer { template<unsigned long> struct ArgumentType;\n";
  for (const auto &argument : spec.debug_info().arguments()) {
    std::string type = argument.cpp_type();
    if (argument.has_object_type()) {
      type = "const ::" + argument.object_type().record_name();
      if (argument.cpp_type().find("volatile") != std::string::npos)
        type += " volatile";
      type +=
          (argument.is_reference() || argument.is_object_value()) ? "&" : "*";
    } else if (argument.is_reference() && !type.starts_with("const "))
      type = "const " + type;
    code += "template<> struct ArgumentType<" +
            std::to_string(argument.index()) + "> { using type=" + type +
            "; static constexpr bool object_value=" +
            (argument.is_object_value() ? "true" : "false") + "; };\n";
  }
  return code + "}\n";
}

bool compile_probe(Probe &p, clangtracer::v1::ProbeSpec &specification,
                   std::string &error) {
  const std::string &source = specification.callback_source();
  p.argument_names.assign(specification.argument_names().begin(),
                          specification.argument_names().end());
  clang::CXXRecordDecl *receiver_record = nullptr;
  if (!build_and_validate_receiver_ast(specification, p.request, p.receiver_ast,
                                       receiver_record, error,
                                       p.debug_records)) {
    return false;
  }
  llvm::sys::DynamicLibrary::AddSymbol(
      "tracer_callback_dispatch",
      reinterpret_cast<void *>(&tracer_callback_dispatch));
  llvm::sys::DynamicLibrary::AddSymbol(
      "tracer_event_dispatch",
      reinterpret_cast<void *>(&tracer_event_dispatch));
  llvm::sys::DynamicLibrary::AddSymbol(
      "tracer_probe_enter", reinterpret_cast<void *>(&tracer_probe_enter));
  llvm::sys::DynamicLibrary::AddSymbol(
      "tracer_probe_leave", reinterpret_cast<void *>(&tracer_probe_leave));
  llvm::sys::DynamicLibrary::AddSymbol(
      "tracer_next_call_id", reinterpret_cast<void *>(&tracer_next_call_id));
  llvm::sys::DynamicLibrary::AddSymbol(
      "tracer_stack_enter", reinterpret_cast<void *>(&tracer_stack_enter));
  llvm::sys::DynamicLibrary::AddSymbol(
      "tracer_stack_leave", reinterpret_cast<void *>(&tracer_stack_leave));
  llvm::sys::DynamicLibrary::AddSymbol(
      "tracer_capture_stack", reinterpret_cast<void *>(&tracer_capture_stack));
  llvm::sys::DynamicLibrary::AddSymbol(
      "tracer_capture_entry_stack", reinterpret_cast<void *>(&tracer_capture_entry_stack));
  if (!initialize_llvm(error)) {
    return false;
  }
  const std::string resource_dir_argument =
      std::string("-resource-dir=") + tracer::runtime::clang_resource_dir();
  auto compiler_arguments = tracer::runtime::jit_arguments();
  compiler_arguments.insert(compiler_arguments.end(),
                            {"-std=c++20", "-x", "c++", "-fPIC", "-O0",
                             resource_dir_argument});
  std::vector<const char *> args;
  for (const auto &argument : compiler_arguments)
    args.push_back(argument.c_str());
  clang::IncrementalCompilerBuilder builder;
  builder.SetCompilerArgs(args);
  auto compiler = builder.CreateCpp();
  if (!compiler) {
    error = llvm::toString(compiler.takeError());
    return false;
  }
  auto created = clang::Interpreter::create(std::move(*compiler));
  if (!created) {
    error = llvm::toString(created.takeError());
    return false;
  }
  p.interpreter = std::move(*created);
  const std::string prelude =
      std::string(tracer::kEmbeddedProtocolHeader) +
      "\nusing int32_t=std::int32_t; using int64_t=std::int64_t; "
      "using uint32_t=std::uint32_t; using uint64_t=std::uint64_t; "
      "using uintptr_t=std::uintptr_t;\n"
      "#define CLANG_TRACER_CALLBACK_TYPES_DEFINED 1\n";
  const std::string user_source = prelude + argument_aliases(specification) +
                                  tracer::kEmbeddedCallbackHeader + source;
  /*
   * A typed receiver preflight imports the same real Clang declaration into a
   * temporary syntax-only translation unit. This keeps callback validation
   * ahead of Interpreter::ParseAndExecute without copying the class text.
   */
  auto syntax_args = compiler_arguments;
  syntax_args.push_back("-fsyntax-only");
  CallbackShape shape;
  std::string bridge;
  std::string complete_source;
  std::string baseline_error;
  DiagnosticCapture user_capture(nullptr);
  auto user_ast = clang::tooling::buildASTFromCodeWithArgs(
      user_source, syntax_args, "clang-tracer-callback.cc", "clang-tracer",
      std::make_shared<clang::PCHContainerOperations>(),
      clang::tooling::getClangStripOutputAdjuster(), {}, &user_capture);
  bool shape_valid = false;
  if (user_ast) {
    shape_valid = inspect_callback_shape(user_ast->getASTContext(), shape,
                                         baseline_error);
  } else {
    baseline_error = user_capture.message();
  }
  if (!user_capture.message().empty() && baseline_error.empty()) {
    baseline_error = user_capture.message();
  }

  bool baseline_preflight_succeeded = false;
  if (shape_valid) {
    if (shape.class_mode) {
      bridge = make_class_bridge(shape);
    } else {
      bridge = "extern \"C\" long long tracer_user_dispatch(const "
               "tracer::TraceContext *raw){ return trace_callback(*raw); }\n";
    }
    const std::string generated =
        make_wrapper(p.request, reinterpret_cast<std::uintptr_t>(p.trampoline),
                     shape.has_pre, shape.has_post, &specification);
    complete_source = user_source + "\n" + bridge + "\n" + generated;
    DiagnosticCapture complete_capture(nullptr);
    auto complete_ast = clang::tooling::buildASTFromCodeWithArgs(
        complete_source, syntax_args, "clang-tracer-callback.cc",
        "clang-tracer", std::make_shared<clang::PCHContainerOperations>(),
        clang::tooling::getClangStripOutputAdjuster(), {}, &complete_capture);
    baseline_preflight_succeeded =
        complete_ast && !complete_capture.had_error();
    if (!baseline_preflight_succeeded) {
      baseline_error = complete_capture.message();
      if (baseline_error.empty()) {
        baseline_error = "callback source failed Clang syntax validation";
      }
    }
  }

  if (!baseline_preflight_succeeded) {
    if (!p.receiver_ast) {
      error = baseline_error.empty()
                  ? "callback source failed Clang syntax validation"
                  : baseline_error;
      return false;
    }

    auto *ci = p.interpreter->getCompilerInstance();
    if (!ci) {
      error = "Clang Interpreter has no CompilerInstance for receiver import";
      return false;
    }
    ReceiverASTImporter importer(p.interpreter->getASTContext(),
                                 ci->getFileManager(),
                                 p.receiver_ast->getASTContext(),
                                 p.receiver_ast->getFileManager(), false);
    for (const auto &entry : p.debug_records) {
      if (auto err = importer.ImportDefinition(entry.decl)) {
        error = llvm::toString(std::move(err));
        return false;
      }
      auto imported = importer.Import(entry.decl);
      if (!imported) {
        error = llvm::toString(imported.takeError());
        return false;
      }
      if (!validate_receiver_layout(p.interpreter->getASTContext(),
                                    llvm::cast<clang::CXXRecordDecl>(*imported),
                                    entry.layout, error))
        return false;
    }
    if (llvm::Error import_error = importer.ImportDefinition(receiver_record)) {
      error = "cannot import selected receiver declaration: " +
              llvm::toString(std::move(import_error));
      return false;
    }
    auto imported_decl = importer.Import(receiver_record);
    if (!imported_decl) {
      error = "cannot retrieve imported selected receiver declaration: " +
              llvm::toString(imported_decl.takeError());
      return false;
    }
    auto *imported_receiver =
        llvm::dyn_cast<clang::CXXRecordDecl>(*imported_decl);
    if (!imported_receiver ||
        !validate_receiver_layout(p.interpreter->getASTContext(),
                                  imported_receiver, specification.receiver(),
                                  error)) {
      error = "receiver layout changed during AST import: " + error;
      return false;
    }
    if (!create_receiver_alias(p.interpreter->getASTContext(),
                               imported_receiver, specification.receiver())) {
      error = "cannot create typed receiver alias in interpreter AST";
      return false;
    }

    std::string typed_user_source =
        prelude +
        (p.request.kind == tracer::CallableKind::Method
             ? "#define CLANG_TRACER_HAS_TYPED_RECEIVER 1\n"
             : "") +
        "#define CLANG_TRACER_FIELD_INFERENCE 1\n" +
        argument_aliases(specification) + tracer::kEmbeddedCallbackHeader +
        source;
    std::string import_error;
    ReceiverCallbackShape receiver_shape;
    receiver_shape.argument_names = p.argument_names;
    auto typed_action = std::make_unique<ReceiverImportAction>(
        p.receiver_ast->getASTContext(), p.receiver_ast->getFileManager(),
        receiver_record, receiver_shape, &specification.receiver(),
        receiver_record, &import_error, &p.debug_records);
    const bool user_syntax_ok = clang::tooling::runToolOnCodeWithArgs(
        std::move(typed_action), typed_user_source, syntax_args,
        "clang-tracer-callback.cc", "clang-tracer",
        std::make_shared<clang::PCHContainerOperations>());
    if (!user_syntax_ok || !receiver_shape.valid || !import_error.empty()) {
      error = import_error.empty() ? receiver_shape.error : import_error;
      if (error.empty()) {
        error = "typed receiver callback failed Clang syntax validation";
      }
      return false;
    }
    std::sort(receiver_shape.inferred_fields.rbegin(),
              receiver_shape.inferred_fields.rend());
    receiver_shape.inferred_fields.erase(
        std::unique(receiver_shape.inferred_fields.begin(),
                    receiver_shape.inferred_fields.end()),
        receiver_shape.inferred_fields.end());
    for (const auto &[offset, name] : receiver_shape.inferred_fields) {
      if (offset > typed_user_source.size() || !debug_path_identifier(name)) {
        error = "invalid inferred field location";
        return false;
      }
      typed_user_source.insert(offset, "\"" + name + "\", ");
    }
    const std::string inference_define =
        "#define CLANG_TRACER_FIELD_INFERENCE 1";
    const auto inference_pos = typed_user_source.find(inference_define);
    if (inference_pos != std::string::npos)
      typed_user_source.erase(inference_pos, inference_define.size());
    shape.class_mode = receiver_shape.class_mode;
    shape.has_pre = receiver_shape.has_pre;
    shape.has_post = receiver_shape.has_post;
    bridge = receiver_shape.bridge;
    const std::string generated =
        make_wrapper(p.request, reinterpret_cast<std::uintptr_t>(p.trampoline),
                     shape.has_pre, shape.has_post, &specification);
    complete_source = typed_user_source + "\n" + bridge + "\n" + generated;
    import_error.clear();
    ReceiverCallbackShape final_shape;
    final_shape.argument_names = p.argument_names;
    auto final_action = std::make_unique<ReceiverImportAction>(
        p.receiver_ast->getASTContext(), p.receiver_ast->getFileManager(),
        receiver_record, final_shape, &specification.receiver(),
        receiver_record, &import_error, &p.debug_records);
    const bool complete_syntax_ok = clang::tooling::runToolOnCodeWithArgs(
        std::move(final_action), complete_source, syntax_args,
        "clang-tracer-callback.cc", "clang-tracer",
        std::make_shared<clang::PCHContainerOperations>());
    if (!complete_syntax_ok || !final_shape.valid || !import_error.empty()) {
      error = import_error.empty() ? final_shape.error : import_error;
      if (error.empty()) {
        error = "typed receiver callback or wrapper failed Clang preflight";
      }
      return false;
    }
  } else {
    // The callback and generated wrapper compiled without the receiver alias;
    // preserve legacy callbacks and avoid importing unrelated target bodies.
    p.receiver_ast.reset();
  }
  p.class_callback = shape.class_mode;
  p.has_pre = shape.has_pre;
  p.has_post = shape.has_post;
  auto *ci = p.interpreter->getCompilerInstance();
  if (!ci) {
    error = "Clang Interpreter has no CompilerInstance";
    return false;
  }
  DiagnosticCaptureScope capture(ci->getDiagnostics());
  auto parsed = p.interpreter->ParseAndExecute(complete_source);
  if (parsed) {
    error = capture.message();
    if (error.empty()) {
      error = llvm::toString(std::move(parsed));
    } else {
      llvm::consumeError(std::move(parsed));
    }
    return false;
  }
  auto cb = p.interpreter->getSymbolAddress("tracer_user_dispatch");
  if (!cb) {
    error = llvm::toString(cb.takeError());
    return false;
  }
  p.request.callback_address = address_value(*cb);
  p.callback = reinterpret_cast<long long (*)(const tracer::TraceContext *)>(
      static_cast<std::uintptr_t>(p.request.callback_address));
  auto wrapper = p.interpreter->getSymbolAddress("clangtracer_wrapper");
  if (!wrapper) {
    error = llvm::toString(wrapper.takeError());
    return false;
  }
  p.wrapper = reinterpret_cast<void *>(
      static_cast<std::uintptr_t>(address_value(*wrapper)));
  p.request.wrapper_address = reinterpret_cast<std::uintptr_t>(p.wrapper);
  p.receiver_ast.reset(); // Import, validation, and JIT parsing are complete.
  return true;
}

clangtracer::v1::Value to_proto_value(const tracer::TraceValue &v) {
  clangtracer::v1::Value out;
  out.set_type(static_cast<clangtracer::v1::ValueType>(v.type));
  switch (v.type) {
  case tracer::ValueType::Int32:
  case tracer::ValueType::Int64:
    out.set_signed_value(v.signed_value);
    break;
  case tracer::ValueType::UInt64:
  case tracer::ValueType::Object:
  case tracer::ValueType::Pointer:
    out.set_unsigned_value(v.unsigned_value);
    break;
  case tracer::ValueType::Float32:
  case tracer::ValueType::Float64:
    out.set_real_value(v.real_value);
    break;
  default:
    break;
  }
  return out;
}

extern "C" void tracer_probe_enter(unsigned long long id) noexcept {
  if (auto *p = find_probe(id)) {
    p->in_flight.fetch_add(1, std::memory_order_seq_cst);
  }
}

extern "C" void tracer_probe_leave(unsigned long long id) noexcept {
  if (auto *p = find_probe(id)) {
    p->in_flight.fetch_sub(1, std::memory_order_seq_cst);
  }
}

extern "C" unsigned long long
tracer_next_call_id(unsigned long long id) noexcept {
  if (auto *p = find_probe(id)) {
    return p->call_ids.fetch_add(1, std::memory_order_relaxed) + 1;
  }
  return 0;
}

extern "C" long long
tracer_callback_dispatch(unsigned long long id,
                         const tracer::TraceContext *ctx) noexcept {
  auto *p = find_probe(id);
  if (!p || !ctx || !p->active.load(std::memory_order_seq_cst) ||
      !p->callback_enabled.load(std::memory_order_relaxed) || callback_depth) {
    return 0;
  }
  const int saved_errno = errno;
  ++callback_depth;
  long long value = 0;
  bool succeeded = false;
  tracer::TraceContext invocation = *ctx;
  invocation.named_count = 0;
  for (unsigned i = 0; i < invocation.argument_count && i < tracer::kMaxArguments; ++i)
    invocation.argument_names[i] = i < p->argument_names.size()
                                     ? p->argument_names[i].c_str() : nullptr;
  try {
    value = p->callback(&invocation);

    succeeded = true;
  } catch (...) {
    p->callback_enabled.store(false, std::memory_order_relaxed);
    p->callback_errors.fetch_add(1, std::memory_order_relaxed);
  }
  --callback_depth;
  if (succeeded) {
    tracer_event_dispatch(id, &invocation, value);
  }
  errno = saved_errno;
  return value;
}

extern "C" void tracer_capture_stack(const tracer::TraceContext *ctx,
                                      unsigned depth, bool application_only) noexcept {
  if (!ctx)
    return;
  const int saved_errno = errno;
  ++callback_depth;
  tracer::capture_native_stack(*ctx, depth, application_only);
  --callback_depth;
  errno = saved_errno;
}

extern "C" void tracer_capture_entry_stack(const tracer::TraceContext *ctx,
                                            unsigned depth) noexcept {
  if (!callback_depth)
    tracer_capture_stack(ctx, depth, ctx->stack_application_only);
}

extern "C" void tracer_event_dispatch(unsigned long long id,
                                      const tracer::TraceContext *ctx,
                                      long long cb) noexcept {
  const int saved_errno = errno;
  auto *p = find_probe(id);
  if (callback_depth || !p || !ctx ||
      !p->active.load(std::memory_order_seq_cst) || p->delivery.socket_fd < 0) {
    errno = saved_errno;
    return;
  }
  ++callback_depth;
  const auto sequence = p->sequence.fetch_add(1, std::memory_order_relaxed) + 1;
  auto &transport = agent.load(std::memory_order_acquire)->transport;
  auto *packet = transport.try_acquire(p->delivery);
  if (!packet) {
    --callback_depth;
    errno = saved_errno;
    return;
  }
  bool valid = false;
  try {
    clangtracer::v1::TraceEvent event;
    event.set_protocol_version(tracer::kProtocolVersion);
    event.set_session_id(p->session_id);
    event.set_probe_id(id);
    event.set_phase(ctx->phase == tracer::TracePhase::Pre
                        ? clangtracer::v1::PRE
                        : clangtracer::v1::POST);
    event.set_call_id(ctx->call_id);
    event.set_pid(static_cast<std::uint32_t>(::getpid()));
    event.set_tid(static_cast<std::uint32_t>(::syscall(SYS_gettid)));
    event.set_probe_sequence(sequence);
    timespec now{};
    if (::clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
      event.set_monotonic_time_ns(
          static_cast<std::uint64_t>(now.tv_sec) * 1000000000ULL + now.tv_nsec);
    }
    event.set_object_address(ctx->object_address);
    for (std::uint32_t i = 0;
         i < ctx->argument_count && i < tracer::kMaxArguments; ++i) {
      *event.add_arguments() = to_proto_value(ctx->arguments[i]);
    }
    if (ctx->named_count > 16) {
      throw std::invalid_argument("invalid named value count");
    }
    for (std::uint32_t i = 0; i < ctx->named_count; ++i) {
      const auto &value = ctx->named_values[i];
      const auto length = ::strnlen(value.name, sizeof(value.name));
      if (length == 0 || length == sizeof(value.name) ||
          (value.kind != 1 && value.kind != 2)) {
        throw std::invalid_argument("invalid named value");
      }
      auto *named = value.kind == 1 ? event.add_named_arguments()
                                    : event.add_object_fields();
      named->set_name(value.name, length);
      named->set_argument_index(value.argument_index);
      *named->mutable_value() = to_proto_value(value.value);
    }
    for (unsigned i = 0;
         i < p->argument_names.size() && i < ctx->argument_count; ++i) {
      bool captured = false;
      for (const auto &value : event.named_arguments())
        if (value.argument_index() == i)
          captured = true;
      if (!captured) {
        auto *named = event.add_named_arguments();
        named->set_name(p->argument_names[i]);
        named->set_argument_index(i);
        *named->mutable_value() = to_proto_value(ctx->arguments[i]);
      }
    }
    event.set_callback_value(cb);
    if (ctx->stack.status != tracer::StackStatus::Disabled)
      tracer::serialize_native_stack(ctx->stack, *event.mutable_stack());
    if (ctx->phase == tracer::TracePhase::Post) {
      *event.mutable_result() = to_proto_value(ctx->result);
    }
    event.set_agent_dropped_events(
        p->delivery.dropped.load(std::memory_order_relaxed));
    const auto size = event.ByteSizeLong();
    if (size <= tracer::EventTransport::kMaxEventBytes) {
      packet->bytes.resize(size);
      valid =
          event.SerializeToArray(packet->bytes.data(), static_cast<int>(size));
    }
    if (!valid)
      p->delivery.dropped.fetch_add(1, std::memory_order_relaxed);
  } catch (...) {
    p->delivery.dropped.fetch_add(1, std::memory_order_relaxed);
  }
  // Publish even after an exception, so no abandoned reservation stalls FIFO.
  transport.publish(*packet, valid);
  --callback_depth;
  errno = saved_errno;
}

bool validate_request(const tracer::Request &r, std::string &error) {
  if (r.version != tracer::kBootstrapVersion) {
    error = "unsupported request protocol";
    return false;
  }
  if (!r.probe_id) {
    error = "probe_id must be nonzero";
    return false;
  }
  if (r.kind != tracer::CallableKind::Function &&
      r.kind != tracer::CallableKind::Method) {
    error = "unsupported callable kind";
    return false;
  }
  if (r.argument_count > tracer::kMaxArguments) {
    error = "at most eight explicit arguments are supported";
    return false;
  }
  if (!valid_type(r.result, true)) {
    error = "unsupported result type";
    return false;
  }
  for (unsigned i = 0; i < r.argument_count; ++i) {
    if (!valid_type(r.arguments[i], false)) {
      error = "unsupported argument type";
      return false;
    }
  }
  if (!std::memchr(r.session_id, '\0', sizeof(r.session_id)) ||
      !r.session_id[0] || !std::memchr(r.symbol, '\0', sizeof(r.symbol)) ||
      !r.symbol[0] ||
      !std::memchr(r.source_path, '\0', sizeof(r.source_path)) ||
      !r.source_path[0] ||
      !std::memchr(r.event_socket, '\0', sizeof(r.event_socket)) ||
      !r.event_socket[0]) {
    error = "session, symbol, source path and event socket are required";
    return false;
  }
  return true;
}
} // namespace

extern "C" int tracer_agent_start(tracer::Request *request) {
  try {
    if (!request || request->version != tracer::kBootstrapVersion) {
      return -1;
    }
    request->error[0] = '\0';
    std::string error;
    if (!validate_request(*request, error)) {
      set_error(request, error.c_str());
      return -1;
    }
    std::lock_guard<std::mutex> lock(agent_mutex);
    Agent *current = agent.load(std::memory_order_acquire);
    if (!current) {
      // Keep the agent alive until process exit: a late wrapper entrant may
      // still reference a logically detached probe.
      current = std::make_unique<Agent>().release();
      agent.store(current, std::memory_order_release);
    }
    Agent &a = *current;
    if (a.pid && a.pid != ::getpid()) {
      set_error(request, "agent session pid mismatch");
      return -1;
    }
    if (find_probe(request->probe_id)) {
      set_error(request, "probe_id has already been used in this process");
      return -1;
    }
    if (a.retained >= tracer::kMaxProbes) {
      set_error(request, "the process has reached its 32 retained-probe limit");
      return -1;
    }
    auto p = std::make_unique<Probe>();
    p->request = *request;
    clangtracer::v1::ProbeSpec specification;
    if (!read_probe_spec(p->request, specification, error)) {
      set_error(request, error.c_str());
      return -1;
    }
    // Named tracing uses the target's own independently re-read DWARF and ELF
    // mappings. No dynamic symbol lookup is involved, including for local and
    // hidden functions. Raw-symbol requests retain their explicit dlsym path.
    void *entry = specification.has_debug_info()
                      ? reinterpret_cast<void *>(validate_debug_entry(specification))
                      : ::dlsym(RTLD_DEFAULT, request->symbol);
    if (!entry) {
      set_error(request, "target symbol was not found in the process");
      return -1;
    }
    p->request.original_entry = reinterpret_cast<std::uintptr_t>(entry);
    int prot = 0;
    std::uintptr_t map_lo = 0, map_hi = 0;
    if (!mapping(entry, map_lo, map_hi, prot) || (prot & PROT_EXEC) == 0) {
      set_error(request, "target symbol is not in an executable mapping");
      return -1;
    }
    for (std::size_t i = 0; i < a.retained; ++i) {
      auto *other = a.probes[i].get();
      if (other && other->active.load() &&
          other->request.original_entry == p->request.original_entry) {
        set_error(request, "target symbol address is already hooked");
        return -1;
      }
    }
    if (!decode_patch(static_cast<const std::uint8_t *>(entry), p->request,
                      error)) {
      set_error(request, error.c_str());
      return -1;
    }
    if (specification.has_debug_info() &&
        p->request.patch_size > specification.debug_info().entry_size()) {
      set_error(request, "safe patch window exceeds the DWARF function range");
      return -1;
    }
    for (std::size_t i = 0; i < a.retained; ++i) {
      auto *other = a.probes[i].get();
      if (!other || !other->active.load()) {
        continue;
      }
      const auto x = other->request.original_entry,
                 y = p->request.original_entry;
      const auto xe = x + other->request.patch_size,
                 ye = y + p->request.patch_size;
      if (x < ye && y < xe) {
        set_error(request, "target patch overlaps an already hooked function");
        return -1;
      }
    }
    long page_size = ::sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
      set_error(request, "could not determine page size");
      return -1;
    }
    p->mapping_size = static_cast<std::size_t>(page_size);
    p->trampoline = ::mmap(nullptr, p->mapping_size, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p->trampoline == MAP_FAILED) {
      set_error(request, "could not allocate trampoline");
      return -1;
    }
    std::memcpy(p->trampoline, p->request.original, p->request.patch_size);
    add_jump(static_cast<std::uint8_t *>(p->trampoline) + p->request.patch_size,
             static_cast<std::uint8_t *>(entry) + p->request.patch_size);
    if (::mprotect(p->trampoline, p->mapping_size, PROT_READ | PROT_EXEC)) {
      set_error(request, "could not make trampoline executable");
      return -1;
    }
    if (!compile_probe(*p, specification, error)) {
      set_error(request, error.c_str());
      return -1;
    }
    p->delivery.socket_fd =
        ::socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (p->delivery.socket_fd < 0) {
      set_error(request, "could not create event socket");
      return -1;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (std::strlen(request->event_socket) >= sizeof(addr.sun_path)) {
      set_error(request, "event socket path is too long");
      return -1;
    }
    std::strncpy(addr.sun_path, request->event_socket,
                 sizeof(addr.sun_path) - 1);
    if (::connect(p->delivery.socket_fd, reinterpret_cast<sockaddr *>(&addr),
                  sizeof(addr))) {
      set_error(request, "could not connect event socket");
      return -1;
    }
    p->session_id = request->session_id;
    a.pid = ::getpid();
    const auto slot = a.retained;
    std::array<std::uint8_t, tracer::kMaxPatchBytes> patch{};
    add_jump(patch.data(), p->wrapper);
    std::fill(patch.begin() + kJumpBytes, patch.begin() + p->request.patch_size,
              0x90);
    std::memcpy(p->request.installed, patch.data(), p->request.patch_size);
    a.transport.activate();
    p->transport_active = true;
    p->active.store(true, std::memory_order_seq_cst);
    a.probes[slot] = std::move(p);
    Probe *installed_probe = a.probes[slot].get();
    a.probe_slots[slot].store(installed_probe, std::memory_order_release);
    ++a.retained;
    *request = installed_probe->request;
    return 0;
  } catch (const std::exception &e) {
    set_error(request, e.what());
    return -1;
  } catch (...) {
    set_error(request, "unexpected exception while adding probe");
    return -1;
  }
}

extern "C" int tracer_agent_stop(tracer::Request *request) {
  try {
    if (!request || request->version != tracer::kBootstrapVersion) {
      return -1;
    }
    std::lock_guard<std::mutex> lock(agent_mutex);
    auto *p = find_probe(request->probe_id);
    if (!p) {
      set_error(request, "probe_id is unknown");
      return -1;
    }
    if (std::strncmp(request->session_id, p->request.session_id,
                     sizeof(request->session_id)) != 0) {
      set_error(request, "probe session id does not match");
      return -1;
    }
    void *entry = reinterpret_cast<void *>(p->request.original_entry);
    std::uintptr_t lo = 0, hi = 0;
    int prot = 0;
    if (!mapping(entry, lo, hi, prot) ||
        reinterpret_cast<std::uintptr_t>(entry) + p->request.patch_size > hi ||
        !(prot & PROT_EXEC)) {
      set_error(request, "remove requires the original executable mapping");
      return -2;
    }
    if (std::memcmp(entry, p->request.original, p->request.patch_size) != 0) {
      set_error(request, "remove requires the caller to restore original "
                         "bytes first");
      return -2;
    }
    p->active.exchange(false, std::memory_order_seq_cst);
    tracer::EventTransport::begin_drain(p->delivery);
    if (p->in_flight.load(std::memory_order_seq_cst) ||
        p->delivery.pending.load(std::memory_order_acquire)) {
      p->request.event_drops =
          p->delivery.dropped.load(std::memory_order_relaxed);
      p->request.callback_errors =
          p->callback_errors.load(std::memory_order_relaxed);
      *request = p->request;
      set_error(request, "probe callbacks or queued events are in flight; "
                         "remove can be retried");
      return 1;
    }
    if (p->transport_active) {
      agent.load(std::memory_order_acquire)->transport.deactivate();
      p->transport_active = false;
    }
    p->request.events_sent = p->delivery.sent.load(std::memory_order_relaxed);
    p->request.event_drops =
        p->delivery.dropped.load(std::memory_order_relaxed);
    p->request.callback_errors =
        p->callback_errors.load(std::memory_order_relaxed);
    *request = p->request;
    return 0;
  } catch (const std::exception &e) {
    set_error(request, e.what());
    return -1;
  } catch (...) {
    set_error(request, "unexpected exception while removing probe");
    return -1;
  }
}

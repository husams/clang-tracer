#include "arm_snapshot_codec.hpp"

#include "context_paths.hpp"
#include "prepared_contract.hpp"
#include "prepared_header_search.hpp"
#include "tracer/compiler_options.hpp"
#include "tracer/protocol.hpp"
#include "tracer_runtime_config.hpp"
#include "value_snapshot_codegen.hpp"
#include "value_snapshot_storage.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/Basic/TargetInfo.h>
#include <llvm/Config/llvm-config.h>
#if LLVM_VERSION_MAJOR >= 22
#include <clang/Interpreter/IncrementalExecutor.h>
#endif
#include <clang/Interpreter/Interpreter.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/TargetSelect.h>

#include <mutex>
#include <stdexcept>

namespace tracer::arm_snapshot {
namespace {
using Snapshot = clangtracer::context::v1::ValueSnapshot;

bool copy_snapshot(const ValueSnapshot &source, Snapshot &result, bool members) {
  result.set_status(static_cast<Snapshot::Status>(source.status));
  if (source.cpp_type)
    result.set_cpp_type(source.cpp_type);
  if (source.detail)
    result.set_detail(source.detail);
  if (source.status != ValueSnapshot::Complete)
    return true;
  if (!source.descriptor || !source.root_type || source.descriptor_size > 4096 ||
      source.payload_size > 4096 || (!source.payload && source.payload_size) ||
      source.container_role_count > 128 || source.record_type_count > 128 ||
      (source.member_count && (!members || !source.members || source.member_count > kMaxSnapshotMembers)))
    return false;
  result.set_root_type(source.root_type);
  result.set_descriptor_set(source.descriptor, source.descriptor_size);
  if (source.payload_size)
    result.set_payload(source.payload, source.payload_size);
  for (std::size_t i = 0; i != source.container_role_count; ++i)
    result.add_container_roles(source.container_roles[i]);
  for (std::size_t i = 0; i != source.record_type_count; ++i)
    result.add_record_types(source.record_types[i]);
  for (std::size_t i = 0; i != source.member_count; ++i) {
    const auto &member = source.members[i];
    if (!member.name || !member.snapshot)
      continue;
    auto &child = *result.add_members();
    child.set_name(member.name);
    if (!copy_snapshot(*member.snapshot, *child.mutable_snapshot(), false))
      return false;
  }
  return true;
}

bool encode_snapshot(const ValueSnapshot &source, std::string &bytes) noexcept {
  try {
    Snapshot result;
    return copy_snapshot(source, result, true) && result.ByteSizeLong() <= 65536 &&
        result.SerializeToString(&bytes);
  } catch (...) {
    return false;
  }
}
}

Codec::Codec() = default;
Codec::~Codec() = default;
Codec::Codec(Codec &&) noexcept = default;
Codec &Codec::operator=(Codec &&) noexcept = default;

Codec compile(const clangtracer::v1::ProbeSpec &spec) {
  static std::once_flag target;
  std::call_once(target, [] {
    if (llvm::InitializeNativeTarget() || llvm::InitializeNativeTargetAsmPrinter())
      throw std::runtime_error("cannot initialize the native snapshot compiler");
  });
  const auto &method = spec.prepared();
  const auto root = context_header_directory();
  auto options = prepared::toolchain_arguments(method,
      {runtime::jit_flags.begin(), runtime::jit_flags.end()});
  options.insert(options.end(), {"-std=c++23", "-fPIC", "-O1", "-I" + root,
      "-I" + root + "/../include", "-resource-dir=" TRACER_CLANG_RESOURCE_DIR});
  options.insert(options.end(), method.adapter_abi_options().begin(), method.adapter_abi_options().end());
  const auto arguments = clang_driver_arguments(options);
  const auto &types = spec.debug_info();
  std::string source = "#include <string>\n#include <tracer/protocol.hpp>\n" + prepared::adapter_source(method) +
      "\nnamespace tracer { using ResolvedTarget = " + types.record_name() +
      "; using ResolvedSignature = " + types.result_cpp_type() + "(";
  for (int index = 0; index < types.arguments_size(); ++index) {
    if (index)
      source += ",";
    source += types.arguments(index).cpp_type();
  }
  source += "); }\n";
  auto ast = clang::tooling::buildASTFromCodeWithArgs(source, arguments, "snapshot-adapter.cc");
  if (!ast || ast->getDiagnostics().hasErrorOccurred() ||
      !ast->getASTContext().getTargetInfo().getTriple().isAArch64())
    throw std::runtime_error("cannot compile the prepared AArch64 receiver adapter");
  prepared::check_toolchain_headers(ast->getPreprocessor().getHeaderSearchInfo(), method);
  auto snapshots = make_value_snapshot_code(ast->getASTContext(), spec);
  if (!snapshots.error.empty() || snapshots.receiver.empty())
    throw std::runtime_error(snapshots.error.empty() ? "prepared adapter has no receiver codec" : snapshots.error);
  // Keep support symbols private to this injected image. Calling their
  // addresses avoids symbol interposition with a target's libraries.
  const auto bind = [&](std::string_view declaration, const char *name, std::string_view arguments, auto pointer) {
    source += "\nextern \"C\" " + std::string(declaration) + " { return reinterpret_cast<decltype(&" +
        name + ")>(" + std::to_string(reinterpret_cast<std::uintptr_t>(pointer)) + "ULL)(" +
        std::string(arguments) + "); }\n";
  };
  bind("std::size_t tracer_snapshot_mark() noexcept", "tracer_snapshot_mark", "", &tracer_snapshot_mark);
  bind("void tracer_snapshot_reset(std::size_t mark) noexcept", "tracer_snapshot_reset", "mark", &tracer_snapshot_reset);
  bind("std::byte* tracer_snapshot_storage(std::size_t* size) noexcept", "tracer_snapshot_storage", "size", &tracer_snapshot_storage);
  bind("void tracer_snapshot_commit(std::size_t mark, std::size_t size) noexcept", "tracer_snapshot_commit", "mark, size", &tracer_snapshot_commit);
  bind("bool tracer_snapshot_readable(const void* ptr, std::size_t size, std::size_t align) noexcept",
       "tracer_snapshot_readable", "ptr, size, align", &tracer_snapshot_readable);
  bind("bool tracer_snapshot_read_rtti(const void* ptr, const void** object, const char** name) noexcept",
       "tracer_snapshot_read_rtti", "ptr, object, name", &tracer_snapshot_read_rtti);
  bind("bool tracer_snapshot_read_rtti_provider(const void* ptr, const void** object, const char** name, const void** type, const void** table) noexcept",
       "tracer_snapshot_read_rtti_provider", "ptr, object, name, type, table", &tracer_snapshot_read_rtti_provider);
  bind("bool tracer_snapshot_rtti_matches(const char* name, const char* expected, std::size_t len, const void* object, const void* base, std::size_t size, std::size_t align) noexcept",
       "tracer_snapshot_rtti_matches", "name, expected, len, object, base, size, align", &tracer_snapshot_rtti_matches);
  source += snapshots.declarations;
  source += R"cpp(
extern "C" bool ct_arm_snapshot_sample(const void *receiver, std::string &output) noexcept {
  try {
    if (!tracer_snapshot_readable(receiver, sizeof(tracer::ResolvedTarget), alignof(tracer::ResolvedTarget)))
      return false;
    tracer_snapshot_detail::Scope storage;
    tracer::TraceContext ctx{};
    ctx.object_address = reinterpret_cast<std::uintptr_t>(receiver);
)cpp";
  source += snapshots.receiver;
  source += R"cpp(
    return ctx.receiver_snapshot && reinterpret_cast<bool (*)(const tracer::ValueSnapshot &, std::string &) noexcept>(
)cpp";
  source += std::to_string(reinterpret_cast<std::uintptr_t>(&encode_snapshot));
  source += R"cpp(ULL)(*ctx.receiver_snapshot, output);
  } catch (...) { return false; }
}
)cpp";
  std::vector<const char *> argv;
  for (const auto &arg : arguments)
    argv.push_back(arg.c_str());
  clang::IncrementalCompilerBuilder builder;
  builder.SetCompilerArgs(argv);
  auto compiler = builder.CreateCpp();
  if (!compiler)
    throw std::runtime_error(llvm::toString(compiler.takeError()));
  auto jit_builder = std::make_unique<llvm::orc::LLJITBuilder>();
#if LLVM_VERSION_MAJOR >= 22
  auto executor = std::make_unique<clang::IncrementalExecutorBuilder>();
  executor->JITBuilder = std::move(jit_builder);
  auto interpreter = clang::Interpreter::create(std::move(*compiler), std::move(executor));
#else
  auto interpreter = clang::Interpreter::create(std::move(*compiler), std::move(jit_builder));
#endif
  if (!interpreter)
    throw std::runtime_error(llvm::toString(interpreter.takeError()));
  auto engine = (*interpreter)->getExecutionEngine();
  if (!engine)
    throw std::runtime_error(llvm::toString(engine.takeError()));
  if (auto error = (*interpreter)->ParseAndExecute(source))
    throw std::runtime_error(llvm::toString(std::move(error)));
  auto symbol = (*interpreter)->getSymbolAddress("ct_arm_snapshot_sample");
  if (!symbol)
    throw std::runtime_error(llvm::toString(symbol.takeError()));
  Codec result;
  result.sample = symbol->toPtr<Codec::Sample>();
  result.compiler = std::move(*interpreter);
  return result;
}
}

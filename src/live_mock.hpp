#pragma once

#include <optional>
#include <span>

#include "live_update.hpp"

namespace clang {
class ASTContext;
}

namespace clangtracer::v1 {
class LiveUpdateRequest;
}

namespace llvm {
class Function;
}

namespace tracer::live {

struct MockBindingOption {
  std::string alias;
  std::string selector;
};

struct MockCommand {
  std::string selector;
  SourceFile source;
  std::string callback;
  std::optional<SourceFile> fixture;
  std::vector<MockBindingOption> bindings;
  bool recursive_original = false;
};

// Tokens after `mock`. The REPL has already decoded quoted arguments.
MockCommand parse_live_mock(std::span<const std::string> arguments);

// Read source and fixture bytes before sending either to the stage operation.
MockCommand snapshot_live_mock(std::span<const std::string> arguments);

// The REPL calls the first overload before RPC. The server calls the second
// to validate metadata and retrieve the exact transferred bytes.
void snapshot_live_mock(clangtracer::v1::LiveUpdateRequest& request);
MockCommand read_live_mock(const clangtracer::v1::LiveUpdateRequest& request);

// Source types and the emitted native definition come from the same
// build-bound compiler input. Resolve the entry separately in the mapped ELF
// and keep that identity, the baseline trampoline and all imports pinned by
// the shared transaction. An ordinary import uses address, including for a
// concrete member bridge. It does not use the trampoline.
struct MockCallable {
  std::string selector;
  std::string result;
  std::vector<std::string> parameters;
  std::string receiver; // cv-qualified type, empty for free/static functions
  bool is_noexcept = false;
  std::uintptr_t address = 0;
  const llvm::Function* native_definition = nullptr;
};

// Run this on the parsed, build-bound native translation unit. Only exact
// selected signatures are collected. Header paths must also be present in
// the caller's verified source inventory before this declaration is admitted.
struct MockDeclaration {
  std::string linkage;
  MockCallable callable;
  std::vector<std::string> headers;
};

std::string normalize_mock_selector(std::string spelling);

std::map<std::string, MockDeclaration>
describe_mock_callables(clang::ASTContext& ast, std::span<const std::string> selectors);

struct MockCompileContext {
  MockCallable target;
  std::uintptr_t original = 0;
  // In the exact same order as MockCommand::bindings.
  std::vector<MockCallable> bindings;
  // Build-bound application declarations and other snapshotted inputs.
  std::vector<SourceFile> files;
  std::string source_root;
  std::vector<std::string> declaration_headers;
  std::vector<std::string> compiler_arguments;
  std::string directory;
};

inline constexpr std::string_view mock_entry_symbol = "clangtracer_live_mock_entry_v1";

// Compiles the generated source to check ABI without publishing a revision.
// The common agent path materializes the returned request with compile_in_target.
JitRequest build_live_mock(const MockCommand& command, const MockCompileContext& context);

} // namespace tracer::live

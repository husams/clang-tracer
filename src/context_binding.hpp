#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Support/Error.h>

namespace clang {
class ASTUnit;
class ASTContext;
class Preprocessor;
} // namespace clang

namespace tracer {
class ContextStdlibImage;

namespace context_support {
struct UnwindBases;
}

// Generated from both compiler instances, never from callback annotations.
// Equal names alone do not authorize interpreting an owner's objects.
struct ContextTypeLayout {
  std::string name;
  std::string definition;
  std::uint64_t bytes = 0;
  std::uint64_t alignment = 0;
  bool operator==(const ContextTypeLayout &) const = default;
};

struct ContextCompilationContract {
  std::string target;
  std::string data_layout;
  std::string compiler;
  std::string language;
  std::string standard_library;
  std::vector<ContextTypeLayout> types;

  struct Bridge {
    std::string symbol;
    std::string source_name;
    std::string type;
    std::vector<ContextTypeLayout> records;
    bool operator==(const Bridge &) const = default;
  };

  std::vector<Bridge> bridges;
  // Actual emitted definitions whose spelling locations are in the native
  // toolchain's physical standard headers, never a source namespace assertion.
  std::vector<std::string> standard_definitions;

  // Different modules declare different bridges. Each import is compared with
  // its actual owner export separately, including pointer/reference pointees.
  bool operator==(const ContextCompilationContract &other) const {
    return target == other.target && data_layout == other.data_layout &&
           compiler == other.compiler && language == other.language &&
           standard_library == other.standard_library && types == other.types;
  }
};

llvm::Expected<std::vector<std::string>>
context_bridge_symbols(const ContextCompilationContract &,
                       const std::vector<std::string> &source_names);

// Includes recursively reached field/base records and their actual Clang
// layouts. Returns an error for incomplete/dependent/unsupported declarations.
llvm::Expected<ContextCompilationContract>
context_compilation_contract(clang::ASTUnit &,
                             const std::vector<std::string> &state_types);
llvm::Expected<ContextCompilationContract>
context_compilation_contract(clang::ASTContext &, clang::Preprocessor &,
                             const std::vector<std::string> &state_types,
                             const std::vector<std::string> &type_aliases = {},
                             std::string_view owned_input_marker = {});

struct CompiledContextModule {
  llvm::orc::ThreadSafeModule module;
  ContextCompilationContract contract;
};

// Setup-only, parse/codegen without executing any user initializer. Transfers
// the IR to an independent context so code retirement cannot outlive Clang's
// compiler context. Arguments are the engine's normalized compilation profile.
llvm::Expected<CompiledContextModule>
compile_context_module(const std::string &source,
                       const std::vector<std::string> &compiler_arguments,
                       const std::vector<std::string> &state_types);

class PrivateContextCode;

struct ContextNativeAddress {
  std::string source_name;
  std::uintptr_t address = 0;
};

class ContextNativeSymbol final {
public:
  // Only native engine code builds a catalog from its canonical declarations;
  // callback source never supplies the addresses or expected prototypes.
  static llvm::Expected<std::vector<ContextNativeSymbol>>
  catalog(const std::string &canonical_source,
          const std::vector<std::string> &compiler_arguments,
          const std::vector<ContextNativeAddress> &);

private:
  friend class PrivateContextCode;
  ContextCompilationContract profile_;
  ContextCompilationContract::Bridge bridge_;
  std::string signature_;
  std::uintptr_t address_ = 0;
};

// Native-only capability. Retaining this pins the exact code/unwind generation;
// it is never exposed in a callback's public C++ prelude or payload type.
class ContextSymbol final {
public:
  std::uintptr_t address() const noexcept { return address_; }

  std::uint64_t generation() const noexcept;

  const std::string &name() const noexcept { return name_; }

private:
  friend class PrivateContextCode;
  std::shared_ptr<const PrivateContextCode> owner_;
  std::string name_;
  std::string signature_;
  ContextCompilationContract::Bridge bridge_;
  std::uintptr_t address_ = 0;
};

// A loader accepts only a closed LLVM module plus explicit capabilities from
// one exact owner. No process search generator or global AddSymbol lookup is
// installed. Materialization is eager: backend-introduced dependencies must
// resolve before publication as well as dependencies observed in LLVM IR.
class PrivateContextCode final
    : public std::enable_shared_from_this<PrivateContextCode> {
public:
  enum class Role { support, owner, callback };
  static llvm::Expected<std::shared_ptr<const PrivateContextCode>>
  load(llvm::orc::ThreadSafeModule, ContextCompilationContract,
       std::vector<std::string> exports,
       std::vector<ContextSymbol> imports = {}, Role = Role::owner,
       std::shared_ptr<const ContextStdlibImage> standard_library = {},
       std::vector<ContextNativeSymbol> native_imports = {});
  ~PrivateContextCode();
  PrivateContextCode(const PrivateContextCode &) = delete;
  PrivateContextCode &operator=(const PrivateContextCode &) = delete;

  llvm::Expected<ContextSymbol> symbol(const std::string &) const;
  std::uint64_t generation() const noexcept;
  const ContextCompilationContract &contract() const noexcept;
  const std::shared_ptr<const ContextStdlibImage> &
  standard_library() const noexcept;
  const void *find_fde(std::uintptr_t,
                       context_support::UnwindBases &) const noexcept;
  static const void *unwind_lookup(const void *, std::uintptr_t,
                                   context_support::UnwindBases &) noexcept;

private:
  struct Impl;
  explicit PrivateContextCode(std::unique_ptr<Impl>);
  std::unique_ptr<Impl> impl_;
};

} // namespace tracer

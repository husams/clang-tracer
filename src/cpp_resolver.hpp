#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "tracer.pb.h"

namespace tracer::client {

struct PreparedHeaderCommand;

struct ResolvedCallable {
  clangtracer::v1::ProbeSpec spec;
  std::string declaration;
  std::string source;
  bool requires_typed_adapter = false;
  // Typed-context routes declare their own adapter function. The selector names it for
  // verification, and the donor translation unit and driver mode go into the package.
  std::string adapter_selector;
  // The selector the package records when it differs from the one requested: the tagged
  // spelling of a callable that was requested without its ABI tags.
  std::string package_selector;
  std::string translation_unit;
  bool cl_driver = false;
  // Set for a pure virtual declaration resolved from headers: the concrete overrides those
  // headers define. Whether the declaration has a body is a question for the module.
  std::string pure_virtual_overrides;
};

// Supplied compiler-visible type definitions: headers and the compilation command that gives
// them their language options, target, include paths and standard-library selection.
struct TypeContext {
  std::vector<std::string> headers;
  std::string compilation_database;
  std::string translation_unit;
  std::vector<std::string> extra_arguments;
  clangtracer::v1::SnapshotLimits limits;
  bool no_automatic_members = false;
  bool receiver_storage = false;
};

// Everything known about one concrete function before its types are compiled. The function is
// selected by its ELF symbol (and so by identity), independently of any declaration in headers.
struct SymbolContract {
  std::string symbol;                // linkage name, the adapter's asm label
  std::string display;               // selector for diagnostics
  std::string signature;             // supplied C++ function type (explicit-signature route)
  // Indexed route: one type spelling per role instead of a single function type.
  std::optional<std::string> result_type;
  std::vector<std::string> argument_types;
  std::string encoded_parameters;    // demangled "(T, U)" of the symbol, when it has one
  std::string encoded_result;        // demangled return type, when the mangling encodes it
  std::string receiver;              // "", "void*", "const void*", ...
  std::string declarations;          // forward declarations emitted after the supplied headers
  // Record sizes recorded in debug information, to cross-check by-value roles.
  std::optional<std::uint64_t> result_size;
  std::vector<std::optional<std::uint64_t>> argument_sizes;
};

// Compiles the contract's types in the supplied context and returns the typed adapter. Every
// capture step is verified by recompiling the standalone adapter, so an incompatible context
// fails before a package is written.
ResolvedCallable resolve_symbol_in_type_context(const SymbolContract &, const TypeContext &);

// Tries each capture level from the fullest to the narrowest and reports why a fuller level
// was dropped. `resolve` receives (pointer_layouts, member_level).
ResolvedCallable resolve_with_capture_steps(
    bool no_automatic_members, const std::function<ResolvedCallable(bool, int)> &resolve);

ResolvedCallable resolve_header_callable(const std::string& selector,
                                         const clangtracer::v1::ProbeSpec& header_context,
                                         const PreparedHeaderCommand& command,
                                         bool pointer_layouts,
                                         int member_level = 3,
                                         const clangtracer::v1::SnapshotLimits &limits = {},
                                         bool receiver_storage = false);

void verify_prepared_adapter(const std::string &selector,
                             const clangtracer::v1::PreparedInjector &adapter,
                             const ResolvedCallable &original,
                             bool pointer_layouts);

// Resolves a C++ callable using the project's compilation database. The
// selector may include an explicit parameter list and member qualifiers.
// Throws std::runtime_error for missing, ambiguous, or unsupported callables.
ResolvedCallable resolve_cpp_callable(const std::string& name,
                                      const std::string& compilation_database,
                                      const std::vector<std::string>& sources);

std::vector<clangtracer::v1::ObjectWatchRequest::SourceWriter> discover_object_writers(
    const std::string &type, const std::string &field, const std::string &database,
    const std::vector<std::string> &sources);

} // namespace tracer::client

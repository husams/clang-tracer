#pragma once

#include <optional>
#include <string>
#include <vector>

namespace clang {
class CompilerInvocation;
}

namespace tracer::callback_pch {

enum class Mode { Syntax, Interpreter };

struct Artifact {
  std::string pch_path;
  std::string prefix_path;
  std::string key;
};

// Build a process-local PCH for the common standard-header prefix, or return
// the cached artifact for the same compiler environment and dependency set.
std::optional<Artifact> prepare(const clang::CompilerInvocation &base,
                                Mode mode);

// Configure an incremental interpreter before Interpreter::create().
void attach(clang::CompilerInvocation &invocation, const Artifact &artifact);

// Add the same PCH and guarded forced-include shim to a tooling invocation.
void append_syntax_args(std::vector<std::string> &arguments,
                        const Artifact &artifact);

// Disable an artifact after its consumer proves it cannot be loaded.
void disable(const Artifact &artifact) noexcept;

} // namespace tracer::callback_pch

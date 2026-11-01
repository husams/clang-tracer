#pragma once

#include <string>

#include <clang/Basic/SourceLocation.h>

namespace clang {
class ASTUnit;
class SourceManager;
}

namespace tracer {

// Validate the bounded exception and cleanup effects used by native support
// admission. Empty means the AST has a finite proof within the fixed reserve.
std::string validate_context_support_effects(clang::ASTUnit &);

// Returns true only for the engine's physical compiler/stdlib headers or its
// installed private headers; -isystem and namespace spelling do not confer
// trust.
bool context_source_is_trusted_support(const clang::SourceManager &,
                                       clang::SourceLocation);

} // namespace tracer

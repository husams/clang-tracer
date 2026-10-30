#pragma once
#include "cpp_resolver.hpp"
#include <cstdint>

namespace tracer::client {
// Uses only mapped ELF/DWARF data, never target source files or build
// databases.
ResolvedCallable resolve_debug_callable(std::uint32_t pid,
                                        const std::string &selector);
} // namespace tracer::client

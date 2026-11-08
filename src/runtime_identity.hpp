#pragma once

#include <string_view>

namespace tracer::runtime {
// Check the loaded ELF SONAME instead of a distribution-specific real filename.
bool has_soname(const void* address, std::string_view expected) noexcept;
} // namespace tracer::runtime

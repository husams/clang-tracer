#pragma once

#include <string>

#include "tracer.pb.h"

namespace tracer::client {
enum class HeaderCommandSelection { infer, first_in_header_directory };

// Resolves flags only. The donor implementation file is never parsed or opened.
void resolve_header_context(clangtracer::v1::ProbeSpec& spec, const std::string& database_path,
                            const std::string& translation_unit = {},
                            HeaderCommandSelection selection = HeaderCommandSelection::infer);
} // namespace tracer::client

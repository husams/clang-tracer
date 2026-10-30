#pragma once

#include <string>

#include "tracer.pb.h"

namespace tracer::client {
// Resolves flags only. The donor implementation file is never parsed or opened.
void resolve_header_context(clangtracer::v1::ProbeSpec& spec, const std::string& database_path);
} // namespace tracer::client

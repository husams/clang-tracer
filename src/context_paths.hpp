#pragma once
#include <string>
namespace tracer {
std::string context_header_directory();
// Compiler option that gives code built against the on-disk headers the same argument limit
// (and so the same TraceContext layout) as this build; see TRACER_MAX_ARGUMENTS.
std::string context_argument_limit_option();
std::string context_standard_library_path();
} // namespace tracer

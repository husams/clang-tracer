#pragma once

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
};

ResolvedCallable resolve_header_callable(const std::string& selector,
                                         const clangtracer::v1::ProbeSpec& header_context,
                                         const PreparedHeaderCommand& command,
                                         bool pointer_layouts, bool receiver_storage = false);

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

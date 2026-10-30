#pragma once

#include <string>
#include <vector>

#include "tracer.pb.h"

namespace tracer::client {

struct ResolvedCallable {
  clangtracer::v1::ProbeSpec spec;
  std::string declaration;
  std::string source;
};

// Resolves a C++ callable using the project's compilation database. The
// selector may include an explicit parameter list and member qualifiers.
// Throws std::runtime_error for missing, ambiguous, or unsupported callables.
ResolvedCallable resolve_cpp_callable(const std::string& name,
                                      const std::string& compilation_database,
                                      const std::vector<std::string>& sources);

} // namespace tracer::client

#pragma once

#include <cstdlib>
#include <string>
#include <string_view>

namespace tracer::detail {

// Resolve a mapped pathname to the canonical path visible in this mount
// namespace. Empty means the mapping has no currently resolvable pathname.
inline std::string canonical_mapped_path(std::string_view path) {
  if (path.empty() || path.front() != '/') {
    return {};
  }
  // A real filename can end in " (deleted)". Let resolution distinguish it
  // from procfs's marker for an unlinked file rather than rejecting the name.
  const std::string input(path);
  char *resolved = ::realpath(input.c_str(), nullptr);
  if (resolved == nullptr) {
    return {};
  }
  std::string result(resolved);
  std::free(resolved);
  return result;
}

} // namespace tracer::detail

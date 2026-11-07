#pragma once

#include "tracer.pb.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <string_view>

namespace tracer::prepared {

inline constexpr unsigned typed_version = 4;
inline constexpr unsigned max_replay_type_bytes = 64 * 1024;
inline constexpr std::array<std::string_view, 29> standard_headers{
    "array", "atomic", "bitset", "chrono", "complex", "cstddef", "cstdint",
    "deque", "forward_list", "functional", "initializer_list", "list", "map",
    "memory", "optional", "ostream", "queue", "set", "sstream", "stack", "string", "string_view",
    "tuple", "type_traits", "unordered_map", "unordered_set", "utility", "variant",
    "vector"};
inline bool standard_header(std::string_view header) {
  return std::find(standard_headers.begin(), standard_headers.end(), header) !=
             standard_headers.end();
}

inline bool abi_option(std::string_view option) {
  return option == "-std=c++17" || option == "-std=gnu++17" ||
         option == "-std=c++20" || option == "-std=gnu++20" ||
         option == "-std=c++23" || option == "-std=gnu++23" ||
         option == "-D_GLIBCXX_USE_CXX11_ABI=0" ||
         option == "-D_GLIBCXX_USE_CXX11_ABI=1" ||
         option == "-fshort-enums" || option == "-fshort-wchar";
}

inline std::string adapter_source(const clangtracer::v1::PreparedInjector &package) {
  const auto &declarations = package.adapter_declarations();
  if (declarations.empty() ||
      declarations.find_first_of("#\\\r") != std::string::npos ||
      declarations.find('\0') != std::string::npos ||
      declarations.find("%:") != std::string::npos)
    throw std::invalid_argument("invalid prepared adapter declarations");
  std::string source;
  for (const auto &header : package.adapter_standard_headers()) {
    if (!standard_header(header))
      throw std::invalid_argument("unsupported prepared adapter standard header");
    source += "#include <" + header + ">\n";
  }
  for (const auto &option : package.adapter_abi_options())
    if (!abi_option(option))
      throw std::invalid_argument("unsupported prepared adapter ABI option");
  return source + declarations;
}

} // namespace tracer::prepared

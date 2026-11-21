#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace tracer {

// An ABI tag annotates a name rather than naming another callable. The demangled form of a
// tagged function spells it `[abi:tag]`, so removing every annotation gives the structural
// key that compares a tagged selector with the same selector written without tags.
inline std::string strip_abi_tags(std::string_view value) {
  constexpr std::string_view open = "[abi:";
  std::string out;
  for (std::size_t at = 0; at < value.size();) {
    if (value.compare(at, open.size(), open) == 0) {
      const auto close = value.find(']', at + open.size());
      if (close != value.npos) {
        at = close + 1;
        continue;
      }
    }
    out += value[at++];
  }
  return out;
}

} // namespace tracer

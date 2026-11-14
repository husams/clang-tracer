#pragma once

#include <cstddef>
#include <string>

namespace tracer {

// Location of the generated prepared-adapter declarations inside a source
// buffer. Context admission uses it to admit the one generated asm label that
// names the probe's own target symbol; everything else stays user code.
struct PreparedAdapterRegion final {
  std::size_t offset = 0; // into the source buffer
  std::size_t size = 0;   // 0 means no adapter
  std::string symbol;     // exact ELF symbol the label may name
};

} // namespace tracer

#pragma once
#include "cpp_resolver.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tracer::client {
// Uses only mapped ELF/DWARF data, never target source files or build
// databases.
ResolvedCallable resolve_debug_callable(std::uint32_t pid,
                                        const std::string &selector);

struct DebugSymbol {
  std::string selector;
  std::string linkage_symbol;
  std::string module_path;
};

struct DebugSymbolPage {
  std::vector<DebugSymbol> symbols;
  std::string mapping_fingerprint;
  bool has_more = false;
  bool cancelled = false;
};

DebugSymbolPage list_debug_symbols(
    std::uint32_t pid, const std::string &query, const std::string &module,
    std::size_t page_size, const std::string &after_selector,
    const std::string &after_linkage_symbol, const std::string &after_module_path,
    const std::string &expected_mapping_fingerprint,
    const std::function<bool()> &is_cancelled);
} // namespace tracer::client

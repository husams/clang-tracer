#pragma once
#include "cpp_resolver.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <stdexcept>
#include <vector>

namespace tracer::client {
// Uses only mapped ELF/DWARF data, never target source files or build
// databases.
struct DebugLookupOptions {
  std::string module;
  std::uint64_t max_entries = 0; // Zero is unlimited.
  std::uint32_t max_depth = 0; // Zero is unlimited.
  std::function<bool()> is_cancelled = [] { return false; };
};

class DebugLookupLimit : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

std::vector<std::string> list_debug_modules(std::uint32_t pid);
ResolvedCallable resolve_debug_callable(
    std::uint32_t pid, const std::string &selector,
    const DebugLookupOptions &options = {});

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

// page_size == 0 returns all matches; positive values opt into pagination.
DebugSymbolPage list_debug_symbols(
    std::uint32_t pid, const std::string &query, const std::string &module,
    std::size_t page_size, const std::string &after_selector,
    const std::string &after_linkage_symbol, const std::string &after_module_path,
    const std::string &expected_mapping_fingerprint,
    const std::function<bool()> &is_cancelled);
} // namespace tracer::client

#pragma once
#include "cpp_resolver.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <stdexcept>
#include <vector>

namespace llvm {
class DWARFDie;
namespace object { class ObjectFile; }
}

namespace tracer::client {
// Bind a code range from an already opened executable to the live image.
// Requires a matching mapped build ID and returns the ASLR load bias.
std::uint64_t bind_live_executable(std::uint32_t pid, const std::string &path,
                                  const llvm::object::ObjectFile &object,
                                  std::uint64_t entry, std::uint64_t size,
                                  bool allow_main_without_build_id = false);
std::optional<std::string> find_matching_debug_file(
    const std::string &path, const llvm::object::ObjectFile &object);
// Read the verified ELF range and require an exact match with live text.
std::string verified_function_code(std::uint32_t pid,
    const clangtracer::v1::DebugInfo &info, std::uint64_t max_bytes,
    std::string_view expected_image_sha256 = {});
// Uses only mapped ELF/DWARF data, never target source files or build
// databases.
struct DebugLookupOptions {
  std::string module;
  std::uint64_t max_entries = 0; // Zero is unlimited.
  std::uint32_t max_depth = 0; // Zero is unlimited.
  std::function<bool()> is_cancelled = [] { return false; };
  bool require_override_abi = false;
  bool allow_virtual_entry = false; // Only for inspecting or translating a resolved body.
};

class DebugLookupLimit : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

std::vector<std::string> list_debug_modules(std::uint32_t pid);
clangtracer::v1::ListModulesResponse list_debug_module_mappings(
    std::uint32_t pid, const std::string &elf_path = {});
clangtracer::v1::PreviewBulkTraceResponse preview_bulk_trace(
    std::uint32_t pid, const clangtracer::v1::BulkTraceRules &rules,
    const std::function<bool()> &is_cancelled);
ResolvedCallable resolve_debug_callable(
    std::uint32_t pid, const std::string &selector,
    const DebugLookupOptions &options = {});

void verify_rebound_method_signature(llvm::DWARFDie function,
    const clangtracer::v1::PreparedInjector &prepared);

struct PreparedCodePatch {
  std::string_view original;
  std::string_view installed;
};

// The agent supplies its active patch, if any, when checking images without
// a mapped build ID. The returned views are borrowed for the duration of lookup.
ResolvedCallable bind_prepared_callable(
    std::uint32_t pid, const clangtracer::v1::PreparedInjector &prepared,
    const DebugLookupOptions &options = {},
    const std::function<PreparedCodePatch(std::uint64_t)> &live_patch = {});

struct DebugSymbol {
  std::string selector;
  std::string linkage_symbol;
  std::string module_path;
  std::uint64_t runtime_address = 0;
  std::uint64_t elf_address = 0;
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
    const std::function<bool()> &is_cancelled,
    bool include_addresses = false, std::uint64_t address = 0);
} // namespace tracer::client

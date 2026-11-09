#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace tracer_module {

struct ModuleSource {
  std::filesystem::path image;
  std::filesystem::path debug_file;
  std::string expected_build_id;
  static ModuleSource file(std::filesystem::path image,
                           std::filesystem::path debug_file = {});
};

struct ModuleIndexOptions {
  std::uint64_t max_entries = 0;
  std::uint32_t max_depth = 0;
  std::function<bool()> cancelled;
};

struct ModuleIdentity {
  std::string image, sha256, build_id, debug_file, debug_sha256;
  std::vector<std::string> diagnostics;
};

struct ModuleField {
  std::string name, type_id;
  std::optional<std::uint64_t> byte_offset, bit_offset, bit_size;
  bool base = false;
};

struct ModuleType {
  std::string id, name, kind, target_id, file;
  std::optional<std::uint64_t> size, alignment, count, encoding;
  std::uint64_t line = 0;
  bool declaration = false, namespace_scope = false;
  std::vector<ModuleField> fields;
};

struct ModuleParameter {
  std::string name, type_id;
};

struct ModuleSymbol {
  std::string id, name, selector, linkage, kind, version, type_id, file, diagnostic;
  std::uint64_t address = 0, size = 0, line = 0;
  bool exported = false, default_version = false, has_debug = false, member = false;
  std::vector<ModuleParameter> parameters;
};

struct ModuleQuery {
  std::string filter, page;
  std::size_t limit = 0;
};

struct SymbolQuery : ModuleQuery {
  bool exports = false;
  SymbolQuery &exported_only() { exports = true; return *this; }
};

template <class Entry> struct ModulePage {
  std::vector<Entry> entries;
  std::string next_page;
};

struct ModuleSelection {
  std::vector<std::string> symbols, types;
};

class ModuleIndex {
public:
  static ModuleIndex build(const ModuleSource &, const std::filesystem::path &output,
                           const ModuleIndexOptions & = {});
  static ModuleIndex open(const std::filesystem::path &);
  const std::filesystem::path &path() const { return path_; }
  const ModuleIdentity &identity() const { return identity_; }

  ModulePage<ModuleType> types(const ModuleQuery & = {}) const;
  ModulePage<ModuleSymbol> symbols(const SymbolQuery & = {}) const;
  ModuleType find_type(const std::string &name) const;
  ModuleSymbol find_symbol(const std::string &name) const;
  ModuleType get_type(const std::string &id) const;
  ModuleSymbol get_symbol(const std::string &id) const;

  void emit_headers(const ModuleSelection &, const std::filesystem::path &output) const;
  std::filesystem::path prepare_probe(const std::string &symbol_id,
      const std::filesystem::path &module, const std::filesystem::path &output,
      const std::filesystem::path &generated = {}, bool metadata_only = false) const;

private:
  std::filesystem::path path_;
  ModuleIdentity identity_;
};

} // namespace tracer_module

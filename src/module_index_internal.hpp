#pragma once

#include "tracer/module_index.hpp"

#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/JSON.h>

#include <functional>
#include <string>
#include <string_view>

namespace tracer::module_index {

using namespace ::tracer_module;

using Elf = llvm::object::ELF64LEObjectFile;
namespace fs = std::filesystem;

std::string digest(llvm::StringRef bytes);
std::string inspect_elf(const fs::path &, const std::function<void(const Elf &)> & = {},
                        std::string_view expected_digest = {});
ModuleIdentity scan(const ModuleSource &, const ModuleIndexOptions &,
                    const std::function<void(const ModuleType &)> &,
                    const std::function<void(const ModuleSymbol &)> &);

llvm::json::Object json(const ModuleIdentity &);
llvm::json::Object json(const ModuleType &);
llvm::json::Object json(const ModuleSymbol &);
std::string dump(llvm::json::Value);
std::string normalize_selector(std::string_view);
std::string read_file(const fs::path &);
void write_file(const fs::path &, std::string_view);

struct HeaderBundle {
  std::string declarations, views, manifest, preamble;
};
HeaderBundle headers(const ModuleIndex &, const ModuleSelection &);

struct IndexedCpp { std::string signature, preamble; };
IndexedCpp indexed_cpp(const ModuleIndex &, const ModuleSymbol &);
fs::path prepare_indexed(const ModuleIndex &, const std::string &id,
                         const fs::path &module, const fs::path &output,
                         const fs::path &generated, bool metadata_only);


} // namespace tracer::module_index

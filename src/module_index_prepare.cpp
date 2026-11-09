#include "module_index_internal.hpp"
#include "prepare_elf_signature.hpp"
#include "prepare_package.hpp"

#include <llvm/ADT/StringExtras.h>

#include <stdexcept>

namespace tracer::module_index {

std::filesystem::path prepare_indexed(const ModuleIndex &index, const std::string &symbol_id,
    const std::filesystem::path &module, const std::filesystem::path &output,
    const std::filesystem::path &generated, bool metadata_only) {
  const auto image = fs::canonical(module.empty() ? fs::path(index.identity().image) : module);
  const auto directory = fs::absolute(output);
  if (fs::exists(directory) || fs::is_symlink(directory))
    throw std::invalid_argument("output already exists; choose a new directory");
  const auto symbol = index.get_symbol(symbol_id);
  auto cpp = indexed_cpp(index, symbol);
  if (!generated.empty()) {
    const auto manifest = read_file(generated / "manifest.json");
    auto parsed = llvm::json::parse(manifest);
    if (!parsed) throw std::invalid_argument(llvm::toString(parsed.takeError()));
    const auto *root = parsed->getAsObject();
    if (!root) throw std::invalid_argument("invalid generated bundle manifest");
    ModuleSelection selection;
    const auto extract = [&](const char *key, std::vector<std::string> &ids) {
      const auto *entries = root->getArray(key);
      if (!entries) throw std::invalid_argument("invalid generated bundle selection");
      for (const auto &entry : *entries) {
        const auto id = entry.getAsString();
        if (!id) throw std::invalid_argument("invalid generated bundle entry");
        ids.push_back(id->str());
      }
    };
    extract("symbols", selection.symbols);
    extract("types", selection.types);
    if (std::find(selection.symbols.begin(), selection.symbols.end(), symbol_id) ==
        selection.symbols.end())
      throw std::invalid_argument("generated bundle does not contain the selected symbol");
    const auto expected = headers(index, selection);
    if (manifest != expected.manifest ||
        read_file(generated / "declarations.hpp") != expected.declarations ||
        read_file(generated / "views.hpp") != expected.views)
      throw std::invalid_argument("generated bundle differs from the module index");
    cpp.preamble = expected.preamble;
  }
  const tracer::client::IndexedElfSignature evidence{cpp.preamble, symbol.linkage,
                                                       symbol.address, symbol.size};
  std::optional<client::ResolvedCallable> resolved;
  const auto hash = inspect_elf(image, [&](const Elf &elf) {
    resolved = client::resolve_elf_signature(elf, symbol.selector, cpp.signature, symbol.member,
                                             &evidence);
  }, index.identity().sha256);
  client::write_prepared_package(*resolved, symbol.selector, image, llvm::fromHex(hash),
                                 directory, metadata_only);
  return directory;
}

} // namespace tracer::module_index

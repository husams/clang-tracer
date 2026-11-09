#pragma once

#include <llvm/ADT/StringExtras.h>
#include <llvm/Object/BuildID.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/CRC.h>
#include <llvm/Support/Endian.h>

#include <filesystem>
#include <optional>
#include <vector>

namespace tracer::debug_file {

using Elf = llvm::object::ELF64LEObjectFile;
namespace fs = std::filesystem;

struct Link {
  std::string name;
  std::optional<std::uint32_t> crc;
};

inline Link debug_link(const Elf &object) {
  for (const auto &section : object.sections()) {
    auto name = section.getName();
    if (!name) {
      llvm::consumeError(name.takeError());
      continue;
    }
    if (*name != ".gnu_debuglink") continue;
    auto data = section.getContents();
    if (!data) {
      llvm::consumeError(data.takeError());
      return {};
    }
    const auto end = data->find('\0');
    if (end == llvm::StringRef::npos || !end) return {};
    const auto offset = (end + 4) & ~std::size_t(3);
    if (offset > data->size() || data->size() - offset < 4) return {};
    const auto filename = data->take_front(end);
    if (filename.contains('/') || filename == "." || filename == "..") return {};
    return {filename.str(), llvm::support::endian::read32le(data->data() + offset)};
  }
  return {};
}

using Binary = llvm::object::OwningBinary<llvm::object::Binary>;
struct Companion {
  fs::path path;
  Binary binary;
};

inline std::optional<Companion> open(const fs::path &path, const Elf &object,
                                     const fs::path &explicit_path = {}) {
  const auto id = llvm::object::getBuildID(&object);
  const auto link = debug_link(object);
  std::vector<std::pair<fs::path, bool>> candidates;
  if (!explicit_path.empty()) {
    candidates.emplace_back(explicit_path, false);
  } else {
    if (link.crc) {
      candidates.emplace_back(path.parent_path() / link.name, false);
      candidates.emplace_back(path.parent_path() / ".debug" / link.name, false);
      candidates.emplace_back("/usr/lib/debug" / path.parent_path().relative_path() / link.name, false);
    }
    if (!id.empty()) {
      const auto hex = llvm::toHex(id, true);
      candidates.emplace_back("/usr/lib/debug/.build-id/" + hex.substr(0, 2) +
                              "/" + hex.substr(2) + ".debug", true);
    }
  }
  for (const auto &[candidate, require_id] : candidates) {
    auto binary = llvm::object::createBinary(candidate.string());
    if (!binary) {
      llvm::consumeError(binary.takeError());
      continue;
    }
    const auto *debug = llvm::dyn_cast<Elf>(binary->getBinary());
    if (!debug || debug->getArch() != llvm::Triple::x86_64) continue;
    const auto debug_id = llvm::object::getBuildID(debug);
    const bool id_match = !id.empty() && id == debug_id;
    if ((!id.empty() && !debug_id.empty() && !id_match) || (require_id && !id_match))
      continue;
    const auto bytes = debug->getMemoryBufferRef().getBuffer();
    const bool crc_match = link.crc && *link.crc == llvm::crc32(
        llvm::ArrayRef<std::uint8_t>(reinterpret_cast<const std::uint8_t *>(bytes.data()),
                                    bytes.size()));
    if ((link.crc && !require_id && !crc_match) || (!id_match && !crc_match)) continue;
    return Companion{candidate, std::move(*binary)};
  }
  return std::nullopt;
}

} // namespace tracer::debug_file

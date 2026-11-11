#pragma once

#include "tracer.pb.h"
#include "../mapped_path.hpp"

#include <cstdint>
#include <istream>
#include <map>
#include <string>
#include <vector>

#if defined(__linux__) && defined(__x86_64__)
#include <algorithm>
#include <charconv>
#include <cstring>
#include <set>
#include <sstream>
#include <string_view>
#include <tuple>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include <llvm/ADT/ScopeExit.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Object/BuildID.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/MemoryBuffer.h>

// The system ELF macros share names with LLVM's ELF constants.
#include <link.h>
#endif

namespace tracer_repl {

struct PreparedLibrary {
  std::string path;
  std::string build_id;
};

#if defined(__linux__) && defined(__x86_64__)
namespace prepared_library_detail {

inline bool matches_device(std::string_view device, dev_t file_device) {
  const auto colon = device.find(':');
  if (colon == device.npos) return false;
  const auto matches = [](std::string_view component, unsigned expected) {
    unsigned number = 0;
    const auto end = component.data() + component.size();
    const auto parsed = std::from_chars(component.data(), end, number, 16);
    return parsed.ec == std::errc{} && parsed.ptr == end && number == expected;
  };
  return matches(device.substr(0, colon), major(file_device)) &&
         matches(device.substr(colon + 1), minor(file_device));
}

inline std::set<std::string> verified_exports(
    const std::string &path,
    const std::vector<const clangtracer::v1::ModuleMapping *> &mappings,
    const std::set<std::string, std::less<>> &wanted, std::string &build_id) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK);
  if (fd < 0) return {};
  const auto close = llvm::make_scope_exit([fd] { ::close(fd); });
  struct stat file{};
  if (::fstat(fd, &file) != 0 || !S_ISREG(file.st_mode) || file.st_size <= 0) return {};
  const auto mapping = std::find_if(mappings.begin(), mappings.end(), [&](const auto *entry) {
    return entry->inode() == file.st_ino && matches_device(entry->device(), file.st_dev);
  });
  if (mapping == mappings.end()) return {};

  auto buffer = llvm::MemoryBuffer::getOpenFile(fd, path, file.st_size);
  if (!buffer) return {};
  auto binary = llvm::object::createBinary((*buffer)->getMemBufferRef());
  if (!binary) {
    llvm::consumeError(binary.takeError());
    return {};
  }
  const auto *object = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(binary->get());
  if (!object || object->getArch() != llvm::Triple::x86_64) return {};
  const auto &elf = object->getELFFile();
  if (elf.getHeader().e_type != ET_DYN ||
      llvm::toHex(llvm::object::getBuildID(object), true) != (*mapping)->build_id())
    return {};

  auto programs = elf.program_headers();
  if (!programs) {
    llvm::consumeError(programs.takeError());
    return {};
  }
  if (std::any_of(programs->begin(), programs->end(), [](const auto &program) {
        return program.p_type == PT_INTERP;
      }))
    return {};
  auto entries = elf.dynamicEntries();
  if (!entries) {
    llvm::consumeError(entries.takeError());
    return {};
  }
  for (const auto &entry : *entries)
    if (entry.d_tag == DT_FLAGS_1 && (entry.d_un.d_val & DF_1_PIE))
      return {};

  llvm::ArrayRef<llvm::object::ELF64LE::Versym> versions;
  auto sections = elf.sections();
  if (!sections) {
    llvm::consumeError(sections.takeError());
    return {};
  }
  for (const auto &section : *sections) {
    if (section.sh_type != SHT_GNU_versym) continue;
    auto indices = elf.getSectionContentsAsArray<llvm::object::ELF64LE::Versym>(section);
    if (!indices) {
      llvm::consumeError(indices.takeError());
      return {};
    }
    versions = *indices;
    break;
  }

  std::set<std::string> exported;
  std::size_t ordinal = 0;
  for (const auto &symbol : object->getDynamicSymbolIterators()) {
    // The dynamic symbol iterator omits the ELF table's mandatory null entry.
    ++ordinal;
    if (!versions.empty() &&
        (ordinal >= versions.size() || (versions[ordinal].vs_index & llvm::ELF::VERSYM_HIDDEN)))
      continue;
    auto name = symbol.getName();
    if (!name) {
      llvm::consumeError(name.takeError());
      continue;
    }
    const auto required = wanted.find(std::string_view(name->data(), name->size()));
    if (required == wanted.end()) continue;
    // The JIT calls destructor variants and reads vtables, VTTs, and RTTI as data.
    const auto expected_type = name->starts_with("_ZN") ? STT_FUNC : STT_OBJECT;
    if (llvm::object::ELFSymbolRef(symbol).getELFType() != expected_type) continue;
    auto flags = symbol.getFlags();
    if (!flags) {
      llvm::consumeError(flags.takeError());
      continue;
    }
    if ((*flags & llvm::object::SymbolRef::SF_Exported) &&
        !(*flags & llvm::object::SymbolRef::SF_Undefined))
      exported.insert(*required);
  }
  build_id = (*mapping)->build_id();
  return exported;
}

} // namespace prepared_library_detail
#endif

inline std::map<std::vector<std::string>, PreparedLibrary> find_prepared_libraries(
    const clangtracer::v1::ListModulesResponse &modules,
    const std::vector<std::vector<std::string>> &groups) {
  std::map<std::vector<std::string>, PreparedLibrary> matches;
#if defined(__linux__) && defined(__x86_64__)
  std::set<std::string, std::less<>> wanted;
  for (const auto &group : groups)
    wanted.insert(group.begin(), group.end());
  if (wanted.empty()) return matches;

  std::map<std::string, std::vector<const clangtracer::v1::ModuleMapping *>> candidates;
  for (const auto &entry : modules.mappings()) {
    const auto &path = entry.path();
    if (entry.inode() && !entry.build_id().empty() && !path.empty() && path.front() == '/' &&
        path.find('\0') == path.npos)
      candidates[path].push_back(&entry);
  }
  for (const auto &[path, mappings] : candidates) {
    std::string build_id;
    const auto exported = prepared_library_detail::verified_exports(path, mappings, wanted, build_id);
    for (const auto &group : groups) {
      if (group.empty() || !std::all_of(group.begin(), group.end(), [&](const auto &name) {
            return exported.count(name) != 0;
          }))
        continue;
      auto [match, inserted] = matches.try_emplace(group, PreparedLibrary{path, build_id});
      if (!inserted && match->second.path != path) match->second.path.clear();
    }
  }
  for (auto match = matches.begin(); match != matches.end();) {
    if (match->second.path.empty()) match = matches.erase(match);
    else ++match;
  }
#else
  (void)modules;
  (void)groups;
#endif
  return matches;
}

// Keeps only mappings that the local kernel also reports for the process, so
// a server cannot steer the REPL to load a library the process never mapped.
inline clangtracer::v1::ListModulesResponse confirmed_local_mappings(
    clangtracer::v1::ListModulesResponse modules, std::istream &process_maps) {
#if defined(__linux__) && defined(__x86_64__)
  using Key = std::tuple<std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::string>;
  const auto number = [](const std::string &text, int base, std::uint64_t &value) {
    const auto end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, value, base);
    return !text.empty() && parsed.ec == std::errc{} && parsed.ptr == end;
  };
  // The server reports canonical paths, so a line may match by raw or resolved path.
  std::map<Key, std::vector<std::string>> local;
  std::string text;
  while (std::getline(process_maps, text)) {
    std::istringstream row(text);
    std::string range, permissions, offset, device, inode, path;
    if (!(row >> range >> permissions >> offset >> device >> inode)) continue;
    std::getline(row >> std::ws, path);
    const auto dash = range.find('-');
    std::uint64_t begin = 0, end = 0, file_offset = 0, file_inode = 0;
    if (path.empty() || path.front() != '/' || dash == range.npos ||
        !number(range.substr(0, dash), 16, begin) || !number(range.substr(dash + 1), 16, end) ||
        !number(offset, 16, file_offset) || !number(inode, 10, file_inode))
      continue;
    local[Key{begin, end, file_offset, file_inode, std::move(device)}].push_back(std::move(path));
  }
  std::vector<clangtracer::v1::ModuleMapping> kept;
  for (const auto &entry : modules.mappings()) {
    if (entry.path().empty() || entry.inode() == 0) continue;
    const auto candidates = local.find(Key{entry.begin_address(), entry.end_address(),
                                           entry.file_offset(), entry.inode(), entry.device()});
    if (candidates == local.end()) continue;
    const auto &raw = candidates->second;
    if (std::find(raw.begin(), raw.end(), entry.path()) != raw.end() ||
        std::any_of(raw.begin(), raw.end(), [&](const std::string &path) {
          return tracer::detail::canonical_mapped_path(path) == entry.path();
        }))
      kept.push_back(entry);
  }
  modules.clear_mappings();
  for (auto &entry : kept) *modules.add_mappings() = std::move(entry);
#else
  (void)process_maps;
  modules.clear_mappings();
#endif
  return modules;
}

inline bool prepared_library_build_id_matches(std::uintptr_t load_address,
                                              const std::string &build_id) {
#if defined(__linux__) && defined(__x86_64__)
  struct Lookup {
    std::uintptr_t address;
    std::string expected;
  } lookup{load_address, {}};
  if (build_id.empty() || !llvm::tryGetFromHex(build_id, lookup.expected)) return false;
  return ::dl_iterate_phdr([](dl_phdr_info *image, std::size_t, void *data) {
    const auto &lookup = *static_cast<const Lookup *>(data);
    if (image->dlpi_addr != lookup.address) return 0;
    const auto *end = image->dlpi_phdr + image->dlpi_phnum;
    for (auto segment = image->dlpi_phdr; segment != end; ++segment) {
      if (segment->p_type != PT_NOTE || (segment->p_align > 1 &&
          segment->p_align != 4 && segment->p_align != 8)) continue;
      // PT_NOTE describes a file region. Check that the bytes were mapped.
      if (std::none_of(image->dlpi_phdr, end, [&](const auto &load) {
            if (load.p_type != PT_LOAD || !(load.p_flags & PF_R) ||
                segment->p_vaddr < load.p_vaddr) return false;
            const auto offset = segment->p_vaddr - load.p_vaddr;
            return offset <= load.p_filesz && segment->p_filesz <= load.p_filesz - offset;
          })) continue;
      const auto *cursor = reinterpret_cast<const unsigned char *>(lookup.address + segment->p_vaddr);
      std::size_t remaining = segment->p_filesz;
      const std::size_t align = segment->p_align == 8 ? 8 : 4;
      const auto padded = [align](std::size_t size) { return (size + align - 1) & ~(align - 1); };
      while (remaining >= sizeof(ElfW(Nhdr))) {
        ElfW(Nhdr) note{};
        std::memcpy(&note, cursor, sizeof(note));
        const auto offset = padded(sizeof(note) + std::size_t{note.n_namesz});
        const auto size = padded(note.n_descsz);
        if (offset > remaining || size > remaining - offset) break;
        if (note.n_type == NT_GNU_BUILD_ID && note.n_namesz == 4 &&
            note.n_descsz == lookup.expected.size() &&
            std::memcmp(cursor + sizeof(note), "GNU", 4) == 0 &&
            std::memcmp(cursor + offset, lookup.expected.data(), note.n_descsz) == 0)
          return 1;
        cursor += offset + size;
        remaining -= offset + size;
      }
    }
    return 0;
  }, &lookup) == 1;
#else
  (void)load_address;
  (void)build_id;
  return false;
#endif
}

} // namespace tracer_repl

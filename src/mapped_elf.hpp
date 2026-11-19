#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Object/ELF.h>
#include <llvm/Object/BuildID.h>

namespace tracer::detail {

// Translate a pathname-matched VMA through PT_LOAD file offsets. Procfs
// device/inode numbers deliberately play no part in mapped image identity.
inline std::optional<std::uint64_t> mapped_elf_bias(
    const llvm::object::ELF64LEObjectFile &object, std::uint64_t begin,
    std::uint64_t end, std::uint64_t offset, bool executable = true,
    std::optional<std::uint64_t> expected_bias = std::nullopt) {
  if (begin >= end)
    return std::nullopt;
  auto programs = object.getELFFile().program_headers();
  if (!programs) {
    llvm::consumeError(programs.takeError());
    return std::nullopt;
  }
  std::optional<std::uint64_t> bias;
  for (const auto &segment : *programs) {
    if (segment.p_type != llvm::ELF::PT_LOAD ||
        (executable && !(segment.p_flags & llvm::ELF::PF_X)))
      continue;
    const auto file_begin = std::max<std::uint64_t>(offset, segment.p_offset);
    if (file_begin - offset >= end - begin ||
        file_begin - segment.p_offset >= segment.p_filesz ||
        file_begin - segment.p_offset > UINT64_MAX - segment.p_vaddr)
      continue;
    const auto runtime = begin + (file_begin - offset);
    const auto address = segment.p_vaddr + (file_begin - segment.p_offset);
    if (runtime < address ||
        (object.getELFFile().getHeader().e_type == llvm::ELF::ET_EXEC && runtime != address))
      continue;
    const auto candidate = runtime - address;
    if (expected_bias && candidate != *expected_bias)
      continue;
    if (bias && *bias != candidate)
      return std::nullopt;
    bias = candidate;
  }
  return bias;
}

// The reader compares target memory with expected bytes, without dereferencing
// target pointers. A build ID binds the disk ELF to the loaded image. For ELFs
// without one, compare the ELF header and immutable allocated non-code
// sections. Code is checked separately immediately before installing a patch;
// excluding it here permits our own redirects, including on old single-RX-load
// SDK libraries with no build ID.
template <class MatchesBytes>
bool matches_mapped_elf(const llvm::object::ELF64LEObjectFile &object,
                        std::uint64_t bias, MatchesBytes matches) {
  const auto file = object.getMemoryBufferRef().getBuffer();
  const auto id = llvm::object::getBuildID(&object);
  auto programs = object.getELFFile().program_headers();
  if (!programs) {
    llvm::consumeError(programs.takeError());
    return false;
  }
  if (!id.empty()) {
    const auto offset = static_cast<std::uint64_t>(id.data() -
        reinterpret_cast<const std::uint8_t *>(file.data()));
    for (const auto &segment : *programs) {
      if (segment.p_type != llvm::ELF::PT_LOAD || offset < segment.p_offset)
        continue;
      const auto delta = offset - segment.p_offset;
      if (delta > segment.p_filesz || id.size() > segment.p_filesz - delta ||
          segment.p_vaddr > UINT64_MAX - bias || delta > UINT64_MAX - bias - segment.p_vaddr)
        continue;
      return matches(bias + segment.p_vaddr + delta,
                     std::string_view(reinterpret_cast<const char *>(id.data()), id.size()));
    }
    return false;
  }
  bool header = false;
  for (const auto &segment : *programs) {
    if (segment.p_type == llvm::ELF::PT_LOAD && segment.p_offset == 0 &&
        segment.p_filesz >= sizeof(llvm::ELF::Elf64_Ehdr) &&
        file.size() >= sizeof(llvm::ELF::Elf64_Ehdr) && segment.p_vaddr <= UINT64_MAX - bias) {
      header = matches(bias + segment.p_vaddr,
                       std::string_view(file.data(), sizeof(llvm::ELF::Elf64_Ehdr)));
      break;
    }
  }
  if (!header)
    return false;
  auto sections = object.getELFFile().sections();
  if (!sections) {
    llvm::consumeError(sections.takeError());
    return false;
  }
  bool immutable_content = false;
  for (const auto &section : *sections) {
    if (!(section.sh_flags & llvm::ELF::SHF_ALLOC) || !section.sh_size ||
        (section.sh_flags & (llvm::ELF::SHF_WRITE | llvm::ELF::SHF_EXECINSTR)) ||
        section.sh_type == llvm::ELF::SHT_NOBITS)
      continue;
    const bool loaded = std::any_of(programs->begin(), programs->end(), [&](const auto &segment) {
      return segment.p_type == llvm::ELF::PT_LOAD && section.sh_addr >= segment.p_vaddr &&
          section.sh_addr - segment.p_vaddr <= segment.p_filesz &&
          section.sh_size <= segment.p_filesz - (section.sh_addr - segment.p_vaddr) &&
          section.sh_offset >= segment.p_offset &&
          section.sh_offset - segment.p_offset == section.sh_addr - segment.p_vaddr;
    });
    if (!loaded || section.sh_offset > file.size() || section.sh_size > file.size() - section.sh_offset ||
        section.sh_addr > UINT64_MAX - bias || section.sh_size > UINT64_MAX - bias - section.sh_addr ||
        !matches(bias + section.sh_addr,
                 std::string_view(file.data() + section.sh_offset, section.sh_size)))
      return false;
    immutable_content = true;
  }
  return immutable_content;
}

} // namespace tracer::detail

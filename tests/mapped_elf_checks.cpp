#include "../src/mapped_elf.hpp"
#include "../src/linux_syscalls.hpp"
#include <link.h>

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
}
void check(const char *path, bool has_id) {
  auto binary = llvm::object::createBinary(path);
  if (!binary) throw std::runtime_error(llvm::toString(binary.takeError()));
  const auto &object = llvm::cast<llvm::object::ELF64LEObjectFile>(*binary->getBinary());
  auto programs = object.getELFFile().program_headers();
  if (!programs) throw std::runtime_error(llvm::toString(programs.takeError()));
  const auto bytes = object.getMemoryBufferRef().getBuffer();
  const auto id = llvm::object::getBuildID(&object);
  require(id.empty() != has_id, "wrong fixture build-ID mode");
  constexpr std::uint64_t bias = 0x10000000;
  struct Range { std::uint64_t address; std::string bytes; };
  std::vector<Range> live;
  std::uint64_t corrupt = 0;
  for (const auto &segment : *programs) {
    if (segment.p_type != PT_LOAD || !segment.p_filesz) continue;
    live.push_back({bias + segment.p_vaddr, bytes.substr(segment.p_offset, segment.p_filesz).str()});
    if (!has_id && segment.p_offset == 0) corrupt = bias + segment.p_vaddr;
    if (has_id) {
      const auto offset = id.data() - reinterpret_cast<const std::uint8_t *>(bytes.data());
      if (offset >= segment.p_offset && offset - segment.p_offset < segment.p_filesz)
        corrupt = bias + segment.p_vaddr + offset - segment.p_offset;
    }
    if (segment.p_flags & PF_X) {
      const auto result = tracer::detail::mapped_elf_bias(object, bias + segment.p_vaddr,
          bias + segment.p_vaddr + segment.p_filesz, segment.p_offset);
      require(result && *result == bias, "incorrect offset/range translation");
      require(!tracer::detail::mapped_elf_bias(object, 1, 1, segment.p_offset), "empty range accepted");
      require(!tracer::detail::mapped_elf_bias(object, bias, bias + 4096, bytes.size()), "unmapped file offset accepted");
    }
  }
  const auto matches = [&](auto address, auto expected) {
    for (const auto &range : live)
      if (address >= range.address && address - range.address <= range.bytes.size() &&
          expected.size() <= range.bytes.size() - (address - range.address))
        return std::string_view(range.bytes).substr(address - range.address, expected.size()) == expected;
    return false;
  };
  require(tracer::detail::matches_mapped_elf(object, bias, matches), "identical content rejected");
  require(!tracer::detail::matches_mapped_elf(object, bias + 4096, matches), "wrong load bias accepted");
  require(corrupt != 0, "no identity bytes found");
  for (auto &range : live)
    if (corrupt >= range.address && corrupt - range.address < range.bytes.size())
      range.bytes[corrupt - range.address] ^= 1;
  require(!tracer::detail::matches_mapped_elf(object, bias, matches), "different live content accepted");
  if (!has_id) {
    // Keep the ELF header unchanged: a fallback must also check loaded data.
    for (auto &range : live)
      if (corrupt >= range.address && corrupt - range.address < range.bytes.size())
        range.bytes[corrupt - range.address] ^= 1;
    auto sections = object.getELFFile().sections();
    if (!sections) throw std::runtime_error(llvm::toString(sections.takeError()));
    bool changed = false;
    for (const auto &section : *sections) {
      if (!(section.sh_flags & SHF_ALLOC) || !section.sh_size ||
          (section.sh_flags & (SHF_WRITE | SHF_EXECINSTR)) || section.sh_type == SHT_NOBITS)
        continue;
      for (auto &range : live)
        if (bias + section.sh_addr >= range.address &&
            bias + section.sh_addr - range.address < range.bytes.size()) {
          range.bytes[bias + section.sh_addr - range.address] ^= 1;
          changed = true;
        }
      if (changed) break;
    }
    require(changed, "no immutable fallback section found");
    require(!tracer::detail::matches_mapped_elf(object, bias, matches), "changed immutable section accepted");
  }
  std::cout << "verified " << (has_id ? "build ID" : "no-build-ID fallback") << '\n';
}
}
int main(int argc, char **argv) {
  try {
    require(argc == 3, "need build-ID and no-build-ID fixtures");
    check(argv[1], true);
    check(argv[2], false);
    std::string failure;
    const auto stopped = ::dl_iterate_phdr([](dl_phdr_info *info, std::size_t, void *data) {
      if (!info->dlpi_name[0] || info->dlpi_name[0] != '/') return 0;
      auto &failure = *static_cast<std::string *>(data);
      auto binary = llvm::object::createBinary(info->dlpi_name);
      if (!binary) {
        failure = llvm::toString(binary.takeError());
        return 1;
      }
      const auto *object = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(binary->getBinary());
      const bool matched = object && tracer::detail::matches_mapped_elf(*object, info->dlpi_addr,
          [](auto address, auto expected) {
            std::vector<char> bytes(expected.size());
            iovec local{bytes.data(), bytes.size()}, remote{reinterpret_cast<void *>(address), bytes.size()};
            return tracer::linux_api::process_vm_readv(::getpid(), &local, 1, &remote, 1, 0) ==
                static_cast<ssize_t>(bytes.size()) && std::string_view(bytes.data(), bytes.size()) == expected;
          });
      if (!matched) failure = std::string("live library rejected: ") + info->dlpi_name;
      return matched ? 0 : 1;
    }, &failure);
    if (stopped) throw std::runtime_error(failure);
    std::cout << "verified live SDK libraries\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

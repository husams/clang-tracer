#pragma once

#include "cpp_resolver.hpp"

#include <cstdint>
#include <string>

#include <llvm/Object/ELFObjectFile.h>

namespace tracer::client {
struct IndexedElfSignature {
  std::string preamble, linkage;
  std::uint64_t address, size;
};

ResolvedCallable resolve_elf_signature(
    const llvm::object::ELF64LEObjectFile &image, const std::string &selector,
    const std::string &signature, bool member,
    const IndexedElfSignature *indexed = nullptr);
} // namespace tracer::client

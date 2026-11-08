#pragma once

#include "cpp_resolver.hpp"

#include <string>

#include <llvm/Object/ELFObjectFile.h>

namespace tracer::client {
ResolvedCallable resolve_elf_signature(
    const llvm::object::ELF64LEObjectFile &image, const std::string &selector,
    const std::string &signature, bool member);
} // namespace tracer::client

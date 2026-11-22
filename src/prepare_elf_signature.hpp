#pragma once

#include "cpp_resolver.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <llvm/Object/ELFObjectFile.h>

namespace tracer::client {
struct IndexedElfSignature {
  std::string preamble, linkage;
  std::uint64_t address, size;
  // The role types from debug information. A type context compiles these one by one, so a
  // missing or incompatible definition is reported for its own role, and cross-checks each
  // by-value record against the size recorded for it.
  std::string result_type;
  std::vector<std::string> argument_types;
  std::optional<std::uint64_t> result_size;
  std::vector<std::optional<std::uint64_t>> argument_sizes;
};

ResolvedCallable resolve_elf_signature(
    const llvm::object::ELF64LEObjectFile &image, const std::string &selector,
    const std::string &signature, bool member,
    const IndexedElfSignature *indexed = nullptr, const TypeContext *context = nullptr);
} // namespace tracer::client

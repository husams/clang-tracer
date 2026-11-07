#pragma once

#include "prepared_contract.hpp"

#include <clang/Basic/FileManager.h>
#include <clang/Lex/HeaderSearch.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Path.h>

namespace tracer::prepared {

// Resolve an angle-bracket include against a compiler initialized with just
// the toolchain options. Stop at a shadowing non-system search entry.
inline clang::OptionalFileEntryRef toolchain_header(const clang::HeaderSearch &search,
                                                  llvm::StringRef name) {
  if (!valid_include_name(std::string_view(name.data(), name.size())))
    return std::nullopt;
  for (auto dir = search.angled_dir_begin(); dir != search.search_dir_end(); ++dir) {
    if (!dir->isNormalDir())
      continue;
    llvm::SmallString<256> path(dir->getName());
    llvm::sys::path::append(path, name);
    if (auto file = search.getFileMgr().getOptionalFileRef(path))
      return dir->isSystemHeaderDirectory() ? file : std::nullopt;
  }
  return std::nullopt;
}

inline void check_toolchain_headers(const clang::HeaderSearch &search,
                                    const clangtracer::v1::PreparedInjector &package) {
  for (const auto &header : package.adapter_standard_headers())
    if (!toolchain_header(search, header))
      throw std::invalid_argument("prepared header <" + header +
                                  "> is unavailable in the selected toolchain");
}

} // namespace tracer::prepared

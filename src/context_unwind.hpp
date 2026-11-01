#pragma once

#include "context_stdlib_abi.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <llvm/Support/Error.h>

namespace tracer {
// Setup-built immutable index over relocated EH records. Guarded lookup reads
// only this index; it never invokes the loader, registers FDEs or allocates.
class ContextUnwindTable final {
public:
  llvm::Error add_region(const void *, std::size_t, std::uintptr_t load_address,
                         unsigned target_arch);
  const void *find(std::uintptr_t,
                   context_support::UnwindBases &) const noexcept;
  std::size_t size() const noexcept { return entries_.size(); }
  const std::string &error() const noexcept { return error_; }
  void record_error(std::string error) {
    if (error_.empty())
      error_ = std::move(error);
  }

private:
  struct Entry {
    std::uintptr_t begin, end;
    const void *fde;
  };
  std::vector<Entry> entries_;
  std::size_t bytes_ = 0;
  std::string error_;
};
} // namespace tracer

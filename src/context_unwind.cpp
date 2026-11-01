#include "context_unwind.hpp"

#include <algorithm>
#include <limits>

#include <llvm/DebugInfo/DWARF/DWARFDataExtractor.h>
#include <llvm/DebugInfo/DWARF/DWARFDebugFrame.h>

namespace tracer {
llvm::Error ContextUnwindTable::add_region(const void *bytes, std::size_t size,
                                           std::uintptr_t load_address,
                                           unsigned target_arch) {
  const auto invalid = [](const std::string &reason) {
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                   "unsupported_unwind_path: " + reason);
  };
  if (!bytes || !size || size > 16 * 1024 * 1024 ||
      bytes_ > 16 * 1024 * 1024 - size)
    return invalid("EH metadata exceeds the frozen generation budget");
  llvm::DWARFDebugFrame frame(static_cast<llvm::Triple::ArchType>(target_arch),
                              true, load_address);
  if (auto error = frame.parse(llvm::DWARFDataExtractor(
          llvm::StringRef(static_cast<const char *>(bytes), size), true, 8)))
    return error;
  auto next = entries_;
  for (const auto &entry : frame.entries()) {
    const auto *fde = llvm::dyn_cast<llvm::dwarf::FDE>(&entry);
    if (!fde)
      continue;
    const auto *cie = fde->getLinkedCIE();
    if (!cie || !fde->getAddressRange() || fde->getOffset() >= size ||
        fde->getInitialLocation() >
            std::numeric_limits<std::uintptr_t>::max() - fde->getAddressRange())
      return invalid("incomplete FDE/CIE range");
    const auto encoding = cie->getFDEPointerEncoding() & 0x70;
    if (encoding && encoding != llvm::dwarf::DW_EH_PE_pcrel)
      return invalid("unadmitted FDE base encoding");
    if (next.size() == 65536)
      return invalid("too many FDEs in one code generation");
    next.push_back({fde->getInitialLocation(),
                    fde->getInitialLocation() + fde->getAddressRange(),
                    static_cast<const std::byte *>(bytes) + fde->getOffset()});
  }
  std::sort(next.begin(), next.end(),
            [](const auto &a, const auto &b) { return a.begin < b.begin; });
  for (std::size_t i = 1; i < next.size(); ++i)
    if (next[i].begin < next[i - 1].end)
      return invalid("overlapping EH ranges");
  entries_ = std::move(next);
  bytes_ += size;
  return llvm::Error::success();
}
const void *
ContextUnwindTable::find(std::uintptr_t pc,
                         context_support::UnwindBases &bases) const noexcept {
  auto found = std::upper_bound(
      entries_.begin(), entries_.end(), pc,
      [](auto value, const auto &entry) { return value < entry.begin; });
  if (found == entries_.begin())
    return nullptr;
  --found;
  if (pc >= found->end)
    return nullptr;
  bases = {nullptr, nullptr, reinterpret_cast<void *>(found->begin)};
  return found->fde;
}
} // namespace tracer

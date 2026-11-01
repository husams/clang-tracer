#pragma once

#include <cstddef>
#include <cstdint>

namespace tracer::context_support {
inline constexpr std::uint64_t kKernelAbi = 0x4354585355500001ULL;
enum UnwindEntry : unsigned {
  raise_exception,
  delete_exception,
  get_data_base,
  get_ip_info,
  get_language_data,
  get_region_start,
  get_text_base,
  resume,
  resume_or_rethrow,
  set_gr,
  set_ip,
  get_gr,
  get_ip,
  get_cfa,
  unwind_entries
};
struct UnwindBases {
  void *text = nullptr, *data = nullptr, *function = nullptr;
};
struct Kernel final {
  std::uint64_t abi = kKernelAbi;
  std::size_t bytes = sizeof(Kernel);
  void *domain = nullptr;
  void *(*allocate)(void *, std::size_t, std::size_t) noexcept = nullptr;
  unsigned (*release)(void *, void *, std::size_t,
                      std::size_t) noexcept = nullptr;
  std::size_t (*allocation_size)(void *, const void *) noexcept = nullptr;
  void *(*exception_allocate)(void *, std::size_t) noexcept = nullptr;
  void (*exception_release)(void *, void *) noexcept = nullptr;
  void *(*exception_globals)() noexcept = nullptr;
  int *(*error_location)() noexcept = nullptr;
  void (*panic)(void *, unsigned) noexcept = nullptr;
  bool (*owns_exception)(void *, const void *) noexcept = nullptr;
  bool (*private_unwind)() noexcept = nullptr;
  void (*set_private_unwind)(bool) noexcept = nullptr;
  const void *(*find_fde)(void *, std::uintptr_t,
                          UnwindBases *) noexcept = nullptr;
  std::uintptr_t unwind[unwind_entries]{};
};
struct Profile final {
  std::uint64_t abi;
  std::uint64_t standard_library_date;
  unsigned standard_library_release, dual_abi, cxx11_abi;
  unsigned pointer_bytes, wchar_bytes, any_bytes, any_alignment;
  unsigned shared_bytes, shared_alignment;
  unsigned exception_header_bytes, dependent_exception_bytes;
  char archive_sha256[65];
};
} // namespace tracer::context_support

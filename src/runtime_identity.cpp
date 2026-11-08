#include "runtime_identity.hpp"

#if defined(__linux__) && defined(__x86_64__)
#include <cstddef>

#include <dlfcn.h>
#include <link.h>

namespace tracer::runtime {
bool has_soname(const void* address, std::string_view expected) noexcept {
  if (!address) {
    return false;
  }
  Dl_info info{};
  link_map* module = nullptr;
  if (!::dladdr1(address, &info, reinterpret_cast<void**>(&module), RTLD_DL_LINKMAP) || !module ||
      !module->l_ld) {
    return false;
  }
  const char* strings = nullptr;
  std::size_t string_bytes = 0, name_offset = 0;
  bool found = false;
  for (const auto* entry = module->l_ld; entry->d_tag != DT_NULL; ++entry) {
    if (entry->d_tag == DT_STRTAB) {
      strings = reinterpret_cast<const char*>(entry->d_un.d_ptr);
    } else if (entry->d_tag == DT_STRSZ) {
      string_bytes = entry->d_un.d_val;
    } else if (entry->d_tag == DT_SONAME) {
      name_offset = entry->d_un.d_val;
      found = true;
    }
  }
  if (!found || !strings || name_offset >= string_bytes ||
      expected.size() >= string_bytes - name_offset) {
    return false;
  }
  return std::string_view(strings + name_offset, expected.size()) == expected &&
         strings[name_offset + expected.size()] == '\0';
}
} // namespace tracer::runtime
#endif

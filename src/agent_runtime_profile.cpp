#include "agent_runtime_profile.hpp"

#include <cstdio>
#include <cstring>

#include <dlfcn.h>
#include <link.h>
#if defined(__GLIBC__)
#include <gnu/libc-version.h>
#endif

#include <clang/Basic/Version.h>
#include <llvm/Config/llvm-config.h>

#include "tracer/protocol.hpp"

namespace {

struct BuildIdLookup {
  std::uintptr_t base;
  std::uint8_t (&id)[20];
};

int copy_build_id(dl_phdr_info *image, std::size_t, void *opaque) noexcept {
  auto &lookup = *static_cast<BuildIdLookup *>(opaque);
  if (image->dlpi_addr != lookup.base)
    return 0;
  for (unsigned i = 0; i < image->dlpi_phnum; ++i) {
    const auto &segment = image->dlpi_phdr[i];
    if (segment.p_type != PT_NOTE || segment.p_align != 4)
      continue;
    auto *cursor = reinterpret_cast<const unsigned char *>(lookup.base +
                                                          segment.p_vaddr);
    std::size_t remaining = segment.p_filesz;
    while (remaining >= sizeof(ElfW(Nhdr))) {
      ElfW(Nhdr) header{};
      std::memcpy(&header, cursor, sizeof(header));
      cursor += sizeof(header);
      remaining -= sizeof(header);
      const auto padded = [](std::size_t length) {
        return (length + 3) & ~std::size_t{3};
      };
      const auto name_size = padded(header.n_namesz);
      const auto value_size = padded(header.n_descsz);
      if (name_size > remaining || value_size > remaining - name_size)
        break;
      if (header.n_type == NT_GNU_BUILD_ID && header.n_namesz == 4 &&
          header.n_descsz == sizeof(lookup.id) &&
          std::memcmp(cursor, "GNU", 4) == 0) {
        std::memcpy(lookup.id, cursor + name_size, sizeof(lookup.id));
        return 1;
      }
      cursor += name_size + value_size;
      remaining -= name_size + value_size;
    }
  }
  return 0;
}

} // namespace

extern "C" int tracer_agent_runtime_profile_v1(
    tracer::AgentRuntimeProfile *output, std::uintptr_t size) noexcept {
  if (!output || size != sizeof(*output))
    return -1;
  tracer::AgentRuntimeProfile profile;
  Dl_info agent{};
  if (!::dladdr(reinterpret_cast<const void *>(&tracer_agent_runtime_profile_v1),
                &agent))
    return -1;
  BuildIdLookup lookup{reinterpret_cast<std::uintptr_t>(agent.dli_fbase),
                       profile.build_id};
  if (::dl_iterate_phdr(copy_build_id, &lookup) != 1)
    return -1;

  profile.bootstrap_protocol = tracer::kBootstrapVersion;
  profile.pointer_bytes = sizeof(void *);
  std::snprintf(profile.clang_version, sizeof(profile.clang_version), "%s",
                CLANG_VERSION_STRING);
  std::snprintf(profile.llvm_version, sizeof(profile.llvm_version), "%s",
                LLVM_VERSION_STRING);
#if defined(__GLIBC__)
  std::snprintf(profile.glibc_version, sizeof(profile.glibc_version), "%s",
                ::gnu_get_libc_version());
#endif
#if defined(_GLIBCXX_RELEASE)
  profile.libstdcxx_header_release = _GLIBCXX_RELEASE;
#endif
#if defined(_GLIBCXX_USE_CXX11_ABI)
  profile.libstdcxx_default_cxx11_abi = _GLIBCXX_USE_CXX11_ABI;
#endif
  Dl_info runtime{};
  if (::dladdr(::dlsym(RTLD_DEFAULT, "__cxa_throw"), &runtime) &&
      runtime.dli_fname &&
      std::strlen(runtime.dli_fname) < sizeof(profile.cxx_runtime_path))
    std::strcpy(profile.cxx_runtime_path, runtime.dli_fname);
  *output = profile;
  return 0;
}

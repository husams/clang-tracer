// Deterministic maps-versus-stat disagreement without requiring an overlay
// mount. The kernel's maps remain untouched. Used only by dedicated CTests.
#include <cstdlib>
#include <dlfcn.h>
#include <sys/stat.h>

namespace {
template <class Stat> int altered(int result, Stat *info) {
  if (!result && S_ISREG(info->st_mode)) {
    info->st_dev ^= 0x4000;
    // Preserve distinct identities for Clang's header cache while making
    // every regular file disagree with its kernel-reported mapping inode.
    if (std::getenv("TRACER_TEST_DIFFERENT_INODE"))
      info->st_ino ^= 0x40000000;
  }
  return result;
}
}

#define PATH_STAT(name, type) \
extern "C" int name(const char *path, type *info) noexcept { \
  static auto original = reinterpret_cast<int (*)(const char *, type *)>(::dlsym(RTLD_NEXT, #name)); \
  return altered(original(path, info), info); \
}
#define FD_STAT(name, type) \
extern "C" int name(int fd, type *info) noexcept { \
  static auto original = reinterpret_cast<int (*)(int, type *)>(::dlsym(RTLD_NEXT, #name)); \
  return altered(original(fd, info), info); \
}
#define XPATH_STAT(name, type) \
extern "C" int name(int version, const char *path, type *info) noexcept { \
  static auto original = reinterpret_cast<int (*)(int, const char *, type *)>(::dlsym(RTLD_NEXT, #name)); \
  return altered(original(version, path, info), info); \
}
#define XFD_STAT(name, type) \
extern "C" int name(int version, int fd, type *info) noexcept { \
  static auto original = reinterpret_cast<int (*)(int, int, type *)>(::dlsym(RTLD_NEXT, #name)); \
  return altered(original(version, fd, info), info); \
}
PATH_STAT(stat, struct stat)
PATH_STAT(stat64, struct stat64)
FD_STAT(fstat, struct stat)
FD_STAT(fstat64, struct stat64)
XPATH_STAT(__xstat, struct stat)
XPATH_STAT(__xstat64, struct stat64)
XFD_STAT(__fxstat, struct stat)
XFD_STAT(__fxstat64, struct stat64)

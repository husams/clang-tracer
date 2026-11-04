#include <cstring>
#include <stdexcept>

#include <dlfcn.h>
#include <pthread.h>

#include "runtime_identity.hpp"

namespace {
void application_function() {}
} // namespace

int main() {
  using tracer::runtime::has_soname;
  // In a non-PIE executable &memcpy can name its PLT stub, whose owner really
  // is the executable. Resolve the provider exactly as production lookups do.
  const auto memcpy_address = ::dlsym(RTLD_DEFAULT, "memcpy");
  if (!has_soname(memcpy_address, "libc.so.6") ||
      has_soname(reinterpret_cast<const void*>(&application_function), "libc.so.6") ||
      has_soname(nullptr, "libc.so.6")) {
    throw std::runtime_error("runtime identity accepted an application or rejected libc");
  }
  const auto pthread_create = ::dlsym(RTLD_DEFAULT, "pthread_create");
  if (!has_soname(pthread_create, "libc.so.6") && !has_soname(pthread_create, "libpthread.so.0")) {
    throw std::runtime_error("runtime identity rejected the pthread provider");
  }
}

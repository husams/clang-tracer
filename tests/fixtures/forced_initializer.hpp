#pragma once

#include <cstdio>
#include <cstdlib>

namespace clang_tracer_forced_initializer_fixture {
struct Marker final {
  Marker() noexcept {
    const char *path = std::getenv("CLANG_TRACER_FORCED_INCLUDE_MARKER");
    if (!path || !*path)
      return;
    if (std::FILE *file = std::fopen(path, "w")) {
      (void)std::fputs("initializer-ran\n", file);
      (void)std::fclose(file);
    }
  }
};

inline Marker marker;
} // namespace clang_tracer_forced_initializer_fixture

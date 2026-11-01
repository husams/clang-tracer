#include "context_paths.hpp"
#include <dlfcn.h>
#include <filesystem>
#include <stdexcept>

namespace tracer {
namespace {
namespace fs = std::filesystem;
int module_anchor;
fs::path module_directory() {
  Dl_info info{};
  if (!::dladdr(&module_anchor, &info) || !info.dli_fname)
    throw std::runtime_error("cannot locate the shared-context runtime image");
  return fs::canonical(info.dli_fname).parent_path();
}
} // namespace
std::string context_header_directory() {
  const auto installed = module_directory() / TRACER_CONTEXT_HEADER_RELATIVE;
  if (fs::is_regular_file(installed / "context_facade.hpp"))
    return fs::canonical(installed).string();
  if (fs::is_regular_file(fs::path(TRACER_CONTEXT_ENGINE_ROOT) /
                          "context_facade.hpp"))
    return fs::canonical(TRACER_CONTEXT_ENGINE_ROOT).string();
  throw std::runtime_error(
      "the packaged shared-context compiler headers are unavailable");
}
std::string context_standard_library_path() {
  const auto installed =
      module_directory() / "libclang_tracer_context_stdlib.so";
  if (fs::is_regular_file(installed))
    return fs::canonical(installed).string();
#ifdef TRACER_CONTEXT_STDLIB_PATH
  if (fs::is_regular_file(TRACER_CONTEXT_STDLIB_PATH))
    return fs::canonical(TRACER_CONTEXT_STDLIB_PATH).string();
#endif
  throw std::runtime_error(
      "the packaged private standard library is unavailable");
}
} // namespace tracer

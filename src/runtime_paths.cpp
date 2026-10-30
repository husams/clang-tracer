#include "runtime_paths.hpp"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <dlfcn.h>

#include "tracer_runtime_config.hpp"

namespace tracer::runtime {
std::vector<std::string> jit_arguments() {
  return {jit_flags.begin(), jit_flags.end()};
}

namespace {
namespace fs = std::filesystem;
#ifdef TRACER_RUNTIME_IS_LIBRARY
int module_anchor;
#endif
fs::path module_directory() {
#ifdef TRACER_RUNTIME_IS_LIBRARY
  Dl_info info{};
  if (!::dladdr(&module_anchor, &info) || !info.dli_fname) {
    throw std::runtime_error("cannot locate the tracing agent library");
  }
  return fs::canonical(info.dli_fname).parent_path();
#else
  return fs::canonical("/proc/self/exe").parent_path();
#endif
}

bool has_headers(const fs::path& path) {
  std::error_code error;
  return fs::is_regular_file(path / "include/stddef.h", error);
}
} // namespace

std::string clang_resource_dir() {
  if (const char* value = std::getenv("CLANG_TRACER_CLANG_RESOURCE_DIR"); value && *value) {
    if (!fs::path(value).is_absolute() || !has_headers(value)) {
      throw std::runtime_error("CLANG_TRACER_CLANG_RESOURCE_DIR must name an absolute Clang "
                               "resource directory containing include/stddef.h");
    }
    return value;
  }
  const auto installed = module_directory() / TRACER_RESOURCE_RELATIVE_DIR;
  if (has_headers(installed)) {
    return fs::canonical(installed).string();
  }
  if (has_headers(TRACER_BUILD_CLANG_RESOURCE_DIR)) {
    return TRACER_BUILD_CLANG_RESOURCE_DIR;
  }
  throw std::runtime_error("Clang builtin headers are unavailable; install the bundled headers or "
                           "set CLANG_TRACER_CLANG_RESOURCE_DIR in this process");
}

std::string default_agent_path() {
  if (const char* value = std::getenv("CLANG_TRACER_AGENT"); value && *value) {
    return fs::absolute(value).string();
  }
  const auto directory = module_directory();
  const auto installed = directory / TRACER_AGENT_RELATIVE_PATH;
  if (fs::is_regular_file(installed)) {
    return fs::canonical(installed).string();
  }
  const auto build = directory / "libclang_tracer_agent.so";
  if (fs::is_regular_file(build)) {
    return fs::canonical(build).string();
  }
  throw std::runtime_error("cannot find libclang_tracer_agent.so; use --agent "
                           "or CLANG_TRACER_AGENT");
}
} // namespace tracer::runtime

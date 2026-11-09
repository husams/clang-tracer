#pragma once

#include "cpp_resolver.hpp"

#include <filesystem>
#include <string>

namespace tracer::client {
clangtracer::v1::PreparedInjector write_prepared_package(
    const ResolvedCallable &, const std::string &selector,
    const std::filesystem::path &module, const std::string &digest,
    const std::filesystem::path &output, bool metadata_only,
    const std::string &translation_unit = {}, bool cl_driver = false);
}

#pragma once
#include <string>
#include <vector>

namespace tracer::runtime {
[[gnu::visibility("hidden")]] std::string clang_resource_dir();
[[gnu::visibility("hidden")]] std::string default_agent_path();
[[gnu::visibility("hidden")]] std::vector<std::string> jit_arguments();
} // namespace tracer::runtime

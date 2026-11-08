#pragma once

#include <string>
#include <vector>

#include "tracer.pb.h"

namespace tracer {

inline std::vector<std::string> primary_probe_compile_arguments(
    const clangtracer::v1::ProbeSpec &spec) {
  const auto &toolchain = spec.prepared().adapter_toolchain_options();
  std::vector<std::string> arguments(toolchain.begin(), toolchain.end());
  const auto &context = spec.header_context().compiler_arguments();
  arguments.insert(arguments.end(), context.begin(), context.end());
  const auto &receiver = spec.receiver().compiler_arguments();
  if (!receiver.empty())
    arguments.insert(arguments.end(), receiver.begin() + 1, receiver.end());
  const auto &abi = spec.prepared().adapter_abi_options();
  arguments.insert(arguments.end(), abi.begin(), abi.end());
  const auto &explicit_options = spec.compile_options();
  arguments.insert(arguments.end(), explicit_options.begin(), explicit_options.end());
  return arguments;
}

inline const std::string &primary_probe_compile_directory(
    const clangtracer::v1::ProbeSpec &spec) {
  return spec.has_header_context() ? spec.header_context().working_directory()
                                   : spec.receiver().working_directory();
}

} // namespace tracer

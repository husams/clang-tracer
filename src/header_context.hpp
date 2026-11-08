#pragma once

#include <string>
#include <vector>

#include "tracer.pb.h"

namespace tracer::client {
struct PreparedHeaderCommand {
  std::string directory;
  std::string driver;
  std::vector<std::string> arguments;
  std::vector<std::string> adapter_abi_options;
  std::string driver_mode;
  bool use_default_toolchain = true;
  std::string translation_unit;
};

// Resolves flags only. The donor implementation file is never parsed or opened.
void resolve_header_context(clangtracer::v1::ProbeSpec& spec, const std::string& database_path,
                            const std::string& translation_unit = {});

// The preparer selects the donor and records its supported toolchain settings.
PreparedHeaderCommand resolve_prepared_header_command(
    const std::string& header, const std::string& database_path,
    const std::string& translation_unit);
} // namespace tracer::client

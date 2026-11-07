#pragma once

#include <cstdint>

namespace tracer {

// Fixed layout for tracer_agent_runtime_profile_v1. Evolve it by exporting
// another version, independently of the probe request protocol.
struct AgentRuntimeProfile {
  std::uint32_t bootstrap_protocol = 0;
  std::uint32_t pointer_bytes = 0;
  std::uint32_t libstdcxx_header_release = 0;
  std::int32_t libstdcxx_default_cxx11_abi = -1;
  std::uint8_t build_id[20]{};
  char clang_version[32]{};
  char llvm_version[32]{};
  char glibc_version[32]{};
  char cxx_runtime_path[512]{};
};

} // namespace tracer

extern "C" int tracer_agent_runtime_profile_v1(
    tracer::AgentRuntimeProfile *output, std::uintptr_t size) noexcept;

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace tracer::live {

inline constexpr std::size_t max_entries = 256;
inline constexpr std::size_t max_payload_bytes = 16 * 1024 * 1024;
inline constexpr std::uint64_t agent_magic = 0x4c49564500000001ULL;

struct AgentRequest {
  enum class Action : std::uint32_t { stage, discard, status, launch, poll };
  std::uint64_t version = agent_magic;
  Action action = Action::status;
  std::uint32_t count = 0;
  std::uint64_t input = 0;
  std::uint64_t input_size = 0;
  std::uint64_t token = 0;
  std::uint64_t output = 0;
  std::uint64_t output_capacity = 0;
  std::uint64_t output_size = 0;
  std::uint32_t ready = 0;
  std::uint64_t retained = 0;
  std::uint64_t limit = 0;
  std::array<std::uint64_t, max_entries> addresses{};
  std::array<std::uint64_t, max_entries> mapping_begin{};
  std::array<std::uint64_t, max_entries> mapping_size{};
  char error[2048]{};
};

} // namespace tracer::live

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tracer::native_entry {
inline constexpr std::size_t preview_bytes = 64;

enum class Flow { ordinary, branch, call, returns };

struct Instruction {
  std::uint64_t address;
  std::size_t size;
  std::string text;
  Flow flow;
  std::optional<std::uint64_t> target;
  bool falls_through;
};

struct Preview {
  std::size_t patch_size = 0;
  std::string diagnostic;
  std::vector<Instruction> instructions;
  bool decode_failed = false;
};

Preview preview(std::uint64_t address, std::string_view bytes);
} // namespace tracer::native_entry

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
#include "tracer/protocol.hpp"

namespace tracer::native_detour {
inline constexpr std::size_t kJumpBytes = 14;
inline constexpr std::size_t kMaxDecodedBytes = kMaxPatchBytes - kJumpBytes;

struct RipRelocation {
  std::size_t displacement_offset = 0;
  std::size_t instruction_end = 0;
  std::uintptr_t target = 0;
  std::size_t access_size = 0;
};

struct Patch {
  std::uintptr_t entry = 0;
  std::array<std::uint8_t, kMaxPatchBytes> original{};
  std::size_t size = 0;
  std::vector<RipRelocation> relocations;
};

bool initialize_llvm(std::string &error);
// Publishes a plan only after every instruction and relocation is admitted.
bool decode(std::span<const std::uint8_t> bytes, std::uintptr_t entry,
            Patch &patch, std::string &error);
// Writes only on success, preserving the original bytes in patch.
bool relocate(const Patch &patch, std::uintptr_t destination,
              std::span<std::uint8_t> output, std::string &error);
// The caller owns the returned RX mapping and must retain it while in use.
bool create_trampoline(const Patch &patch, void *&address,
                       std::size_t &mapping_size, std::string &error);
} // namespace tracer::native_detour

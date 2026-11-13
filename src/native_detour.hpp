#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "tracer/compat.hpp"
#include "tracer/protocol.hpp"

namespace tracer::native_detour {
inline constexpr std::size_t kJumpBytes = 14;
inline constexpr std::size_t kRelativeJumpBytes = 5;
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
  std::size_t jump_size = kJumpBytes;
  std::vector<RipRelocation> relocations;
};

bool initialize_llvm(std::string &error);
// Publishes a plan only after every instruction and relocation is admitted.
bool decode(compat::span<const std::uint8_t> bytes, std::uintptr_t entry,
            Patch &patch, std::string &error, std::size_t jump_size = kJumpBytes);
// Writes only on success, preserving the original bytes in patch.
bool relocate(const Patch &patch, std::uintptr_t destination,
              compat::span<std::uint8_t> output, std::string &error);
// The caller owns the returned RX mapping and must retain it while in use.
bool create_trampoline(const Patch &patch, void *&address,
                       std::size_t &mapping_size, std::string &error);
// Completes a new trampoline's relay, if needed, and encodes the entry patch.
bool prepare_entry_jump(const Patch &patch, void *trampoline,
                        std::size_t mapping_size, const void *destination,
                        std::span<std::uint8_t> output, std::string &error);
// A complete integer getter uses only the SysV receiver and returns directly.
bool decode_getter(std::string_view bytes, std::size_t &trap_offset, std::string &error);
bool create_getter_copy(std::string_view bytes, std::uintptr_t caller,
                        void *&address, std::size_t &size, std::string &error);
bool prepare_call_relay(std::uintptr_t caller, void *copy, std::size_t size,
                        const void *wrapper, std::span<std::uint8_t> output,
                        std::uintptr_t &relay, std::string &error);
bool allocate_code_cache(std::uintptr_t caller, std::size_t bytes,
                         void *&address, std::size_t &size, std::string &error,
                         std::span<const RipRelocation> relocations = {});
// Width of a scalar SSE arithmetic memory operand accepted by the code cache.
std::size_t scalar_fp_load_width(std::string_view opcode);
bool code_cache_rip_relocation(std::span<const std::uint8_t> bytes,
                               std::uintptr_t pc, RipRelocation &relocation,
                               std::string &error);
} // namespace tracer::native_detour

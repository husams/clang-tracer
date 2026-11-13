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

struct ScalarStore {
  enum class Kind { move, exchange_add, compare_exchange, rmw };
  std::string base;
  std::string index;
  unsigned scale;
  std::int64_t displacement;
  Kind kind;
  std::string value_register;
  std::optional<std::uint32_t> immediate;
  std::string result_register; // Previous value returned by an AArch64 LSE operation.
  bool locked = false;

  bool supports(bool atomic) const {
    return kind == Kind::move || (kind == Kind::rmw ? atomic == locked : atomic);
  }
};

struct Instruction {
  std::uint64_t address;
  std::size_t size;
  std::string text;
  Flow flow;
  std::optional<std::uint64_t> target;
  bool falls_through;
  bool may_store;
  std::optional<ScalarStore> store32;
  bool cache_copyable;
  bool changes_receiver;
  bool changes_stack;
  bool cache_entry_copyable;
  bool indirect_near_call;
  bool cache_rip_relative;
  bool frame_store = false;
};

struct Preview {
  std::size_t patch_size = 0;
  std::string diagnostic;
  std::vector<Instruction> instructions;
  bool decode_failed = false;
};

Preview preview(std::uint64_t address, std::string_view bytes);
Preview scan(std::uint64_t address, std::string_view bytes);
#if defined(__x86_64__)
std::size_t nop_prefix(std::uint64_t address, std::string_view bytes, std::size_t minimum);
bool forwards_receiver(std::uint64_t address, std::string_view prefix);
#endif
} // namespace tracer::native_entry

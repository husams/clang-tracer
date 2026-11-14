#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include "object_code_cache_protocol.hpp"
#include "value_snapshot_storage.hpp"

namespace tracer::arm_snapshot {
inline constexpr unsigned slots = 64;
inline constexpr unsigned payload_limit = max_snapshot_wire_bytes;

struct Sample {
  std::uint64_t receiver = 0, method = 0, stack = 0, time = 0;
  std::uint32_t tid = 0, size = 0;
  bool entry = false;
  std::uint8_t depth = 0, stack_status = 0;
  std::array<object_cache::Address, object_cache::stack_limit> frames{}, stack_pointers{};
  std::array<char, payload_limit> bytes;
};

struct Queue {
  std::atomic<std::uint64_t> tail{0}, drops{0};
  struct Slot {
    std::atomic<std::uint64_t> turn{0};
    Sample sample;
  };
  std::array<Slot, slots> records;
};

struct Init {
  object_cache::Init code;
  char queue[4096]{};
  struct Range { std::uint64_t begin, end; };
  std::array<Range, 32> read_only{};
  std::size_t read_only_count = 0;
  const char *specifications = nullptr;
  std::size_t specifications_size = 0;
};
}

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>

#include "object_map_range.hpp"
#include "object_pointee_range.hpp"

namespace tracer::object_cache {
using Address = std::uint64_t;
constexpr std::size_t default_event_buffer = 4096;
constexpr std::size_t max_event_buffer = 65536;
constexpr std::size_t default_runtime_limit = 1024;
constexpr std::size_t max_runtime_limit = 65536;
constexpr std::size_t stack_limit = 64;
constexpr std::uint64_t enabled = 1ULL << 63;
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

enum class Hook : std::uint8_t {
  none, create, destroy_begin, destroy_end, destroy_both, move, xadd, cas, borrow, dispatch_end,
  owner_add, owner_remove, collection_enter, collection_suspend, collection_resume,
  collection_end, collection_bound, collection_gap, async_gap, store_fault,
  constructor_enter, constructor_return, destructor_enter, destructor_return,
  construction_aborted, construction_uncertain, owner_add_enter, owner_add_return,
  owner_get_enter, owner_get_return, owner_remove_enter, owner_remove_return, owner_gap,
  dispatch_gap, shared_mapping_gap, post_store_signal, scope_signal, snapshot_entry, snapshot_return,
  lifetime_reused
};
enum class Flow : std::uint8_t { copy, jump, conditional, runtime_call, runtime_tail };

struct Instruction {
  Address pc = 0, target = 0;
  Address boundary_pc = 0;
  std::int64_t displacement = 0;
  std::int64_t inline_receiver_offset = 0;
  std::uint32_t immediate = 0;
  std::uint8_t size = 0, condition = 0, base = 0, index = 0, scale = 0, value = 0, result = 0;
  std::uint8_t inline_receiver_register = 0;
  Hook hook = Hook::none;
  Flow flow = Flow::copy;
  bool dispatch_entry = false;
  bool scope_entry = false;
  bool inline_mutation_entry = false;
  bool collection_resume = false;
  bool uncertain_construct = false;
  bool rip_relative = false;
  bool indirect_memory = false;
  bool successor_return = false;
  std::array<std::uint8_t, 15> bytes{};
};

struct Function {
  const Instruction *instructions = nullptr;
  std::size_t count = 0;
  Address near = 0;
  Address translated = 0;
  Address resume = 0;
  Address dispatch = 0;
};

struct BorrowPlan {
  Address get_return = 0, dispatch_return = 0, typeinfo = 0;
  Address vector_begin = 0, vector_end = 0, vector_capacity = 0;
  std::uint32_t max_elements = 0;
  Address base_offset = 0;
};

struct CollectionPlan {
  Address object = 0, begin = 0, end = 0, capacity = 0;
  Address stride = 0, key_offset = 0;
  std::uint32_t key = 0, max_elements = 0, max_instances = 0;
  object_watch::MapLayout map{};
  std::uint32_t max_string_bytes = 0, map_key_size = 0;
  std::array<char, 512> map_key{};
  bool is_map = false;
  std::array<Address, object_watch::max_pointee_hops> pointer_offsets{};
  unsigned pointer_hops = 0;

  bool active() const { return stride || is_map || pointer_hops; }
};

struct SharedFileIdentity {
  std::uint64_t device = 0;
  std::uint64_t inode = 0;
};

struct Init {
  char path[4096]{};
  char module[4096]{};
  char error[512]{};
  Function *functions = nullptr;
  std::size_t function_count = 0;
  const Address *static_scopes = nullptr;
  std::size_t static_scope_count = 0;
  std::uint64_t scalar = 0;
  std::uint64_t union_tag_offset = 0;
  std::uint64_t async_signals = 0;
  Address receiver_typeinfo = 0;
  Address module_bias = 0;
  BorrowPlan borrow{};
  CollectionPlan collection{};
  SharedFileIdentity shared_file{};
  unsigned stack_depth = 0;
  std::uint32_t max_instances = 0;
  std::uint32_t writer_tid = 0;
  std::uint32_t event_buffer = 0;
  std::uint32_t serialize_wait_us = 0;
  std::uint32_t runtime_limit = 0;
  std::uint64_t max_events = 0;
  bool atomic = false, no_values = false, first_seen = false, tagged_union = false;
  bool stop_on_overflow = false;
  bool cfi_stack = false;
  bool permanent_startup_module = false;
};

struct Observation {
  Address object, pc, destination, owner;
  std::uint64_t id, generation, time;
  std::uint64_t borrow_id;
  std::uint64_t storage_generation;
  std::uint64_t shared_file_offset;
  std::uint32_t tid;
  std::uint32_t owner_index_plus_one;
  std::uint32_t runtime_module;
  Hook hook;
  std::uint8_t values, depth, stack_status;
  std::uint8_t gap;
  bool has_shared_file_offset;
  std::int32_t old_value, new_value;
  std::uint32_t old_tag, new_tag;
  std::array<Address, stack_limit> frames, stack_pointers;
};

struct Slot {
  std::atomic<std::uint64_t> turn{0};
  Observation event;
};

struct RuntimeModule {
  std::atomic<std::uint32_t> generation{0};
  Address bias = 0;
  std::array<std::uint8_t, 32> sha256{};
  std::array<char, 4096> path{};
};

struct RuntimePatch {
  Address pc = 0, gateway = 0, dispatch = 0;
  std::uint8_t size = 0;
  bool call = false;
  std::array<std::uint8_t, 15> original{};
};

struct RuntimeAdmission {
  enum Phase : unsigned { disabled, idle, ready, committing, accepted, rejected };
  std::atomic<unsigned> phase{disabled};
  Address function = 0, patches = 0;
  std::size_t count = 0, scope = 0, width = 0;
  std::uint32_t module = 0;
  Address device = 0, inode = 0, file_offset = 0;
};

struct Channel {
  enum Limit : unsigned { event_limit = 1, timeout = 2, overflow = 3 };
  alignas(64) std::atomic<std::uint64_t> activity{0};
  std::atomic<std::uint64_t> runtime_prefix_gate{0};
  std::atomic<std::uint32_t> reclaim_request{0};
  std::atomic<std::uint32_t> runtime_translations{0}, runtime_reclaimed{0};
  std::atomic<std::uint32_t> static_fragments{0}, static_reclaimed{0};
  RuntimeAdmission admission;
  alignas(64) std::atomic<std::uint64_t> tail{0}, dropped{0};
  std::atomic<std::uint64_t> executed_stores{0};
  std::atomic<std::uint64_t> matched_dispatches{0};
  std::atomic<std::uint64_t> deadline_ns{0};
  std::atomic<unsigned> capture_limit{0};
  // Aligned Slot objects follow the header in the shared mapping.
  Slot *slot(std::size_t index) noexcept {
    return reinterpret_cast<Slot *>(reinterpret_cast<std::uintptr_t>(this) +
                                    sizeof(Channel) + index * sizeof(Slot));
  }
  RuntimeModule *module(std::size_t event_buffer, std::size_t index) noexcept {
    return reinterpret_cast<RuntimeModule *>(reinterpret_cast<std::uintptr_t>(slot(event_buffer)) +
                                              index * sizeof(RuntimeModule));
  }
  std::atomic<unsigned> *scope_status(std::size_t event_buffer, std::size_t runtime_limit,
                                      std::size_t index) noexcept {
    return reinterpret_cast<std::atomic<unsigned> *>(module(event_buffer, runtime_limit)) + index;
  }
  static std::size_t mapping_size(std::size_t event_buffer, std::size_t runtime_limit) {
    return sizeof(Channel) + event_buffer * sizeof(Slot) +
        runtime_limit * (sizeof(RuntimeModule) + sizeof(std::atomic<unsigned>));
  }
};
static_assert(sizeof(Channel) % alignof(Slot) == 0);
static_assert(alignof(Slot) % alignof(RuntimeModule) == 0);

// Indices match the register order in the emitted save area. Zero denotes
// an absent address register or an immediate source.
#if defined(__aarch64__)
constexpr unsigned rax = 1, rdi = 1, rsi = 2, rbp = 30, rsp = 32;

inline std::optional<std::uint32_t> direct_branch(Address from, Address to) {
  constexpr auto reach = Address{1} << 27;
  if ((from | to) & 3 || (to >= from ? to - from >= reach : from - to > reach))
    return std::nullopt;
  return 0x14000000U | (static_cast<std::uint32_t>((to - from) >> 2) & 0x03ffffffU);
}
#else
constexpr unsigned rax = 1, rcx = 2, rdx = 3, rbx = 4, rbp = 5,
                   rsi = 6, rdi = 7, r8 = 8, r15 = 15, rsp = 16;
#endif
} // namespace tracer::object_cache

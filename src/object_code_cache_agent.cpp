#include "object_code_cache_protocol.hpp"

#include "object_code_cache_decode.hpp"
#if defined(__aarch64__)
#include "arm_snapshot_codec.hpp"
#include "arm_snapshot_protocol.hpp"
#endif

#include <fstream>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SHA256.h>

#include <algorithm>
#include <bit>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <span>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#if defined(__x86_64__)
#include <cpuid.h>
#elif defined(__aarch64__)
#include <asm/hwcap.h>
#include <asm/sigcontext.h>
#endif
#include <dlfcn.h>
#include <fcntl.h>
#include <link.h>
#include <linux/futex.h>
#include <signal.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <ucontext.h>
#include <unistd.h>

#include "linux_syscalls.hpp"
#include "native_detour.hpp"
#include "native_stack.hpp"

namespace tracer::object_cache {
namespace {

struct Registers {
#if defined(__aarch64__)
  static constexpr unsigned frame_size = 864;
  static constexpr unsigned fpsr_offset = 256, fpcr_offset = 264;
  static constexpr unsigned svcr_offset = 784, tpidr2_offset = 792, fpmr_offset = 808;
  // Bounds for records synthesized inside sigcontext.__reserved. The
  // normal save and restore path supports the architectural vector lengths.
  static constexpr unsigned max_signal_vl = 96;
  static constexpr unsigned max_signal_matrix_vl = 48;
  static constexpr unsigned signal_payload_budget = 3392;
  static constexpr unsigned scalable_vl_offset = 832;
  static constexpr unsigned signal_mask_offset = 840;
  static constexpr unsigned old_signal_mask_offset = 848;
  static constexpr unsigned signal_mask_status_offset = 856;
  std::array<Address, 31> saved;
  Address flags;
  Address value(unsigned reg) const noexcept {
    if (reg == rsp)
      return reinterpret_cast<Address>(this) + frame_size;
    return reg ? saved[reg - 1] : 0;
  }
#else
  std::array<Address, 15> saved;
  Address flags;
  Address value(unsigned reg) const noexcept {
    if (!reg)
      return 0;
    if (reg == rsp)
      return reinterpret_cast<Address>(this) + sizeof(*this) + 128;
    return saved[15 - reg];
  }
#endif
};

struct DispatchDestination {
  Address original, entry;
};
#if defined(__aarch64__)
constexpr unsigned dispatch_destination_offset = 816;
#else
constexpr unsigned tail_destination_offset = 40;
#endif

static_assert(sizeof(DispatchDestination) == 2 * sizeof(Address));

struct State;
struct SignalWindow {
  Address block_begin = 0, block_end = 0, unblock_begin = 0, pops = 0, end = 0;
#if defined(__aarch64__)
  Address spilled_begin = 0, spilled_end = 0;
  bool sme = false, sme2 = false, fpmr = false, fa64 = false;
#endif

  void relocate(Address base) noexcept {
    if (!block_begin)
      return;
    block_begin += base;
    block_end += base;
    unblock_begin += base;
    pops += base;
    end += base;
#if defined(__aarch64__)
    if (spilled_begin) {
      spilled_begin += base;
      spilled_end += base;
    }
#endif
  }
};

struct GateWindow {
  Address acquired = 0, restoring = 0, branch = 0;
  Address closed = 0, closed_branch = 0;

  void relocate(Address base) noexcept {
    acquired += base;
    restoring += base;
    branch += base;
    closed += base;
    closed_branch += base;
  }
};

struct ExitWindow {
  Address start = 0, release = 0, restoring = 0, branch = 0;
  Address original_pc = 0;

  void relocate(Address base) noexcept {
    start += base;
    release += base;
    restoring += base;
    branch += base;
  }
};

struct Site {
  Instruction instruction;
  State *watch;
  Address function, label = 0, native = 0;
  Address gate = 0;
  Address after_label = 0;
  std::uint32_t runtime_module = 0;
  std::span<const Site> runtime_sites{};
  bool runtime_prefix = false, native_scope = false;
  bool before_hook = false;
  bool reclaimable = false;
  SignalWindow before_signals{}, after_signals{};
  SignalWindow dispatch_signals{};
#if defined(__x86_64__)
  Address dispatch_branch = 0;
  SignalWindow tail_fallback_signals{};
#endif
  GateWindow entry_gate{};
  std::vector<ExitWindow> exit_gates{};
};

struct Lifetime {
  std::uint64_t id = 0, generation = 0;
  Address owner = 0;
  std::optional<std::int32_t> last_ordered_value = std::nullopt;
};

struct Mapping {
  void *address;
  std::size_t size;
  ~Mapping() { ::munmap(address, size); }
};

struct RuntimeBlock {
  std::unique_ptr<Mapping> mapping;
  Address tail_entry = 0;
  std::vector<std::unique_ptr<Mapping>> tail_gates;
  std::vector<std::unique_ptr<Mapping>> scope_bodies;
  std::vector<RuntimePatch> scope_patches;
  std::size_t scope_index = 0;
  bool partial = false;
  void *module_pin = nullptr;
  std::vector<Site> sites;
  std::string bytes;
  Address file_offset = 0;
  RuntimeBlock *previous = nullptr;

  ~RuntimeBlock() {
    if (module_pin)
      ::dlclose(module_pin);
  }
};

struct RuntimeLocation {
  Address bias = 0, end = 0, offset = 0;
  dev_t device = 0;
  ino_t inode = 0;
  std::uint64_t unload_generation = 0;
  bool main_image = false;
  Lmid_t name_space = LM_ID_BASE;
  std::string path;
};

using RuntimeImageKey = std::tuple<dev_t, ino_t, Address, std::uint64_t>;

struct RuntimeImage {
  llvm::object::OwningBinary<llvm::object::Binary> binary;
  std::uint32_t module_index = 0;

  explicit RuntimeImage(const RuntimeLocation &location);
  std::pair<std::string, Address> function(Address elf_address) const;
};

struct State {
  std::unique_ptr<Mapping> channel_mapping;
  Channel *channel;
  pid_t pid;
  Address offset;
  Address union_tag_offset;
  Address receiver_typeinfo;
  std::uint64_t async_signals;
  bool mask_async_signals = false;
  unsigned stack_depth;
  std::uint32_t max_instances = 0;
  std::uint32_t writer_tid = 0;
  std::uint32_t event_buffer = 0;
  std::uint32_t serialize_wait_us = 0;
  std::uint64_t max_events = 0;
  bool atomic, no_values, first_seen, tagged_union;
  bool stop_on_overflow;
  bool cfi_stack;
  BorrowPlan borrow;
  CollectionPlan collection;
  SharedFileIdentity shared_file;
#if defined(__aarch64__)
  std::unique_ptr<Mapping> snapshot_mapping;
  std::vector<arm_snapshot::Codec> snapshot_codecs;
#endif
  void *module_pin = nullptr;
  std::vector<std::unique_ptr<Mapping>> mappings;
  std::vector<std::unique_ptr<Mapping>> static_bodies;
  std::atomic<std::uint64_t> static_gate{enabled};
  std::atomic<bool> static_retired{false};
  std::vector<Site> sites;
  std::vector<Address> static_scopes;
  std::mutex runtime_mutex;
  std::uint32_t runtime_limit = 0, runtime_count = 0;
  std::uint32_t runtime_modules = 0;
  bool recycler_started = false;
  std::atomic<std::uint64_t> runtime_inflight{0};
  std::map<RuntimeImageKey, RuntimeImage> runtime_images;
  std::map<std::pair<RuntimeImageKey, Address>, RuntimeBlock *> runtime_targets;
  std::map<std::tuple<dev_t, ino_t, Address, Address>, RuntimeBlock *> runtime_scopes;
  std::atomic<RuntimeBlock *> runtime_blocks{nullptr};
  State *previous = nullptr;
  std::mutex mutex;
  std::map<Address, Lifetime> lifetimes;
  std::map<Address, std::atomic<std::uint64_t>> write_windows;
  std::map<Address, pid_t> active_collections;
  std::uint64_t next_id = 0;
  std::uint64_t next_borrow = 0;

  ~State() {
    if (module_pin)
      ::dlclose(module_pin);
  }
};

Address tail_body(const State &watch, Address entry) noexcept {
  for (auto *block = watch.runtime_blocks.load(std::memory_order_acquire); block; block = block->previous)
    if (block->tail_entry == entry && block->mapping)
      return reinterpret_cast<Address>(block->mapping->address);
  return 0;
}

std::atomic<State *> retained{nullptr};
std::mutex install_mutex;
struct sigaction previous_actions[NSIG]{};
std::array<bool, NSIG> bridge_installed{};

void finish_runtime_prefix(State &watch) noexcept {
  if (::getpid() == watch.pid)
    watch.channel->runtime_prefix_gate.fetch_sub(1, std::memory_order_release);
}

static_assert(sizeof(std::atomic<std::uint64_t>) == sizeof(std::uint64_t));
static_assert(sizeof(std::atomic<std::uint32_t>) == sizeof(std::uint32_t));

void reclaim_code(State *watch) noexcept {
  auto &channel = *watch->channel;
  bool release_module = watch->runtime_limit != 0;
#if defined(__aarch64__)
  release_module |= bool(watch->snapshot_mapping);
#endif
  while (!channel.reclaim_request.load(std::memory_order_acquire))
    if (::syscall(SYS_futex, &channel.reclaim_request, FUTEX_WAIT, 0,
                  nullptr, nullptr, 0) != 0 && errno != EINTR && errno != EAGAIN)
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
  // The private gate keeps inherited mappings usable in a fork child.
  watch->static_gate.fetch_and(~enabled, std::memory_order_acq_rel);
  bool static_done = false, runtime_done = false;
  RuntimeBlock *retired_modules = nullptr;
  while (!static_done || !runtime_done) {
    if (!static_done && !watch->static_gate.load(std::memory_order_acquire)) {
      watch->static_retired.store(true, std::memory_order_release);
      std::uint32_t reclaimed = 0;
      for (const auto &body : watch->static_bodies)
        if (!::munmap(body->address, body->size))
          ++reclaimed;
#if defined(__aarch64__)
      watch->snapshot_codecs.clear();
      watch->snapshot_mapping.reset();
#endif
      channel.static_reclaimed.store(reclaimed, std::memory_order_release);
      static_done = true;
    }
    if (!runtime_done && static_done) {
      const std::unique_lock lock(watch->runtime_mutex, std::try_to_lock);
      if (lock.owns_lock() && !watch->runtime_inflight.load(std::memory_order_acquire)) {
        // Keep site records valid for a signal handler already scanning the list.
        auto *block = watch->runtime_blocks.exchange(nullptr, std::memory_order_acq_rel);
        retired_modules = block;
        std::uint32_t reclaimed = 0;
        while (block) {
          if (block->mapping) {
            if (!::munmap(block->mapping->address, block->mapping->size))
              ++reclaimed;
          } else {
            bool released = true;
            for (const auto &body : block->scope_bodies)
              released &= !::munmap(body->address, body->size);
            if (released && watch->channel->scope_status(watch->event_buffer, watch->runtime_limit,
                    block->scope_index)->load(std::memory_order_acquire) == RuntimeAdmission::accepted)
              ++reclaimed;
          }
          std::string{}.swap(block->bytes);
          block = block->previous;
        }
        watch->runtime_targets.clear();
        watch->runtime_scopes.clear();
        watch->runtime_images.clear();
        channel.runtime_reclaimed.store(reclaimed, std::memory_order_release);
        runtime_done = true;
      }
    }
    if (!static_done || !runtime_done)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  for (auto *block = retired_modules; block; block = block->previous)
    if (block->module_pin)
      ::dlclose(std::exchange(block->module_pin, nullptr));
  if (watch->module_pin && release_module) {
    while (channel.activity.load(std::memory_order_acquire))
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ::dlclose(std::exchange(watch->module_pin, nullptr));
  }
}

bool enter_static_code(State &watch) noexcept {
  auto value = watch.static_gate.load(std::memory_order_acquire);
  while (value & enabled)
    if (watch.static_gate.compare_exchange_weak(value, value + 1, std::memory_order_acq_rel))
      return true;
  return false;
}

struct Sample {
  const Site *site = nullptr;
  Address stack = 0, destination = 0;
  Lifetime life{};
  std::uint32_t input = 0, expected = 0;
  std::int32_t old = 0;
  bool has_old = false;
  Address collection_owner = 0;
  std::uint64_t storage_generation = 0;
  std::optional<std::uint32_t> old_tag = std::nullopt;
  std::atomic<std::uint64_t> *write_window = nullptr;
  std::uint64_t write_epoch = 0;
  std::uint64_t reentry = 0;
  bool overlapping = false;
  std::optional<std::uint64_t> shared_file_offset = std::nullopt;
};

thread_local Sample pending;
thread_local NativeCodeRegisters pending_cfi_registers{};
thread_local std::atomic<std::uint64_t> helper_reentry{0};
thread_local Sample destroying;
struct LifetimeFrame {
  State *watch;
  Address stack, return_pc;
  bool constructor, outer, uncertain;
  Observation event;
};
thread_local std::vector<LifetimeFrame> lifecycle_frames;
struct OwnerFrame {
  State *watch;
  Address stack, return_pc, boundary, owner, incoming = 0, view = 0;
  std::vector<Address> prior;
  std::vector<std::pair<Address, Lifetime>> watched;
};
thread_local std::vector<OwnerFrame> owner_frames;
struct CollectionScope {
  State *watch = nullptr;
  Address owner = 0, entry_stack = 0, destination = 0;
  object_watch::RangeHeader header{};
  std::uint64_t id = 0, generation = 0;
  bool valid = false;
  Address mutation_return = 0, mutation_stack = 0;
  Address inline_mutation = 0;
  std::optional<std::int32_t> prior_value = std::nullopt;
};
thread_local CollectionScope collection_scope;
struct Borrowed {
  State *watch = nullptr;
  Address owner = 0, object = 0;
  Address stack = 0;
  Lifetime life;
  std::uint64_t id = 0;
};
thread_local Borrowed pending_borrow, active_borrow;
thread_local volatile sig_atomic_t inside_helper = 0;

// The low half tracks active stores. The high half advances whenever another
// store enters the window, invalidating the old sample for every active writer.
constexpr std::uint64_t write_epoch_increment = 1ULL << 32;

void reserve_write(Sample &sample, std::atomic<std::uint64_t> &window,
                   std::uint32_t wait_us) noexcept {
  using Clock = std::chrono::steady_clock;
  Clock::time_point deadline{};
  auto prior = window.load(std::memory_order_relaxed);
  std::uint64_t next;
  for (;;) {
    sample.overlapping = static_cast<std::uint32_t>(prior) != 0;
    if (wait_us && sample.overlapping) {
      const auto now = Clock::now();
      if (deadline == Clock::time_point{})
        deadline = now + std::chrono::microseconds(wait_us);
      if (now < deadline &&
          (sample.site->watch->channel->activity.load(std::memory_order_relaxed) & enabled)) {
        std::this_thread::yield();
        prior = window.load(std::memory_order_relaxed);
        continue;
      }
    }
    // Let the application proceed if the prior writer stopped making progress.
    // Entering concurrently invalidates the old value for both attempts.
    next = prior + 1 + (sample.overlapping ? write_epoch_increment : 0);
    if (window.compare_exchange_weak(prior, next, std::memory_order_acq_rel))
      break;
  }
  sample.write_window = &window;
  sample.write_epoch = next / write_epoch_increment;
}

bool release_write(const Sample &sample) noexcept {
  if (!sample.write_window)
    return false;
  const auto prior = sample.write_window->fetch_sub(1, std::memory_order_acq_rel);
  return sample.overlapping || prior / write_epoch_increment != sample.write_epoch;
}

struct ReleaseOrderedWrite {
  const Sample &sample;
  bool ordered;
  ~ReleaseOrderedWrite() {
    if (ordered)
      release_write(sample);
  }
};

void cancel_pending() noexcept {
  release_write(pending);
  pending = {};
}
thread_local std::atomic<const Site *> active_helper_site{nullptr};
static_assert(std::atomic<const Site *>::is_always_lock_free);

struct HelperGuard {
  explicit HelperGuard(const Site *site) {
    inside_helper = 1;
    active_helper_site.store(site, std::memory_order_relaxed);
  }
  ~HelperGuard() {
    active_helper_site.store(nullptr, std::memory_order_relaxed);
    inside_helper = 0;
  }
};

struct Activity {
  Channel &channel;
  bool active = false;
  explicit Activity(Channel &c) : channel(c) {
    auto value = c.activity.load(std::memory_order_acquire);
    while (value & enabled) {
      if (c.activity.compare_exchange_weak(value, value + 1,
                                            std::memory_order_acq_rel)) {
        active = true;
        return;
      }
    }
  }
  ~Activity() {
    if (active)
      channel.activity.fetch_sub(1, std::memory_order_release);
  }
};

struct PreserveErrno {
  int value = errno;
  ~PreserveErrno() { errno = value; }
};

bool read_self_memory(Address address, void *output, std::size_t size) noexcept {
  iovec local{output, size};
  iovec remote{reinterpret_cast<void *>(address), size};
  return linux_api::process_vm_readv(::getpid(), &local, 1, &remote, 1, 0) ==
         static_cast<ssize_t>(size);
}

template <typename T>
bool read_self(Address address, T &value) noexcept {
  return read_self_memory(address, &value, sizeof(T));
}

std::optional<std::uint64_t> shared_file_offset(const State &watch, Address destination) noexcept {
  try {
    std::ifstream maps("/proc/self/maps");
    for (std::string line; std::getline(maps, line);) {
      unsigned long long begin = 0, end = 0, offset = 0, inode = 0;
      unsigned device_major = 0, device_minor = 0;
      char permissions[5]{};
      if (std::sscanf(line.c_str(), "%llx-%llx %4s %llx %x:%x %llu",
                      &begin, &end, permissions, &offset, &device_major, &device_minor, &inode) != 7)
        continue;
      if (destination < begin)
        break;
      if (destination >= end)
        continue;
      if (end - destination < sizeof(std::int32_t) || permissions[0] != 'r' ||
          permissions[1] != 'w' || permissions[3] != 's' ||
          inode != watch.shared_file.inode ||
          makedev(device_major, device_minor) != watch.shared_file.device ||
          destination - begin > UINT64_MAX - offset)
        break;
      return offset + (destination - begin);
    }
  } catch (...) {
    // A missing procfs observation cannot verify any storage identity.
  }
  return std::nullopt;
}

bool application_return(const Registers &regs, Address &pc) noexcept {
#if defined(__aarch64__)
  pc = strip_return_signature(regs.value(31));
  return pc != 0;
#else
  return read_self(regs.value(rsp), pc);
#endif
}

std::optional<std::uint32_t> union_tag(const State &watch, Address object) noexcept {
  std::uint32_t tag;
  if (watch.tagged_union && !watch.no_values && object <= UINT64_MAX - watch.union_tag_offset &&
      read_self(object + watch.union_tag_offset, tag))
    return tag;
  return std::nullopt;
}

std::uint64_t monotonic_ns() noexcept {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool capture_exhausted(Channel &channel) noexcept {
  if (channel.capture_limit.load(std::memory_order_relaxed))
    return true;
  if (const auto deadline = channel.deadline_ns.load(std::memory_order_relaxed);
      deadline && monotonic_ns() >= deadline) {
    unsigned expected = 0;
    channel.capture_limit.compare_exchange_strong(expected, Channel::timeout, std::memory_order_relaxed);
    return true;
  }
  return false;
}

NativeCodeRegisters application_registers(const Registers &regs, Address pc) noexcept {
  NativeCodeRegisters result{};
#if defined(__aarch64__)
  std::copy(regs.saved.begin(), regs.saved.end(), result.begin());
  result[31] = regs.value(rsp);
#else
  constexpr std::array mapping{rax, rdx, rcx, rbx, rsi, rdi, rbp, rsp,
                               8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U};
  for (std::size_t index = 0; index < mapping.size(); ++index)
    result[index] = regs.value(mapping[index]);
#endif
  result.back() = pc;
  return result;
}

void stack(State &watch, Observation &event, const Registers &regs,
           const NativeCodeRegisters *before_store = nullptr) noexcept {
  if (!watch.stack_depth)
    return;
  if (watch.cfi_stack) {
    const auto original = before_store ? *before_store : application_registers(regs, event.pc);
    const auto depth = std::min<std::size_t>(watch.stack_depth, stack_limit);
    const auto capture = capture_native_code_stack(original, std::span(event.frames).first(depth),
                                                    std::span(event.stack_pointers).first(depth));
    event.depth = capture.count;
    event.stack_status = static_cast<std::uint8_t>(capture.status);
    return;
  }
  auto frame = regs.value(rbp);
  auto sp = regs.value(rsp);
  auto pc = event.pc;
  while (event.depth < watch.stack_depth && pc) {
    const auto index = event.depth++;
    event.frames[index] = pc;
    event.stack_pointers[index] = sp;
    if (frame < sp || frame - sp > 1024 * 1024 || frame % 8)
      break;
    std::array<Address, 2> pair;
    if (!read_self(frame, pair))
      break;
    sp = frame + 16;
    frame = pair[0];
#if defined(__aarch64__)
    pc = strip_return_signature(pair[1]);
#else
    pc = pair[1];
#endif
  }
}

void emit(State &watch, Observation &event) noexcept {
  auto &channel = *watch.channel;
  if (capture_exhausted(channel))
    return;
  auto position = channel.tail.load(std::memory_order_relaxed);
  for (;;) {
    if (watch.max_events && position >= watch.max_events) {
      unsigned expected = 0;
      channel.capture_limit.compare_exchange_strong(expected, Channel::event_limit, std::memory_order_relaxed);
      return;
    }
    auto &slot = *channel.slot(position % watch.event_buffer);
    const auto turn = slot.turn.load(std::memory_order_acquire);
    if (turn == position) {
      if (!channel.tail.compare_exchange_weak(position, position + 1,
                                               std::memory_order_relaxed))
        continue;
      event.tid = static_cast<std::uint32_t>(::syscall(SYS_gettid));
      event.time = monotonic_ns();
      slot.event = event;
      slot.turn.store(position + 1, std::memory_order_release);
      if (watch.max_events && position + 1 == watch.max_events) {
        unsigned expected = 0;
        channel.capture_limit.compare_exchange_strong(expected, Channel::event_limit, std::memory_order_relaxed);
      }
      return;
    }
    if (turn < position) {
      channel.dropped.fetch_add(1, std::memory_order_relaxed);
      if (watch.stop_on_overflow) {
        unsigned expected = 0;
        channel.capture_limit.compare_exchange_strong(expected, Channel::overflow, std::memory_order_relaxed);
      }
      return;
    }
    position = channel.tail.load(std::memory_order_relaxed);
  }
}

void emit_collection(State &watch, const Site &site, const Registers &regs,
                 const CollectionScope &scope, Hook hook, std::uint8_t gap = 0,
                 std::optional<std::int32_t> old = {}, std::optional<std::int32_t> next = {}) noexcept {
  Observation event{};
  event.object = scope.owner;
  event.pc = site.instruction.pc;
  event.id = scope.id;
  event.storage_generation = scope.generation;
  event.destination = scope.destination;
  event.hook = hook;
  event.gap = gap;
  if (old) {
    event.old_value = *old;
    event.values |= 1;
  }
  if (next) {
    event.new_value = *next;
    event.values |= 2;
  }
  stack(watch, event, regs);
  emit(watch, event);
}

bool collection_header(State &watch, const CollectionScope &scope,
                       object_watch::RangeHeader &header) noexcept {
  const auto &plan = watch.collection;
  if (plan.pointer_hops)
    return object_watch::read_pointee_chain(scope.owner,
        std::span(plan.pointer_offsets).first(plan.pointer_hops), header, read_self<Address>);
  if (plan.is_map) {
    header[2] = scope.owner + plan.map.header;
    return read_self(scope.owner + plan.map.root, header[0]) &&
           read_self(scope.owner + plan.map.count, header[1]);
  }
  return read_self(scope.owner + plan.begin, header[0]) &&
         read_self(scope.owner + plan.end, header[1]) &&
         read_self(scope.owner + plan.capacity, header[2]);
}

bool collection_destination(State &watch, const object_watch::RangeHeader &header,
                        Address &destination) noexcept {
  const auto &plan = watch.collection;
  if (plan.pointer_hops) {
    const auto pointee = header[plan.pointer_hops - 1];
    if (pointee > UINT64_MAX - watch.offset ||
        pointee + watch.offset > UINT64_MAX - (sizeof(std::int32_t) - 1))
      return false;
    destination = pointee + watch.offset;
    return true;
  }
  const auto begin = header[0], end = header[1], capacity_address = header[2];
  if (plan.is_map) {
    destination = object_watch::map_destination(plan.map, {begin, end, capacity_address}, watch.offset,
        std::string_view(plan.map_key.data(), plan.map_key_size), plan.max_elements,
        plan.max_string_bytes, read_self_memory);
    return destination != 0;
  }
  if (!plan.stride || end < begin || capacity_address < end || (!begin && end) ||
      (end - begin) % plan.stride || (capacity_address - begin) % plan.stride ||
      (end - begin) / plan.stride > plan.max_elements)
    return false;
  destination = 0;
  for (auto element = begin; element < end; element += plan.stride) {
    std::uint32_t key;
    if (!read_self(element + plan.key_offset, key))
      return false;
    if (key == plan.key) {
      if (destination)
        return false;
      destination = element + watch.offset;
    }
  }
  return destination != 0;
}

void bind_collection(State &watch, const Site &site, const Registers &regs, CollectionScope &scope) noexcept {
  object_watch::RangeHeader header{};
  Address destination = 0;
  const auto old = std::exchange(scope.prior_value, std::nullopt);
  scope.valid = false;
  if (!collection_header(watch, scope, header) || !collection_destination(watch, header, destination)) {
    scope.destination = 0;
    emit_collection(watch, site, regs, scope, Hook::collection_gap, watch.collection.pointer_hops ? 7 : 1);
    return;
  }
  if (!scope.generation || watch.collection.is_map || watch.collection.pointer_hops ||
      scope.header[0] != header[0] ||
      scope.destination != destination)
    ++scope.generation;
  scope.header = header;
  scope.destination = destination;
  scope.valid = true;
  std::int32_t next = 0;
  const bool sampled = !watch.no_values && read_self(destination, next);
  emit_collection(watch, site, regs, scope, Hook::collection_bound, 0,
                  sampled ? old : std::nullopt, sampled ? std::optional(next) : std::nullopt);
  if (!watch.no_values && !sampled)
    emit_collection(watch, site, regs, scope, Hook::collection_gap, 8);
}

void collection_boundary(State &watch, const Site &site, const Registers &regs, Hook hook) {
  auto &scope = collection_scope;
  if (hook == Hook::collection_enter) {
    const std::lock_guard lock(watch.mutex);
    if (scope.watch == &watch && scope.owner) {
      emit_collection(watch, site, regs, scope, Hook::collection_gap, 2);
      watch.active_collections.erase(scope.owner);
    }
    scope = {};
    const auto owner = regs.value(rdi);
    const auto &plan = watch.collection;
    const auto last = !plan.active() ? watch.offset :
        plan.is_map ? std::max({plan.map.root, plan.map.count, plan.map.header}) : plan.capacity;
    if (!owner || owner > UINT64_MAX - last)
      return;
    scope = {&watch, owner, regs.value(rsp), 0, {}, ++watch.next_id};
    const auto tid = static_cast<pid_t>(::syscall(SYS_gettid));
    if (watch.active_collections.size() >= plan.max_instances &&
        !watch.active_collections.count(owner)) {
      emit_collection(watch, site, regs, scope, Hook::collection_gap, 6);
      scope = {};
      return;
    }
    const auto [found, inserted] = watch.active_collections.try_emplace(owner, tid);
    if (!inserted) {
      found->second = 0;
      emit_collection(watch, site, regs, scope, Hook::collection_gap, 3);
      scope = {};
      return;
    }
    emit_collection(watch, site, regs, scope, Hook::collection_enter);
    if (plan.active())
      bind_collection(watch, site, regs, scope);
    else {
      scope.destination = owner + watch.offset;
      scope.valid = true;
    }
    return;
  }
  if (scope.watch != &watch || !scope.owner || regs.value(rsp) > scope.entry_stack)
    return;
  if (hook == Hook::collection_end) {
    const std::lock_guard lock(watch.mutex);
    watch.active_collections.erase(scope.owner);
    emit_collection(watch, site, regs, scope, Hook::collection_end);
    scope = {};
    return;
  }
  if (hook == Hook::collection_suspend) {
    const auto &instruction = site.instruction;
    const auto receiver = instruction.inline_mutation_entry ?
        regs.value(instruction.inline_receiver_register) + static_cast<Address>(instruction.inline_receiver_offset) :
        regs.value(rdi);
    if (receiver != scope.owner + watch.collection.object || scope.mutation_return || scope.inline_mutation)
      return;
    if (instruction.inline_mutation_entry) {
      scope.inline_mutation = instruction.boundary_pc;
      scope.mutation_stack = regs.value(rsp);
    } else {
      Address return_pc;
      if (!application_return(regs, return_pc) || !std::ranges::any_of(watch.sites, [&](const auto &successor) {
            return successor.instruction.collection_resume &&
                   successor.instruction.boundary_pc == instruction.boundary_pc &&
                   successor.instruction.pc == return_pc;
          }))
        return;
      scope.mutation_return = return_pc;
#if defined(__aarch64__)
      scope.mutation_stack = regs.value(rsp);
#else
      scope.mutation_stack = regs.value(rsp) + sizeof(Address);
#endif
    }
    if (scope.valid) {
      scope.valid = false;
      if (!watch.no_values) {
        object_watch::RangeHeader header{};
        Address current = 0;
        std::int32_t value = 0;
        if (!collection_header(watch, scope, header) || header != scope.header ||
            !collection_destination(watch, header, current) || current != scope.destination) {
          emit_collection(watch, site, regs, scope, Hook::collection_gap, 4);
        } else if (read_self(current, value)) {
          scope.prior_value = value;
        } else {
          emit_collection(watch, site, regs, scope, Hook::collection_gap, 5);
        }
      }
      emit_collection(watch, site, regs, scope, Hook::collection_suspend);
    }
  } else if (hook == Hook::collection_resume &&
             (scope.mutation_return == site.instruction.pc ||
              (scope.inline_mutation && scope.inline_mutation == site.instruction.boundary_pc)) &&
             scope.mutation_stack == regs.value(rsp)) {
    scope.mutation_return = 0;
    scope.inline_mutation = 0;
    const std::lock_guard lock(watch.mutex);
    if (const auto found = watch.active_collections.find(scope.owner);
        found != watch.active_collections.end() && found->second)
      bind_collection(watch, site, regs, scope);
  }
}

std::optional<Address> complete_from_base(Address view, const BorrowPlan &plan) noexcept {
  Address vptr, typeinfo;
  std::int64_t offset_to_top;
  if (!view || view <= plan.base_offset || !read_self(view, vptr) || vptr < 2 * sizeof(Address) ||
      !read_self(vptr - 2 * sizeof(Address), offset_to_top) ||
      static_cast<Address>(offset_to_top) != Address{0} - plan.base_offset ||
      !read_self(vptr - sizeof(Address), typeinfo) || typeinfo != plan.typeinfo)
    return std::nullopt;
  return view - plan.base_offset;
}

bool matches_type(Address object, Address expected_typeinfo, Address base_offset = 0) noexcept {
  BorrowPlan layout;
  layout.typeinfo = expected_typeinfo;
  layout.base_offset = base_offset;
  return object && object <= UINT64_MAX - base_offset &&
      complete_from_base(object + base_offset, layout) == object;
}

std::optional<std::pair<Address, Address>> owner_range(const BorrowPlan &plan,
                                                       Address owner) noexcept {
  Address begin, end, capacity;
  if (!owner || owner > UINT64_MAX - std::max({plan.vector_begin, plan.vector_end, plan.vector_capacity}) ||
      !read_self(owner + plan.vector_begin, begin) ||
      !read_self(owner + plan.vector_end, end) ||
      !read_self(owner + plan.vector_capacity, capacity) ||
      begin % sizeof(Address) || begin > end || end > capacity ||
      (end - begin) % sizeof(Address) || (capacity - begin) % sizeof(Address) ||
      (!begin && (end || capacity)) || (end - begin) / sizeof(Address) > plan.max_elements)
    return std::nullopt;
  return std::pair{begin, end};
}

std::optional<bool> contains(const State &watch, Address owner, Address object) noexcept {
  const auto &plan = watch.borrow;
  const auto range = owner_range(plan, owner);
  if (!range || !object || object > UINT64_MAX - plan.base_offset)
    return std::nullopt;
  for (auto slot = range->first; slot < range->second; slot += sizeof(Address)) {
    Address pointee;
    if (!read_self(slot, pointee))
      return std::nullopt;
    if (pointee == object + plan.base_offset)
      return true;
  }
  return false;
}

void emit_boundary(State &watch, const Site &site, Address object, const Lifetime &life,
                   Hook hook, std::uint64_t borrow_id = 0, std::uint32_t owner_index_plus_one = 0,
                   bool transfer = false) noexcept {
  Observation event{};
  event.owner = life.owner;
  event.object = object;
  event.pc = site.instruction.boundary_pc ? site.instruction.boundary_pc : site.function;
  event.id = life.id;
  event.generation = life.generation;
  event.borrow_id = borrow_id;
  event.owner_index_plus_one = owner_index_plus_one;
  event.hook = hook;
  event.gap = transfer;
  if (watch.shared_file.inode && object <= UINT64_MAX - watch.offset) {
    if (const auto offset = shared_file_offset(watch, object + watch.offset)) {
      event.has_shared_file_offset = true;
      event.shared_file_offset = *offset;
    }
  }
  if (hook == Hook::create && !watch.no_values && !watch.shared_file.inode &&
      read_self(object + watch.offset, event.new_value)) {
    event.values = 2;
    if (const auto tag = union_tag(watch, object)) {
      event.new_tag = *tag;
      event.values |= 8;
    }
  }
  emit(watch, event);
}

void observe_owner(State &watch, const Site &site, const Registers &regs) {
  const auto owner = regs.value(rdi);
  const auto complete = complete_from_base(regs.value(rax), watch.borrow);
  if (!complete || !contains(watch, owner, *complete).value_or(false))
    return;
  const auto object = *complete;
  const std::lock_guard lock(watch.mutex);
  const auto found = watch.lifetimes.find(object);
  if (found == watch.lifetimes.end() || !found->second.id || found->second.owner)
    return;
  auto &life = found->second;
  life.owner = owner;
  emit_boundary(watch, site, object, life, Hook::owner_add);
}

void retire_owned(State &watch, const Site &site, Address object) {
  const std::lock_guard lock(watch.mutex);
  const auto found = watch.lifetimes.find(object);
  if (found == watch.lifetimes.end() || !found->second.id)
    return; // A deleting destructor may enter the complete destructor next.
  const auto life = found->second;
  found->second.id = 0;
  found->second.owner = 0;
  emit_boundary(watch, site, object, life, Hook::destroy_begin);
  if (life.owner && contains(watch, life.owner, object) == false)
    emit_boundary(watch, site, object, life, Hook::owner_remove);
}

void observe_borrow(State &watch, const Site &site, const Registers &regs) {
  pending_borrow = {};
  Address return_pc;
  const auto owner = regs.value(rdi);
  const auto complete = complete_from_base(regs.value(rax), watch.borrow);
  if (!application_return(regs, return_pc) || return_pc != watch.borrow.get_return ||
      !complete || !contains(watch, owner, *complete).value_or(false))
    return;
  const auto object = *complete;
  const std::lock_guard lock(watch.mutex);
  const auto found = watch.lifetimes.find(object);
  if (found == watch.lifetimes.end() || !found->second.id || found->second.owner != owner)
    return;
  const auto id = ++watch.next_borrow;
  pending_borrow = {&watch, owner, object, regs.value(rsp), found->second, id};
  emit_boundary(watch, site, object, found->second, Hook::borrow, id);
}

void enter_dispatch(State &watch, const Registers &regs) {
  active_borrow = {};
  Address return_pc;
  if (!application_return(regs, return_pc) || return_pc != watch.borrow.dispatch_return)
    return;
  const auto borrowed = pending_borrow;
  pending_borrow = {};
  if (borrowed.watch != &watch || borrowed.object != regs.value(rdi) ||
      borrowed.stack != regs.value(rsp) ||
      !matches_type(borrowed.object, watch.borrow.typeinfo, watch.borrow.base_offset) ||
      !contains(watch, borrowed.owner, borrowed.object).value_or(false))
    return;
  const std::lock_guard lock(watch.mutex);
  const auto found = watch.lifetimes.find(borrowed.object);
  if (found == watch.lifetimes.end() || found->second.id != borrowed.life.id ||
      found->second.owner != borrowed.owner)
    return;
  active_borrow = borrowed;
  watch.channel->matched_dispatches.fetch_add(1, std::memory_order_relaxed);
}

std::vector<Address> owner_items(const BorrowPlan &plan, Address owner) {
  const auto range = owner_range(plan, owner);
  if (!range)
    throw std::runtime_error("owner vector layout is invalid");
  const auto size = range->second - range->first;
  std::vector<Address> items(size / sizeof(Address));
  if (size && !read_self_memory(range->first, items.data(), size))
    throw std::runtime_error("owner vector is unreadable");
  return items;
}

void ownership_boundary(State &watch, const Site &site, const Registers &regs) {
  const auto &inst = site.instruction;
  const auto sp = regs.value(rsp) - (inst.successor_return ? sizeof(Address) : 0);
  Address return_pc = inst.pc;
  if (!inst.successor_return && !application_return(regs, return_pc))
    return;
  const bool entering = inst.hook == Hook::owner_add_enter || inst.hook == Hook::owner_get_enter ||
                        inst.hook == Hook::owner_remove_enter;
  while (!owner_frames.empty()) {
    const auto &frame = owner_frames.back();
    if (frame.watch == &watch && (sp < frame.stack ||
        (!entering && sp == frame.stack && return_pc == frame.return_pc)))
      break;
    owner_frames.pop_back();
  }
  if (entering) {
    OwnerFrame frame{&watch, sp, return_pc, inst.boundary_pc, regs.value(rdi), 0, 0, {}, {}};
    try {
      if (inst.hook == Hook::owner_add_enter) {
        const auto argument = regs.value(rsi);
        Address pointee = 0;
        read_self(argument, pointee);
        const auto direct = complete_from_base(argument, watch.borrow);
        const auto indirect = complete_from_base(pointee, watch.borrow);
        if (direct && indirect && direct != indirect)
          throw std::runtime_error("owner argument has two verified object views");
        frame.incoming = direct.value_or(indirect.value_or(0));
        frame.view = direct ? argument : pointee;
        const std::lock_guard lock(watch.mutex);
        const auto found = watch.lifetimes.find(frame.incoming);
        if (found == watch.lifetimes.end() || !found->second.id)
          return;
        frame.watched.emplace_back(frame.view, found->second);
      }
      if (inst.hook != Hook::owner_get_enter)
        frame.prior = owner_items(watch.borrow, frame.owner);
      if (inst.hook == Hook::owner_remove_enter) {
        const std::lock_guard lock(watch.mutex);
        for (const auto view : frame.prior) {
          const auto complete = complete_from_base(view, watch.borrow);
          if (!complete)
            continue;
          const auto found = watch.lifetimes.find(*complete);
          if (found != watch.lifetimes.end() && found->second.id && found->second.owner == frame.owner)
            frame.watched.emplace_back(view, found->second);
        }
        if (frame.watched.empty())
          return;
      }
      owner_frames.push_back(std::move(frame));
    } catch (const std::runtime_error &) {
      emit_boundary(watch, site, 0, Lifetime{0, 0, frame.owner}, Hook::owner_gap);
    }
    return;
  }
  if (owner_frames.empty())
    return;
  const auto &top = owner_frames.back();
  if (top.watch != &watch || top.stack != sp || top.return_pc != return_pc ||
      top.boundary != inst.boundary_pc)
    return;
  auto frame = std::move(owner_frames.back());
  owner_frames.pop_back();
  try {
    if (inst.hook == Hook::owner_remove_return && !(regs.value(rax) & 0xff))
      return;
    if (inst.hook == Hook::owner_get_return) {
      const auto view = regs.value(rax);
      const auto object = complete_from_base(view, watch.borrow);
      if (!object)
        return;
      const auto after = owner_items(watch.borrow, frame.owner);
      const auto position = std::find(after.begin(), after.end(), view);
      const std::lock_guard lock(watch.mutex);
      const auto life = watch.lifetimes.find(*object);
      if (life == watch.lifetimes.end() || !life->second.id)
        return;
      if (life->second.owner != frame.owner || position == after.end() ||
          std::find(position + 1, after.end(), view) != after.end())
        throw std::runtime_error("borrowed view is outside the verified membership");
      emit_boundary(watch, site, *object, life->second, Hook::borrow, ++watch.next_borrow,
                    position - after.begin() + 1);
      return;
    }
    auto after = owner_items(watch.borrow, frame.owner);
    const std::lock_guard lock(watch.mutex);
    if (inst.hook == Hook::owner_add_return) {
      const auto life = watch.lifetimes.find(frame.incoming);
      const auto position = std::find(after.begin(), after.end(), frame.view);
      if (life == watch.lifetimes.end() || life->second.id != frame.watched.front().second.id ||
          position == after.end() || std::find(position + 1, after.end(), frame.view) != after.end())
        throw std::runtime_error("ownership change has no verified generation");
      const auto index = position - after.begin() + 1;
      after.erase(position);
      if (after != frame.prior)
        throw std::runtime_error("owner insertion did not preserve existing membership");
      if (const auto previous = life->second.owner) {
        if (previous == frame.owner || contains(watch, previous, frame.incoming) != false)
          throw std::runtime_error("previous owner still has unverified membership");
        emit_boundary(watch, site, frame.incoming, life->second, Hook::owner_remove, 0, 0, true);
      }
      life->second.owner = frame.owner;
      emit_boundary(watch, site, frame.incoming, life->second, Hook::owner_add, 0, index);
      return;
    }
    // An erase can destroy a pointee before this return. The entry sample
    // retains its lifetime and only a verified disappearance retires ownership.
    const auto difference = std::mismatch(frame.prior.begin(), frame.prior.end(),
                                          after.begin(), after.end());
    if (after.size() >= frame.prior.size() ||
        !std::equal(difference.second, after.end(),
                    difference.first + (frame.prior.size() - after.size()), frame.prior.end()))
      throw std::runtime_error("owner removal did not preserve remaining membership");
    for (const auto &[view, before] : frame.watched) {
      if (std::find(after.begin(), after.end(), view) != after.end())
        continue;
      const auto address = view - watch.borrow.base_offset;
      const auto current = watch.lifetimes.find(address);
      if (current != watch.lifetimes.end() && current->second.id == before.id) {
        if (current->second.owner != frame.owner)
          continue;
        current->second.owner = 0;
      }
      const auto index = std::find(frame.prior.begin(), frame.prior.end(), view) - frame.prior.begin() + 1;
      emit_boundary(watch, site, address, before, Hook::owner_remove, 0, index);
    }
  } catch (const std::runtime_error &) {
    emit_boundary(watch, site, 0, Lifetime{0, 0, frame.owner}, Hook::owner_gap);
  }
}

void boundary(State &watch, const Site &site, const Registers &regs, Hook kind) {
  const auto object = regs.value(rdi);
  if (kind == Hook::create && watch.shared_file.inode &&
      (object > UINT64_MAX - watch.offset || !shared_file_offset(watch, object + watch.offset)))
    return;
  if (watch.borrow.dispatch_return) {
    if (kind == Hook::destroy_begin) {
      retire_owned(watch, site, object);
      return;
    }
    if (kind == Hook::create) {
      if (!matches_type(object, watch.borrow.typeinfo, watch.borrow.base_offset))
        return;
      const std::lock_guard lock(watch.mutex);
      if (watch.max_instances && watch.lifetimes.size() >= watch.max_instances &&
          !watch.lifetimes.count(object)) {
        watch.channel->dropped.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      auto &life = watch.lifetimes[object];
      if (life.id)
        return;
      life = {++watch.next_id, life.generation + 1, 0};
      emit_boundary(watch, site, object, life, kind);
      return;
    }
  }
  Lifetime life;
  if (kind == Hook::destroy_end) {
    const auto sample = destroying;
    destroying = {};
    if (!sample.site || sample.site->watch != &watch || sample.stack != regs.value(rsp))
      return;
    life = sample.life;
  } else {
    const std::lock_guard lock(watch.mutex);
    if (kind == Hook::create) {
      if (watch.max_instances && watch.lifetimes.size() >= watch.max_instances &&
          !watch.lifetimes.count(object)) {
        watch.channel->dropped.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      auto &item = watch.lifetimes[object];
      if (item.id)
        return;
      life = {++watch.next_id, ++item.generation};
      item.id = life.id;
    } else {
      destroying = {};
      const auto found = watch.lifetimes.find(object);
      if (found == watch.lifetimes.end() || !found->second.id)
        return;
      life = found->second;
      found->second.id = 0;
      destroying = {&site, regs.value(rsp), object, life};
    }
  }
  emit_boundary(watch, site, object, life, kind);
}

void lifecycle_boundary(State &watch, const Site &site, const Registers &regs) {
  const auto &inst = site.instruction;
  const auto sp = regs.value(rsp) - (inst.successor_return ? sizeof(Address) : 0);
  Address return_pc = inst.pc;
  if (!inst.successor_return && !application_return(regs, return_pc))
    return;
  const bool entering = inst.hook == Hook::constructor_enter || inst.hook == Hook::destructor_enter;
  while (!lifecycle_frames.empty()) {
    auto &frame = lifecycle_frames.back();
    if (frame.watch == &watch && sp < frame.stack)
      break;
    if (frame.watch == &watch && sp == frame.stack && return_pc == frame.return_pc)
      break;
    if (frame.watch == &watch && frame.constructor && frame.outer) {
      frame.event.hook = Hook::construction_aborted;
      emit(watch, frame.event);
    }
    lifecycle_frames.pop_back();
  }

  if (entering) {
    const auto object = regs.value(rdi);
    if (!object || object > UINT64_MAX - watch.offset)
      return;
    const bool constructor = inst.hook == Hook::constructor_enter;
    Observation event{};
    event.object = object;
    event.pc = inst.boundary_pc;
    if (constructor && watch.shared_file.inode) {
      const auto offset = shared_file_offset(watch, object + watch.offset);
      if (!offset)
        return;
      event.has_shared_file_offset = true;
      event.shared_file_offset = *offset;
    }
    stack(watch, event, regs);
    if (!constructor) {
      const std::lock_guard lock(watch.mutex);
      const auto item = watch.lifetimes.find(object);
      if (item == watch.lifetimes.end() || !item->second.id)
        return;
      event.id = item->second.id;
      event.generation = item->second.generation;
      event.owner = item->second.owner;
      item->second.id = 0;
      item->second.owner = 0;
      event.hook = Hook::destroy_begin;
      emit(watch, event);
    }
    const bool outer = !constructor ||
        std::none_of(lifecycle_frames.begin(), lifecycle_frames.end(), [&](const auto &frame) {
          return frame.watch == &watch && frame.constructor && frame.event.object == object;
        });
    lifecycle_frames.push_back({&watch, sp, return_pc, constructor, outer,
                                inst.uncertain_construct, event});
    return;
  }

  if (lifecycle_frames.empty())
    return;
  const auto &top = lifecycle_frames.back();
  if (top.watch != &watch || top.stack != sp || top.return_pc != return_pc ||
      top.event.pc != inst.boundary_pc)
    return;
  auto frame = top;
  lifecycle_frames.pop_back();
  auto &event = frame.event;
  if (!frame.constructor) {
    event.hook = Hook::destroy_end;
    emit(watch, event);
    return;
  }
  if (!frame.outer)
    return;
  if (event.has_shared_file_offset &&
      shared_file_offset(watch, event.object + watch.offset) != event.shared_file_offset) {
    event.hook = Hook::shared_mapping_gap;
    event.destination = event.object + watch.offset;
    event.has_shared_file_offset = false;
    emit(watch, event);
    return;
  }
  if (frame.uncertain || (watch.receiver_typeinfo &&
      !matches_type(event.object, watch.receiver_typeinfo, watch.borrow.base_offset))) {
    event.hook = Hook::construction_uncertain;
    event.gap = frame.uncertain;
    emit(watch, event);
    return;
  }
  const std::lock_guard lock(watch.mutex);
  if (watch.max_instances && watch.lifetimes.size() >= watch.max_instances &&
      !watch.lifetimes.count(event.object)) {
    watch.channel->dropped.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  auto &life = watch.lifetimes[event.object];
  if (life.id) {
    event.hook = Hook::construction_uncertain;
    emit(watch, event);
    return;
  }
  life.id = ++watch.next_id;
  event.id = life.id;
  event.generation = ++life.generation;
  event.hook = Hook::create;
  if (!watch.no_values && !watch.shared_file.inode &&
      read_self(event.object + watch.offset, event.new_value)) {
    event.values = 2;
    if (const auto tag = union_tag(watch, event.object)) {
      event.new_tag = *tag;
      event.values |= 8;
    }
  }
  emit(watch, event);
}

#if defined(__aarch64__)
struct SnapshotInvocation {
  const State *watch;
  Address method, receiver, stack;
};
thread_local std::vector<SnapshotInvocation> snapshot_calls;

void snapshot_boundary(const Site &site, const Registers &regs) {
  auto &watch = *site.watch;
  auto &queue = *static_cast<arm_snapshot::Queue *>(watch.snapshot_mapping->address);
  const auto sp = regs.value(rsp);
  const auto method = site.instruction.boundary_pc;
  const bool entry = site.instruction.hook == Hook::snapshot_entry;
  Address receiver = regs.value(rdi);
  if (entry) {
    std::erase_if(snapshot_calls, [&](const auto &call) {
      return call.watch == &watch ? call.stack >= sp :
          call.watch->channel->reclaim_request.load(std::memory_order_acquire) != 0;
    });
    if (snapshot_calls.size() >= 64) {
      queue.drops.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    snapshot_calls.push_back({&watch, method, receiver, sp});
  } else {
    const auto current = std::find_if(snapshot_calls.rbegin(), snapshot_calls.rend(), [&](const auto &call) {
      return call.watch == &watch && call.method == method && call.stack == sp;
    });
    if (current == snapshot_calls.rend())
      return;
    receiver = current->receiver;
    snapshot_calls.erase(std::next(current).base());
  }
  std::string bytes;
  const auto index = site.instruction.immediate;
  if (index >= watch.snapshot_codecs.size() || !receiver ||
      !watch.snapshot_codecs[index].sample(reinterpret_cast<const void *>(receiver), bytes))
    bytes.clear();
  Observation captured{};
  if (entry && watch.stack_depth) {
    captured.pc = method;
    stack(watch, captured, regs);
  }
  auto ticket = queue.tail.load(std::memory_order_relaxed);
  for (;;) {
    auto &slot = queue.records[ticket % arm_snapshot::slots];
    const auto turn = slot.turn.load(std::memory_order_acquire);
    if (turn < ticket) {
      queue.drops.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    if (turn == ticket && queue.tail.compare_exchange_weak(ticket, ticket + 1, std::memory_order_relaxed)) {
      auto &sample = slot.sample;
      sample.entry = entry;
      sample.method = method;
      sample.receiver = receiver;
      sample.stack = sp;
      sample.tid = static_cast<std::uint32_t>(::syscall(SYS_gettid));
      sample.time = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count());
      sample.size = bytes.size();
      sample.depth = captured.depth;
      sample.stack_status = captured.stack_status;
      std::copy_n(captured.frames.begin(), captured.depth, sample.frames.begin());
      std::copy_n(captured.stack_pointers.begin(), captured.depth, sample.stack_pointers.begin());
      std::memcpy(sample.bytes.data(), bytes.data(), bytes.size());
      slot.turn.store(ticket + 1, std::memory_order_release);
      return;
    }
    ticket = queue.tail.load(std::memory_order_relaxed);
  }
}
#endif

void before(const Site *site, const Registers *regs) noexcept {
  const PreserveErrno preserve;
  auto &watch = *site->watch;
  if (::getpid() != watch.pid)
    return;
  const auto &inst = site->instruction;
  const Activity activity(*watch.channel);
  if (!activity.active || capture_exhausted(*watch.channel))
    return;
  if (inside_helper) {
    helper_reentry.fetch_add(1, std::memory_order_relaxed);
#if defined(__aarch64__)
    if (watch.snapshot_mapping) {
      auto &queue = *static_cast<arm_snapshot::Queue *>(watch.snapshot_mapping->address);
      queue.drops.fetch_add(1, std::memory_order_relaxed);
      return;
    }
#endif
    watch.channel->dropped.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  const HelperGuard guard(site);
  try {
#if defined(__aarch64__)
    if (inst.hook == Hook::snapshot_entry || inst.hook == Hook::snapshot_return) {
      snapshot_boundary(*site, *regs);
      return;
    }
#endif
    const bool store = inst.hook == Hook::move || inst.hook == Hook::xadd || inst.hook == Hook::cas;
    if (watch.writer_tid && (store || watch.first_seen) &&
        static_cast<std::uint32_t>(::syscall(SYS_gettid)) != watch.writer_tid)
      return;
    if (inst.dispatch_entry)
      enter_dispatch(watch, *regs);
    if (inst.scope_entry)
      collection_boundary(watch, *site, *regs, Hook::collection_enter);
    if (inst.inline_mutation_entry)
      collection_boundary(watch, *site, *regs, Hook::collection_suspend);
    if (inst.collection_resume)
      collection_boundary(watch, *site, *regs, Hook::collection_resume);
    if (inst.hook == Hook::none)
      return;
    if (inst.hook == Hook::dispatch_end) {
      active_borrow = {};
      return;
    }
    if (inst.hook == Hook::borrow) {
      observe_borrow(watch, *site, *regs);
      return;
    }
    if (inst.hook == Hook::owner_add) {
      observe_owner(watch, *site, *regs);
      return;
    }
    if (inst.hook >= Hook::owner_add_enter && inst.hook <= Hook::owner_remove_return) {
      ownership_boundary(watch, *site, *regs);
      return;
    }
    if (inst.hook == Hook::create || inst.hook == Hook::destroy_begin ||
        inst.hook == Hook::destroy_end) {
      boundary(watch, *site, *regs, inst.hook);
      return;
    }
    if (inst.hook == Hook::destroy_both) {
      boundary(watch, *site, *regs, Hook::destroy_begin);
      boundary(watch, *site, *regs, Hook::destroy_end);
      return;
    }
    if (inst.hook == Hook::constructor_enter || inst.hook == Hook::constructor_return ||
        inst.hook == Hook::destructor_enter || inst.hook == Hook::destructor_return) {
      lifecycle_boundary(watch, *site, *regs);
      return;
    }
    if (inst.hook == Hook::collection_enter || inst.hook == Hook::collection_suspend ||
        inst.hook == Hook::collection_end) {
      collection_boundary(watch, *site, *regs, inst.hook);
      return;
    }
    if (pending.site && pending.site->watch->serialize_wait_us)
      pending.site->watch->channel->dropped.fetch_add(1, std::memory_order_relaxed);
    cancel_pending();
    const auto destination = regs->value(inst.base) + inst.scale * regs->value(inst.index) +
                             static_cast<Address>(inst.displacement);
    if (watch.first_seen) {
      auto &scope = collection_scope;
      if (scope.watch != &watch || !scope.valid || regs->value(rsp) > scope.entry_stack)
        return;
      const std::lock_guard lock(watch.mutex);
      const auto active = watch.active_collections.find(scope.owner);
      object_watch::RangeHeader header{};
      Address current = 0;
      if (active == watch.active_collections.end() || !active->second ||
          (watch.collection.active() &&
           (!collection_header(watch, scope, header) || header != scope.header ||
            (destination == scope.destination &&
             (!collection_destination(watch, header, current) || current != destination))))) {
        scope.valid = false;
        emit_collection(watch, *site, *regs, scope, Hook::collection_gap, 4);
        return;
      }
      if (destination != scope.destination)
        return;
      Sample sample{site, regs->value(rsp), destination, {scope.id, 0, 0}};
      if (watch.cfi_stack)
        pending_cfi_registers = application_registers(*regs, inst.pc);
      sample.collection_owner = scope.owner;
      sample.storage_generation = scope.generation;
      sample.expected = regs->value(inst.result ? inst.result : rax);
      if (!watch.no_values) {
        sample.input = inst.value ? regs->value(inst.value) : inst.immediate;
        sample.has_old = !watch.atomic && read_self(destination, sample.old);
        sample.old_tag = union_tag(watch, scope.owner);
        if (!watch.atomic && !sample.has_old) {
          emit_collection(watch, *site, *regs, scope, Hook::collection_gap, 5);
          return;
        }
      }
      pending = sample;
      return;
    }
    if (destination < watch.offset)
      return;
    Lifetime life;
    std::atomic<std::uint64_t> *window = nullptr;
    if (watch.borrow.dispatch_return)
      if (active_borrow.watch != &watch || active_borrow.object != destination - watch.offset)
        return;
    {
      const std::lock_guard lock(watch.mutex);
      const auto found = watch.lifetimes.find(destination - watch.offset);
      if (found == watch.lifetimes.end() || !found->second.id)
        return;
      life = found->second;
      if (watch.borrow.dispatch_return && (life.id != active_borrow.life.id ||
                                           life.owner != active_borrow.owner))
        return;
      if (watch.serialize_wait_us || (!watch.no_values && !watch.atomic))
        window = &watch.write_windows.try_emplace(destination, 0).first->second;
    }
    Sample sample{site, regs->value(rsp), destination, life};
    if (watch.shared_file.inode) {
      sample.shared_file_offset = shared_file_offset(watch, destination);
      if (!sample.shared_file_offset)
        return;
    }
    if (watch.cfi_stack)
      pending_cfi_registers = application_registers(*regs, inst.pc);
    sample.expected = regs->value(inst.result ? inst.result : rax);
    if (window) {
      if (watch.serialize_wait_us)
        sample.reentry = helper_reentry.load(std::memory_order_relaxed);
      reserve_write(sample, *window, watch.serialize_wait_us);
    }
    if (!watch.no_values) {
      sample.input = inst.value ? regs->value(inst.value) : inst.immediate;
      if (window)
        sample.has_old = read_self(destination, sample.old);
      sample.old_tag = union_tag(watch, destination - watch.offset);
    }
    pending = sample;
  } catch (...) {
#if defined(__aarch64__)
    if (watch.snapshot_mapping) {
      auto &queue = *static_cast<arm_snapshot::Queue *>(watch.snapshot_mapping->address);
      queue.drops.fetch_add(1, std::memory_order_relaxed);
      return;
    }
#endif
    watch.channel->dropped.fetch_add(1, std::memory_order_relaxed);
  }
}

void complete(const Site *site, const Registers *regs) {
  auto &watch = *site->watch;
  const auto sample = pending;
  pending = {};
  const bool ordered = sample.write_window && sample.site->watch->serialize_wait_us;
  const ReleaseOrderedWrite release{sample, ordered};
  const bool overlapping = ordered ?
      sample.overlapping || sample.write_window->load(std::memory_order_acquire) /
          write_epoch_increment != sample.write_epoch ||
          helper_reentry.load(std::memory_order_relaxed) != sample.reentry :
      release_write(sample);
  if (sample.site != site || sample.stack != regs->value(rsp))
    return;
  const auto &inst = site->instruction;
  if (inst.hook == Hook::cas) {
    const auto succeeded = inst.result ? static_cast<std::uint32_t>(regs->value(inst.result)) == sample.expected
                                      : (regs->flags & (1 << 6)) != 0;
    if (!succeeded)
      return;
  }
  const auto object = sample.collection_owner ? sample.collection_owner : sample.destination - watch.offset;
  std::unique_lock lock(watch.mutex);
  bool drift = false;
  if (sample.collection_owner) {
    const auto active = watch.active_collections.find(object);
    if (active == watch.active_collections.end() || !active->second ||
        collection_scope.watch != &watch || !collection_scope.valid ||
        collection_scope.id != sample.life.id || collection_scope.generation != sample.storage_generation)
      return;
  } else {
    const auto found = watch.lifetimes.find(object);
    if (found == watch.lifetimes.end() || found->second.id != sample.life.id ||
      (watch.borrow.dispatch_return &&
       (active_borrow.watch != &watch || active_borrow.life.id != sample.life.id ||
        found->second.owner != active_borrow.owner)))
      return;
    if (ordered && !watch.no_values) {
      auto &last = found->second.last_ordered_value;
      if (overlapping || !sample.has_old) {
        last.reset();
      } else {
        drift = last && *last != sample.old;
        last = std::bit_cast<std::int32_t>(sample.input);
      }
    }
  }
  if (!watch.borrow.dispatch_return)
    lock.unlock();
  Observation event{};
  event.object = object;
  event.owner = sample.life.owner;
  if (watch.borrow.dispatch_return)
    event.borrow_id = active_borrow.id;
  event.pc = inst.pc;
  event.destination = sample.destination;
  event.id = sample.life.id;
  event.generation = sample.life.generation;
  event.storage_generation = sample.storage_generation;
  event.hook = inst.hook;
  event.runtime_module = site->runtime_module;
  if (sample.shared_file_offset) {
    if (shared_file_offset(watch, sample.destination) != sample.shared_file_offset) {
      event.hook = Hook::shared_mapping_gap;
      stack(watch, event, *regs, &pending_cfi_registers);
      emit(watch, event);
      return;
    }
    event.has_shared_file_offset = true;
    event.shared_file_offset = *sample.shared_file_offset;
  }
  if (!watch.no_values) {
    auto input = sample.input;
    auto old = sample.old;
    bool has_old = sample.has_old && !overlapping && !drift;
    if (inst.hook == Hook::xadd) {
      const auto returned = static_cast<std::uint32_t>(regs->value(inst.result ? inst.result : inst.value));
      old = std::bit_cast<std::int32_t>(returned);
      has_old = true;
      input += returned;
    } else if (inst.hook == Hook::cas) {
      old = std::bit_cast<std::int32_t>(sample.expected);
      has_old = true;
    }
    event.new_value = std::bit_cast<std::int32_t>(input);
    const auto next_tag = overlapping ? std::nullopt : union_tag(watch, object);
    if (!watch.atomic && has_old && old == event.new_value && sample.old_tag == next_tag)
      return;
    event.old_value = old;
    event.values = has_old ? 3 : 2;
    if (sample.old_tag && !overlapping) {
      event.old_tag = *sample.old_tag;
      event.values |= 4;
    }
    if (next_tag) {
      event.new_tag = *next_tag;
      event.values |= 8;
    }
  }
  if (drift)
    event.gap = 2;
  else if (overlapping || (ordered && !watch.no_values && !sample.has_old))
    event.gap = 1;
  stack(watch, event, *regs, &pending_cfi_registers);
  emit(watch, event);
}

void after(const Site *site, const Registers *regs) noexcept {
  const PreserveErrno preserve;
  if (::getpid() != site->watch->pid)
    return;
  auto &channel = *site->watch->channel;
  const Activity activity(channel);
  if (!activity.active) {
    if (pending.site == site)
      cancel_pending();
    return;
  }
  channel.executed_stores.fetch_add(1, std::memory_order_relaxed);
  if (inside_helper) {
    helper_reentry.fetch_add(1, std::memory_order_relaxed);
    channel.dropped.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  const HelperGuard guard(site);
  try {
    complete(site, regs);
  } catch (...) {
    channel.dropped.fetch_add(1, std::memory_order_relaxed);
  }
}

// The save prologue changes only SP before the first register is repurposed.
// An asynchronous handler can see and edit the live application context at
// these instruction boundaries. Resume by restarting the unfinished save.
std::optional<unsigned> application_prologue_sp(Address label, Address pc) noexcept {
  if (!label || pc < label)
    return std::nullopt;
  const auto offset = pc - label;
  if (offset == 0)
    return 0;
#if defined(__aarch64__)
  constexpr auto last = (1 + 15 + 1) * sizeof(std::uint32_t); // SUB, STP pairs, STR LR
  if (offset <= last && (offset & 3) == 0)
    return Registers::frame_size;
#else
  if (offset == 5) // LEA below the application red zone
    return 128;
  if (offset >= 6 && offset < 13) // PUSHFQ, then one-byte register pushes
    return 136 + 8 * (offset - 6);
  if (offset >= 13 && offset <= 29 && (offset & 1)) // REX register pushes
    return 192 + 8 * ((offset - 13) / 2);
#endif
  return std::nullopt;
}

std::optional<unsigned> entry_gate_sp(const Site &site, Address pc) noexcept {
  if (!site.reclaimable || !site.gate || pc < site.gate)
    return std::nullopt;
  switch (pc - site.gate) {
  case 0: return 0;
#if defined(__aarch64__)
  case 4:
  case 8: return 32;
#else
  case 5: return 128;
  case 6: return 136;
  case 8: return 144;
  case 9: return 152;
  case 11: return 160;
#endif
  default: return std::nullopt;
  }
}

#if defined(__aarch64__)
bool restore_arm_gate_stack(mcontext_t &machine, bool scratch_changed, bool scratch_restored) noexcept {
  if (scratch_changed) {
    const PreserveErrno preserve;
    std::array<Address, 4> saved{};
    const auto count = scratch_restored ? 2 : saved.size();
    if (!read_self_memory(machine.sp, saved.data(), count * sizeof(Address)))
      return false;
    machine.regs[0] = saved[0];
    machine.regs[1] = saved[1];
    if (!scratch_restored) {
      machine.regs[2] = saved[2];
      machine.regs[16] = saved[3];
    }
  }
  machine.sp += 32;
  return true;
}

bool restore_entry_gate(const Site &site, Address pc, mcontext_t &machine, bool &reference) noexcept {
  const auto &window = site.entry_gate;
  if (!window.branch || pc < site.gate + 12 || pc > window.closed_branch)
    return false;
  reference = pc == window.acquired ? machine.regs[2] == 0 :
      pc >= window.restoring && pc <= window.branch;
  if (pc == window.branch || pc == window.closed_branch)
    return true;
  return restore_arm_gate_stack(machine, true, pc == window.restoring + 4 || pc == window.closed + 4);
}

bool restore_exit_gate(const ExitWindow &window, Address pc, mcontext_t &machine,
                       bool &reference) noexcept {
  if (pc < window.start || pc > window.branch)
    return false;
  reference = pc < window.release || (pc == window.release && machine.regs[2] != 0);
  if (pc == window.start || pc == window.branch)
    return true;
  return restore_arm_gate_stack(machine, pc >= window.start + 12, pc == window.restoring + 4);
}

// A syscall can discard live SVE registers. Deliver a signal released by
// the generated mask restore from the saved application vector state, in
// the kernel's existing signal frame. Only the fixed record area is used.
bool restore_spilled_sve(const SignalWindow &window, Address frame, mcontext_t &machine) noexcept {
  Address vl = 0, fpsr = 0, fpcr = 0, tpidr2 = 0, fpmr = 0, svcr = 0;
  if (!read_self(frame + Registers::scalable_vl_offset, vl) || vl < 16 ||
      vl > Registers::max_signal_vl || vl % 16 ||
      !read_self(frame + Registers::fpsr_offset, fpsr) || !read_self(frame + Registers::fpcr_offset, fpcr) ||
      (window.sme && !read_self(frame + Registers::svcr_offset, svcr)) ||
      (window.sme && !read_self(frame + Registers::tpidr2_offset, tpidr2)) ||
      (window.fpmr && !read_self(frame + Registers::fpmr_offset, fpmr)))
    return false;
  fpsimd_context *fp = nullptr;
  sve_context *sve = nullptr;
  za_context *za = nullptr;
  std::uint64_t *tls = nullptr, *mode = nullptr;
  auto *records = machine.__reserved;
  constexpr auto capacity = sizeof(machine.__reserved);
  std::size_t end = 0;
  for (; end + 16 <= capacity;) {
    auto &head = *reinterpret_cast<_aarch64_ctx *>(records + end);
    if (!head.magic && !head.size)
      break;
    if (!head.magic || head.size < 16 || head.size % 16 || head.size > capacity - end ||
        head.magic == EXTRA_MAGIC)
      return false;
    if (head.magic == FPSIMD_MAGIC && head.size == sizeof(fpsimd_context))
      fp = reinterpret_cast<fpsimd_context *>(&head);
    if (head.magic == SVE_MAGIC && head.size >= sizeof(sve_context))
      sve = reinterpret_cast<sve_context *>(&head);
    if (head.magic == ZA_MAGIC && head.size == sizeof(za_context))
      za = reinterpret_cast<za_context *>(&head);
    constexpr auto tpidr2_magic = 0x54504902;
    constexpr auto fpmr_magic = 0x46504d52;
    if (head.magic == tpidr2_magic && head.size == 16)
      tls = reinterpret_cast<std::uint64_t *>(records + end + 8);
    if (head.magic == fpmr_magic && head.size == 16)
      mode = reinterpret_cast<std::uint64_t *>(records + end + 8);
    end += head.size;
  }
  const bool streaming = svcr & 1, matrix = svcr & 2;
  if (!fp || !sve || (!streaming && sve->vl != vl) || sve->flags || end + 16 > capacity ||
      (window.sme && !tls) || (window.fpmr && !mode))
    return false;
  if (matrix && (!za || za->vl < 16 || za->vl > Registers::max_signal_matrix_vl || za->vl % 16 ||
                 (streaming && za->vl != vl)))
    return false;
  const auto vq = sve_vq_from_vl(vl);
  const auto size = (SVE_SIG_CONTEXT_SIZE(vq) + 15) & ~std::size_t{15};
  const std::size_t matrix_size = matrix ? za->vl * za->vl : 0;
  const std::size_t zt_size = matrix && window.sme2 ? ZT_SIG_CONTEXT_SIZE(1) : 0;
  const auto old_sve_size = sve->head.size;
  const auto total = end + size - old_sve_size + matrix_size + zt_size + 16;
  if (total > capacity)
    return false;
  // SVE and FPSIMD contain the same low 128 bits. Linux restores those
  // bits from the FPSIMD record, so both copies need the saved value.
  std::array<std::uint8_t, 35 * Registers::max_signal_vl> spill;
  const auto vectors = frame - 35 * vl;
  const auto vector_size = SVE_SIG_REGS_SIZE(vq) - (streaming && !window.fa64 ? vl / 8 : 0);
  if (!read_self_memory(vectors, spill.data(), vector_size))
    return false;
  std::array<std::uint8_t, Registers::max_signal_matrix_vl * Registers::max_signal_matrix_vl + 64> tiles;
  if (matrix && !read_self_memory(vectors - matrix_size - (zt_size ? 64 : 0), tiles.data(),
                                  matrix_size + (zt_size ? 64 : 0)))
    return false;
  const auto increase = static_cast<std::ptrdiff_t>(size) - static_cast<std::ptrdiff_t>(old_sve_size);
  auto *following = reinterpret_cast<std::uint8_t *>(sve) + old_sve_size;
  const auto shifted = [&]<typename T>(T *record) {
    auto *bytes = reinterpret_cast<std::uint8_t *>(record);
    return record && bytes >= following ? reinterpret_cast<T *>(bytes + increase) : record;
  };
  tls = shifted(tls);
  mode = shifted(mode);
  // Linux puts FPSIMD before SVE in the fixed frame.
  if (reinterpret_cast<std::uint8_t *>(fp) >= following)
    return false;
  std::memmove(following + increase, following, records + end + 16 - following);
  std::memset(reinterpret_cast<std::uint8_t *>(sve) + SVE_SIG_REGS_OFFSET, 0, size - SVE_SIG_REGS_OFFSET);
  sve->head.size = size;
  sve->vl = vl;
  sve->flags = streaming ? SVE_SIG_FLAG_SM : 0;
  std::memcpy(reinterpret_cast<std::uint8_t *>(sve) + SVE_SIG_REGS_OFFSET, spill.data(), vector_size);
  for (unsigned index = 0; index != 32; ++index)
    std::memcpy(&fp->vregs[index], spill.data() + index * vl, sizeof(fp->vregs[index]));
  fp->fpsr = fpsr;
  fp->fpcr = fpcr;
  if (window.sme)
    *tls = tpidr2;
  if (window.fpmr)
    *mode = fpmr;
  if (matrix) {
    za = shifted(za);
    auto *following_za = reinterpret_cast<std::uint8_t *>(za) + sizeof(za_context);
    std::memmove(following_za + matrix_size + zt_size, following_za,
                 records + (static_cast<std::ptrdiff_t>(end) + increase + 16) - following_za);
    za->head.size = sizeof(za_context) + matrix_size;
    // The generated spill descends the stack in increasing row order.
    for (unsigned row = 0; row != za->vl; ++row)
      std::memcpy(following_za + row * za->vl, tiles.data() + (za->vl - row - 1) * za->vl, za->vl);
    if (zt_size) {
      auto *zt = reinterpret_cast<zt_context *>(following_za + matrix_size);
      *zt = {{ZT_MAGIC, static_cast<std::uint32_t>(zt_size)}, 1, {}};
      std::memcpy(zt + 1, tiles.data() + matrix_size, 64);
    }
  }
  return true;
}

bool spilled_signal_frame(const SignalWindow &window, Address pc) noexcept {
  return window.spilled_begin && pc >= window.spilled_begin && pc < window.spilled_end;
}

bool restore_signal_registers(const SignalWindow &window, Address pc, mcontext_t &machine,
                              bool &callback_finished) noexcept {
  const bool spilled = spilled_signal_frame(window, pc);
  if (!spilled && (!window.block_begin ||
      !((pc >= window.block_begin && pc < window.block_end) ||
        (pc >= window.unblock_begin && pc < window.end))))
    return false;
  const PreserveErrno preserve;
  const auto frame = spilled ? machine.regs[19] : machine.sp;
  Registers original{};
  if (!read_self(frame, original) || (spilled && !restore_spilled_sve(window, frame, machine)))
    return false;
  std::copy(original.saved.begin(), original.saved.end(), machine.regs);
  machine.sp = frame + Registers::frame_size;
  constexpr Address nzcv = 0xf0000000;
  machine.pstate = (machine.pstate & ~nzcv) | (original.flags & nzcv);
  callback_finished = spilled || pc >= window.unblock_begin;
  return true;
}

bool restore_dispatch(const Site &site, Address pc, mcontext_t &machine,
                      DispatchDestination &destination) noexcept {
  const auto &window = site.dispatch_signals;
  const bool spilled = spilled_signal_frame(window, pc);
  if (!spilled && (!window.unblock_begin || pc < window.unblock_begin || pc >= window.end))
    return false;
  const PreserveErrno preserve;
  const auto frame = spilled ? machine.regs[19] : machine.sp;
  if (!read_self(frame + dispatch_destination_offset, destination) || !destination.original)
    return false;
  if (site.instruction.flow == Flow::runtime_tail && destination.entry) {
    destination.entry = tail_body(*site.watch, destination.entry);
    if (!destination.entry)
      return false;
  }
  bool finished = false;
  return restore_signal_registers(window, pc, machine, finished);
}
#else
bool restore_exit_gate(const ExitWindow &window, Address pc, greg_t *machine, bool &reference) noexcept {
  if (pc < window.start || pc > window.branch)
    return false;
  reference = pc < window.release;
  if (pc == window.start || pc == window.branch)
    return true;
  unsigned adjustment = 128;
  const bool restore_flags = pc != window.start + 5 && pc != window.restoring + 3;
  const bool restore_r11 = pc >= window.start + 8 && pc < window.restoring + 2;
  const auto sp = static_cast<Address>(machine[REG_RSP]);
  if (restore_r11) {
    Address value = 0;
    const PreserveErrno preserve;
    if (!read_self(sp, value))
      return false;
    machine[REG_R11] = value;
    adjustment += sizeof(value);
  }
  if (restore_flags) {
    Address value = 0;
    const PreserveErrno preserve;
    if (!read_self(sp + adjustment - 128, value))
      return false;
    machine[REG_EFL] = value;
    adjustment += sizeof(value);
  }
  machine[REG_RSP] = sp + adjustment;
  return true;
}

bool restore_entry_gate(const Site &site, Address pc, greg_t *machine, bool &reference) noexcept {
  const auto &window = site.entry_gate;
  if (!window.branch || pc < site.gate + 11 || pc > window.closed_branch)
    return false;
  reference = pc == window.acquired ? (machine[REG_EFL] & 0x40) != 0 :
      pc >= window.restoring && pc <= window.branch;
  const auto restore = pc >= window.closed ? window.closed :
      pc >= window.restoring && pc <= window.branch ? window.restoring : 0;
  if (pc == window.branch || pc == window.closed_branch)
    return true;
  unsigned restored = 0;
  if (restore) {
    switch (pc - restore) {
    case 2: restored = 1; break;
    case 3: restored = 2; break;
    case 5: restored = 3; break;
    case 6: restored = 4; break;
    }
  }
  const PreserveErrno preserve;
  constexpr std::array<unsigned, 4> indices{REG_R10, REG_RAX, REG_R11, REG_EFL};
  std::array<Address, indices.size()> saved{};
  auto sp = static_cast<Address>(machine[REG_RSP]);
  if (restored != indices.size() &&
      !read_self_memory(sp, saved.data(), (indices.size() - restored) * sizeof(Address)))
    return false;
  for (unsigned index = restored; index < indices.size(); ++index)
    machine[indices[index]] = saved[index - restored];
  machine[REG_RSP] = sp + (indices.size() - restored) * sizeof(Address) + 128;
  return true;
}

Address signal_register_frame(const SignalWindow &window, Address pc, const greg_t *machine) noexcept {
  if (!window.block_begin)
    return 0;
  if ((pc >= window.block_begin && pc < window.block_end) ||
      (pc >= window.unblock_begin && pc < window.pops))
    return machine[REG_R12];
  if (pc >= window.pops && pc < window.end) {
    const auto offset = pc - window.pops;
    if (offset > 24 || (offset < 16 && (offset & 1)))
      return 0;
    const auto popped = offset < 16 ? offset / 2 : 8 + offset - 16;
    return static_cast<Address>(machine[REG_RSP]) - popped * sizeof(Address);
  }
  return 0;
}

bool restore_signal_registers(const SignalWindow &window, Address pc, greg_t *machine,
                              bool &callback_finished) noexcept {
  const auto frame = signal_register_frame(window, pc, machine);
  const PreserveErrno preserve;
  Registers original{};
  if (!frame || !read_self(frame, original))
    return false;
  constexpr std::array<unsigned, 16> indices{
      REG_RAX, REG_RCX, REG_RDX, REG_RBX, REG_RBP, REG_RSI, REG_RDI, REG_R8,
      REG_R9, REG_R10, REG_R11, REG_R12, REG_R13, REG_R14, REG_R15, REG_RSP};
  for (unsigned index = 0; index < indices.size() - 1; ++index)
    machine[indices[index]] = original.value(index + 1);
  machine[REG_RSP] = frame + sizeof(Registers) + 128;
  machine[REG_EFL] = original.flags;
  callback_finished = pc >= window.unblock_begin;
  return true;
}

bool restore_dispatch(const Site &site, Address pc, greg_t *machine,
                      DispatchDestination &destination) noexcept {
  if (site.instruction.flow == Flow::runtime_tail) {
    const auto contains = [pc](const SignalWindow &window) {
      return window.unblock_begin && pc >= window.unblock_begin && pc < window.pops;
    };
    const auto &window = contains(site.dispatch_signals) ? site.dispatch_signals : site.tail_fallback_signals;
    const PreserveErrno preserve;
    if (!contains(window))
      return false;
    const auto frame = signal_register_frame(window, pc, machine);
    if (!frame || !read_self(frame - tail_destination_offset, destination.original) ||
        !read_self(frame - sizeof(Address), destination.entry) || !destination.original)
      return false;
    if (destination.entry) {
      // A normal tail path enters through a stack adapter. A signal
      // return already restores the original stack, so use the body.
      destination.entry = tail_body(*site.watch, destination.entry);
      if (!destination.entry)
        return false;
    }
    bool finished = false;
    return restore_signal_registers(window, pc, machine, finished);
  }
  const auto &window = site.dispatch_signals;
  if (!window.unblock_begin || pc < window.unblock_begin || pc > site.dispatch_branch)
    return false;
  const bool at_branch = pc == site.dispatch_branch;
  const auto frame = at_branch ? 0 : signal_register_frame(window, pc, machine);
  if (!at_branch && !frame)
    return false;
  const auto app_sp = at_branch ? static_cast<Address>(machine[REG_RSP]) :
                                 frame + sizeof(Registers) + 128;
  const PreserveErrno preserve;
  if (!read_self(app_sp - sizeof(DispatchDestination), destination) || !destination.original)
    return false;
  bool finished = false;
  return at_branch || restore_signal_registers(window, pc, machine, finished);
}
#endif

void fault(int sig, siginfo_t *info, void *context) {
#if defined(__aarch64__)
  auto &machine = static_cast<ucontext_t *>(context)->uc_mcontext;
  auto &signal_registers = machine;
  auto &pc = machine.pc;
  auto &sp = machine.sp;
#else
  auto &registers = static_cast<ucontext_t *>(context)->uc_mcontext.gregs;
  auto &signal_registers = registers;
  auto &pc = registers[REG_RIP];
  auto &sp = registers[REG_RSP];
#endif
  const auto interrupted = static_cast<Address>(pc);
  const bool async = sig != SIGSEGV && sig != SIGBUS;
  const auto selected = [&](const State &watch) {
    return !async || (watch.async_signals & (1ULL << (sig - 1)));
  };
  const Site *faulting = nullptr;
  const Site *dispatching = nullptr;
  DispatchDestination destination{};
  enum class Part { instruction, before_store, after_store, entry_gate, exit_gate };
  auto part = Part::instruction;
  bool callback_finished = false;
  bool gate_reference = false;
  const ExitWindow *exit_gate = nullptr;
  std::optional<unsigned> prologue_sp;
  const Site *synthetic = async ? active_helper_site.load(std::memory_order_relaxed) : nullptr;
  if (synthetic && !selected(*synthetic->watch))
    synthetic = nullptr;
  for (auto *watch = retained.load(std::memory_order_acquire); watch && !faulting;
       watch = watch->previous) {
    if (!selected(*watch))
      continue;
    const bool static_retired = watch->static_retired.load(std::memory_order_acquire);
    const auto scan_sites = [&](std::span<const Site> sites) {
      for (const auto &site : sites) {
        if (async && restore_dispatch(site, interrupted, signal_registers, destination)) {
          dispatching = &site;
          faulting = &site;
          return;
        }
        if (const auto saved_sp = async ? entry_gate_sp(site, interrupted) : std::nullopt) {
          faulting = &site;
          prologue_sp = saved_sp;
          part = Part::entry_gate;
          return;
        }
        if (async && restore_entry_gate(site, interrupted, signal_registers, gate_reference)) {
          faulting = &site;
          part = Part::entry_gate;
          return;
        }
        if (async)
          for (const auto &window : site.exit_gates) {
            if (!window.original_pc ||
                !restore_exit_gate(window, interrupted, signal_registers, gate_reference))
              continue;
            faulting = &site;
            part = Part::exit_gate;
            exit_gate = &window;
            return;
          }
        if (static_retired && site.reclaimable)
          continue;
        if (async) {
          const bool after = restore_signal_registers(site.after_signals, interrupted, signal_registers,
                                                      callback_finished);
          if (after || restore_signal_registers(site.before_signals, interrupted, signal_registers,
                                                callback_finished)) {
            faulting = &site;
            part = after ? Part::after_store : Part::before_store;
            return;
          }
        }
        const auto before = async && site.before_hook ? application_prologue_sp(site.label, interrupted) :
                                                        std::nullopt;
        const auto after = async ? application_prologue_sp(site.after_label, interrupted) : std::nullopt;
        if ((!site.native || site.native != interrupted) && !before && !after)
          continue;
        faulting = &site;
        part = after ? Part::after_store : before ? Part::before_store : Part::instruction;
        prologue_sp = site.instruction.indirect_memory ? std::optional<unsigned>{136} : after ? after : before;
        return;
      }
    };
    const auto find_helper = [&](const Mapping &mapping, std::span<const Site> sites) {
      if (async && !faulting && !synthetic) {
        const auto base = reinterpret_cast<Address>(mapping.address);
        if (interrupted < base || interrupted - base >= mapping.size)
          return;
        for (const auto &site : sites) {
          if (site.gate == base) {
            synthetic = &site;
            return;
          }
          if (site.label >= base && site.label <= interrupted &&
              (!synthetic || synthetic->label < site.label))
            synthetic = &site;
        }
      }
    };
    scan_sites(watch->sites);
    for (const auto &mapping : watch->mappings)
      find_helper(*mapping, watch->sites);
    if (!static_retired)
      for (const auto &body : watch->static_bodies)
        find_helper(*body, watch->sites);
    for (auto *block = watch->runtime_blocks.load(std::memory_order_acquire);
         block && !faulting; block = block->previous) {
      scan_sites(block->sites);
      if (block->mapping)
        find_helper(*block->mapping, block->sites);
      if (!static_retired)
        for (const auto &body : block->scope_bodies)
          find_helper(*body, block->sites);
      for (const auto &gate : block->tail_gates)
        find_helper(*gate, block->sites);
    }
  }
  const bool resample_store = async && faulting && part != Part::after_store &&
      part != Part::entry_gate && part != Part::exit_gate &&
      (faulting->instruction.hook == Hook::move || faulting->instruction.hook == Hook::xadd ||
       faulting->instruction.hook == Hook::cas);
  const bool scope_sample = resample_store && callback_finished &&
      (faulting->instruction.scope_entry || faulting->instruction.collection_resume ||
       faulting->instruction.inline_mutation_entry || faulting->instruction.dispatch_entry);
  Address original_pc = 0;
  if (faulting) {
    original_pc = exit_gate ? exit_gate->original_pc : faulting->instruction.pc +
                  (part == Part::after_store ? faulting->instruction.size : 0);
    if (dispatching)
      original_pc = destination.original;
    pc = original_pc;
    if (prologue_sp)
      sp += *prologue_sp;
    // The handler can change registers or write the destination itself.
    // Invalidate the pre-sample before the handler runs, including re-entry.
    if (resample_store && !scope_sample && pending.site == faulting)
      cancel_pending();
    if (((!async && info->si_code > 0) || (part == Part::after_store && !callback_finished) ||
         scope_sample) &&
        pending.site == faulting) {
      const PreserveErrno preserve;
      const auto sample = pending;
      cancel_pending();
      auto &watch = *faulting->watch;
      const Activity activity(*watch.channel);
      if (activity.active && ::getpid() == watch.pid) {
        Observation event{};
        event.object = sample.collection_owner ? sample.collection_owner :
                                                 sample.destination - watch.offset;
        event.owner = sample.life.owner;
        event.pc = faulting->instruction.pc;
        event.destination = sample.destination;
        event.id = sample.life.id;
        event.generation = sample.life.generation;
        event.runtime_module = faulting->runtime_module;
        event.storage_generation = sample.storage_generation;
        if (sample.shared_file_offset) {
          event.has_shared_file_offset = true;
          event.shared_file_offset = *sample.shared_file_offset;
        }
        event.hook = part == Part::after_store ? Hook::post_store_signal :
                     scope_sample ? Hook::scope_signal : Hook::store_fault;
        event.gap = static_cast<std::uint8_t>(sig);
        emit(watch, event);
      }
    }
  }
  if (synthetic && !faulting) {
    const PreserveErrno preserve;
    auto &watch = *synthetic->watch;
    if (watch.serialize_wait_us)
      helper_reentry.fetch_add(1, std::memory_order_relaxed);
    const Activity activity(*watch.channel);
    if (activity.active && ::getpid() == watch.pid) {
      Observation event{};
      event.pc = synthetic->function;
      event.runtime_module = synthetic->runtime_module;
      event.hook = Hook::async_gap;
      event.gap = static_cast<std::uint8_t>(sig);
      emit(watch, event);
    }
  }
  const auto &previous = previous_actions[sig];
  if (previous.sa_handler == SIG_DFL) {
    // Re-execute the unchanged original instruction so the kernel delivers
    // the default action and original PC. For a raised signal, re-raise it.
    ::sigaction(sig, &previous, nullptr);
    if (info->si_code <= 0)
      ::raise(sig);
    return;
  }
  if (previous.sa_handler == SIG_IGN) {
    if (info->si_code > 0) {
      // Linux delivers a synchronous hardware fault with the default action
      // when the application has ignored its signal.
      struct sigaction action{};
      action.sa_handler = SIG_DFL;
      ::sigaction(sig, &action, nullptr);
    }
    return;
  }
  if (previous.sa_flags & SA_SIGINFO)
    previous.sa_sigaction(sig, info, context);
  else
    previous.sa_handler(sig);
  if (dispatching) {
    // CALL and BL establish the caller's return address before dispatch.
    // A tail call inherits it. Resume with the handler's register and stack edits.
    if (static_cast<Address>(pc) == destination.original)
      pc = destination.entry ? destination.entry : destination.original;
    else if (destination.entry && destination.original != destination.entry)
      dispatching->watch->runtime_inflight.fetch_sub(1, std::memory_order_release);
    return;
  }
  if (faulting) {
    if (async && static_cast<Address>(pc) == original_pc) {
      if (callback_finished) {
        pc = part == Part::after_store ? faulting->after_signals.end :
             resample_store && !scope_sample ? faulting->label : faulting->native;
        return;
      }
      switch (part) {
      case Part::after_store: pc = faulting->after_label; break;
      case Part::entry_gate: pc = gate_reference ? faulting->label : faulting->gate; break;
      case Part::exit_gate: pc = gate_reference ? exit_gate->start : exit_gate->branch; break;
      default: pc = (resample_store || prologue_sp) ? faulting->label : interrupted; break;
      }
      return;
    }
    if (async)
      cancel_pending();
    // A handler can retry, skip, or branch to another original instruction.
    // Start at the corresponding hook boundary, discarding any failed sample.
    const auto resume_sites = faulting->runtime_sites.empty() ?
        std::span<const Site>(faulting->watch->sites) : faulting->runtime_sites;
    const bool holds_static = faulting->reclaimable &&
        ((part != Part::entry_gate && part != Part::exit_gate) || gate_reference);
    for (const auto &site : resume_sites)
      if ((async || site.function == faulting->function) &&
          site.instruction.pc == static_cast<Address>(pc)) {
        if (site.reclaimable && !holds_static && !enter_static_code(*site.watch))
          break;
        if (!site.reclaimable && holds_static)
          faulting->watch->static_gate.fetch_sub(1, std::memory_order_release);
        if (faulting->runtime_prefix && !site.runtime_prefix)
          finish_runtime_prefix(*faulting->watch);
        pc = site.label;
        return;
      }
    if (faulting->runtime_prefix)
      finish_runtime_prefix(*faulting->watch);
    if (!faulting->runtime_sites.empty() && !faulting->native_scope)
      faulting->watch->runtime_inflight.fetch_sub(1, std::memory_order_release);
    if (holds_static)
      faulting->watch->static_gate.fetch_sub(1, std::memory_order_release);
  }
}

void install_bridge(std::uint64_t async_signals) {
  std::vector<int> signals{SIGSEGV, SIGBUS};
  for (int sig = 1; sig < NSIG && sig <= 64; ++sig)
    if (async_signals & (1ULL << (sig - 1)))
      signals.push_back(sig);
  std::vector<int> missing;
  for (const auto sig : signals) {
    struct sigaction current{};
    if (::sigaction(sig, nullptr, &current))
      throw std::runtime_error("cannot read application signal disposition");
    if (bridge_installed[sig]) {
      if (!(current.sa_flags & SA_SIGINFO) || current.sa_sigaction != fault)
        throw std::runtime_error("application replaced the code-cache signal handler");
    } else {
      if (current.sa_flags & SA_RESETHAND)
        throw std::runtime_error("code cache cannot chain one-shot signal handlers");
      if (sig != SIGSEGV && sig != SIGBUS &&
          (current.sa_handler == SIG_DFL || current.sa_handler == SIG_IGN))
        throw std::runtime_error("asynchronous signal needs an existing application handler");
      previous_actions[sig] = current;
      missing.push_back(sig);
    }
  }
  for (const auto sig : missing) {
    struct sigaction action{};
    action.sa_mask = previous_actions[sig].sa_mask;
    action.sa_flags = SA_SIGINFO |
        (previous_actions[sig].sa_flags & (SA_ONSTACK | SA_NODEFER | SA_RESTART));
    action.sa_sigaction = fault;
    if (::sigaction(sig, &action, nullptr)) {
      for (const auto installed : missing) {
        if (installed == sig)
          break;
        if (!::sigaction(installed, &previous_actions[installed], nullptr))
          bridge_installed[installed] = false;
      }
      throw std::runtime_error("cannot install the code-cache signal handler");
    }
    bridge_installed[sig] = true;
  }
}

Address resolve_runtime_call(const Site *site, const Registers *) noexcept;
DispatchDestination resolve_dispatch_call(const Site *site, const Registers *) noexcept;

#if defined(__aarch64__)
// This permanent thunk owns the final return after dropping the execution
// reference. It preserves the original SP and authenticated LR semantics.
#define TRACER_ARM_LEAVE_BODY(return_instruction) \
  asm volatile(".inst 0xd503245f\n" /* BTI C */ \
               "stp x0, x1, [sp, #-16]!\n" \
               "1: ldxr x0, [x16]\n" \
               "sub x0, x0, #1\n" \
               "stlxr w1, x0, [x16]\n" \
               "cbnz w1, 1b\n" \
               "ldp x0, x1, [sp], #16\n" \
               "ldp x16, x17, [sp], #16\n" \
               return_instruction)

[[gnu::naked]] void runtime_leave() { TRACER_ARM_LEAVE_BODY("ret"); }
[[gnu::naked]] void runtime_leave_a() { TRACER_ARM_LEAVE_BODY(".inst 0xd65f0bff"); }
[[gnu::naked]] void runtime_leave_b() { TRACER_ARM_LEAVE_BODY(".inst 0xd65f0fff"); }
#undef TRACER_ARM_LEAVE_BODY

struct XState {
  bool sve = (::getauxval(AT_HWCAP) & HWCAP_SVE) != 0;
  unsigned long hwcap2 = ::getauxval(AT_HWCAP2);
  bool sme = (hwcap2 & HWCAP2_SME) != 0;
  bool fa64 = (hwcap2 & HWCAP2_SME_FA64) != 0;
  bool sme2 = (hwcap2 & HWCAP2_SME2) != 0;
  bool fpmr = (hwcap2 & (1UL << 48)) != 0; // HWCAP2_FPMR
};

struct StaticLink {
  Address entry, exit;
};

class Emitter {
  static constexpr unsigned vector_offset = 272;
  static constexpr unsigned svcr_offset = vector_offset + 32 * 16;
  static constexpr unsigned tpidr2_offset = svcr_offset + 8;
  static constexpr unsigned tail_target_offset = tpidr2_offset + 8;
  static constexpr unsigned fpmr_offset = tail_target_offset + 8;
  static_assert(tail_target_offset + 16 == dispatch_destination_offset);
  static_assert(Registers::svcr_offset == svcr_offset && Registers::tpidr2_offset == tpidr2_offset &&
                Registers::fpmr_offset == fpmr_offset);
  static_assert(offsetof(Registers, flags) == 248);

  void pair(std::uint32_t opcode, unsigned reg, unsigned scaled_offset) {
    code.push_back(opcode | (scaled_offset << 15) | ((reg + 1) << 10) | (31 << 5) | reg);
  }

  void scalar(std::uint32_t opcode, unsigned reg, unsigned offset, unsigned base = 31) {
    code.push_back(opcode | ((offset / 8) << 10) | (base << 5) | reg);
  }

  std::size_t placeholder(std::uint32_t word) {
    const auto index = code.size();
    code.push_back(word);
    return index;
  }

  void local_branch(std::size_t index, std::size_t target, unsigned bits, unsigned shift) {
    const auto delta = static_cast<std::int64_t>(target) - static_cast<std::int64_t>(index);
    const auto mask = (std::uint32_t{1} << bits) - 1;
    code[index] |= (static_cast<std::uint32_t>(delta) & mask) << shift;
  }

  void address(unsigned reg, Address value) {
    code.push_back(0xd2800000 | ((value & 0xffff) << 5) | reg); // MOVZ
    for (unsigned part = 1; part != 4; ++part)
      code.push_back(0xf2800000 | (part << 21) | (((value >> (16 * part)) & 0xffff) << 5) | reg);
  }

  void sve_memory(std::uint32_t opcode, unsigned reg, unsigned base, unsigned offset) {
    code.push_back(opcode | ((offset >> 3) << 16) | ((offset & 7) << 10) | (base << 5) | reg);
  }

  void sve_predicate_base() {
    code.push_back(0x043f53f0); // ADDVL X16, SP, #31
    code.push_back(0x04305030); // ADDVL X16, X16, #1
  }

  template <typename Instructions>
  void preserve_ffr(const XState &state, Instructions emit) {
    if (state.sme && !state.fa64) {
      scalar(0xf9400000, 17, svcr_offset, 19);
      const auto skip = placeholder(0x37000011); // TBNZ X17, #0, no streaming FFR
      emit();
      local_branch(skip, code.size(), 14, 5);
    } else {
      emit();
    }
  }

  void save_sve(const XState &state) {
    code.push_back(0x910003f3); // MOV X19, SP (fixed application register frame)
    // Z0-Z31 need 32 vector lengths. P0-P15 and FFR use 17/8 more.
    // Probe each stack increment before making the next one.
    for (unsigned index = 0; index != 4; ++index) {
      code.push_back(0x043f571f); // ADDVL SP, SP, #-8
      code.push_back(0xf90003ff); // STR XZR, [SP]
    }
    code.push_back(0x043f57bf); // ADDVL SP, SP, #-3
    code.push_back(0xf90003ff);
    for (unsigned reg = 0; reg != 32; ++reg)
      sve_memory(0xe5804000, reg, 31, reg);
    sve_predicate_base();
    for (unsigned reg = 0; reg != 16; ++reg)
      sve_memory(0xe5800000, reg, 16, reg);
    preserve_ffr(state, [&] {
      code.push_back(0x2519f000); // RDFFR P0.B
      sve_memory(0xe5800000, 0, 16, 16);
    });
  }

  void restore_sve(const XState &state) {
    sve_predicate_base();
    preserve_ffr(state, [&] {
      sve_memory(0x85800000, 0, 16, 16);
      code.push_back(0x25289000); // WRFFR P0.B
    });
    for (unsigned reg = 0; reg != 16; ++reg)
      sve_memory(0x85800000, reg, 16, reg);
    for (unsigned reg = 0; reg != 32; ++reg)
      sve_memory(0x85804000, reg, 31, reg);
    code.push_back(0x9100027f); // MOV SP, X19
  }

  void vector_state(const XState &state, bool saving) {
    const auto sve = [&] {
      if (saving)
        save_sve(state);
      else
        restore_sve(state);
    };
    if (state.sve) {
      sve();
      return;
    }
    std::optional<std::size_t> done;
    if (state.sme) {
      scalar(0xf9400000, 17, svcr_offset, 19);
      const auto fpsimd = placeholder(0x36000011); // TBZ X17, #0, FPSIMD path
      sve();
      done = placeholder(0x14000000);
      local_branch(fpsimd, code.size(), 14, 5);
    }
    for (unsigned reg = 0; reg != 32; reg += 2)
      pair(saving ? 0xad000000 : 0xad400000, reg, vector_offset / 16 + reg);
    if (done)
      local_branch(*done, code.size(), 26, 0);
  }

  void save_sme(const XState &state) {
    code.push_back(0xd51bd0bf); // MSR TPIDR2_EL0, XZR
    scalar(0xf9400000, 0, svcr_offset, 19);
    const auto no_za = placeholder(0x36080000); // TBZ X0, #1, no active ZA
    if (state.sme2) {
      code.push_back(0xd10103ff); // SUB SP, SP, #64
      code.push_back(0xe13f83e0); // STR ZT0, [SP]
    }
    code.push_back(0x04bf5820); // RDSVL X0, #1
    code.push_back(0xd280000c); // MOV X12, #0
    const auto next_row = code.size();
    // Spill a row at a time. Every SP decrement is at most one streaming
    // vector length, and the store probes that stack region immediately.
    code.push_back(0xcb2063ff); // SUB SP, SP, X0
    code.push_back(0xe12003e0); // STR ZA[W12, 0], [SP]
    code.push_back(0x9100058c); // ADD X12, X12, #1
    code.push_back(0xeb00019f); // CMP X12, X0
    const auto loop = placeholder(0x54000003); // B.LO next_row
    local_branch(loop, next_row, 19, 5);
    code.push_back(0xd503447f); // SMSTOP ZA
    local_branch(no_za, code.size(), 14, 5);
    code.push_back(0xd503427f); // SMSTOP SM
  }

  void restore_sme(const XState &state) {
    code.push_back(0xd503447f); // SMSTOP ZA
    scalar(0xf9400000, 17, svcr_offset, 19);
    const auto no_za = placeholder(0x36080011); // TBZ X17, #1, no active ZA
    code.push_back(0xd503457f); // SMSTART ZA
    code.push_back(0x04bf5820); // RDSVL X0, #1
    code.push_back(0xd100040c); // SUB X12, X0, #1
    const auto previous_row = code.size();
    code.push_back(0xe10003e0); // LDR ZA[W12, 0], [SP]
    code.push_back(0x8b2063ff); // ADD SP, SP, X0
    code.push_back(0xf100058c); // SUBS X12, X12, #1
    const auto loop = placeholder(0x54000005); // B.PL previous_row
    local_branch(loop, previous_row, 19, 5);
    if (state.sme2) {
      code.push_back(0xe11f83e0); // LDR ZT0, [SP]
      code.push_back(0x910103ff); // ADD SP, SP, #64
    }
    local_branch(no_za, code.size(), 14, 5);
    const auto no_sm = placeholder(0x36000011); // TBZ X17, #0, no streaming mode
    code.push_back(0xd503437f); // SMSTART SM
    local_branch(no_sm, code.size(), 14, 5);
    scalar(0xf9400000, 0, tpidr2_offset, 19);
    code.push_back(0xd51bd0a0); // MSR TPIDR2_EL0, X0
  }

  void restore_general(unsigned stack_adjustment) {
    scalar(0xf9400000, 30, 240);
    for (unsigned reg = 0; reg != 30; reg += 2)
      pair(0xa9400000, reg, reg);
    code.push_back(0x910003ff | (stack_adjustment << 10));
  }

  void signal_mask_syscall() {
    code.push_back(0xd2800103); // MOV X3, #8 (kernel signal set size)
    code.push_back(0xd2800008 | (SYS_rt_sigprocmask << 5));
    code.push_back(0xd4000001); // SVC #0
  }

  void block_signals(const Site &site, unsigned base = 31) {
    address(0, site.watch->async_signals);
    scalar(0xf9000000, 0, Registers::signal_mask_offset, base);
    code.push_back(0xd2800000 | (SIG_BLOCK << 5)); // MOV X0, #SIG_BLOCK
    code.push_back(0x91000001 | (base << 5) | (Registers::signal_mask_offset << 10));
    code.push_back(0x91000002 | (base << 5) | (Registers::old_signal_mask_offset << 10));
    signal_mask_syscall();
    scalar(0xf9000000, 0, Registers::signal_mask_status_offset, base);
  }

  void unblock_signals(unsigned base = 31) {
    scalar(0xf9400000, 0, Registers::signal_mask_status_offset, base);
    const auto skip = placeholder(0xb5000000); // CBNZ X0, SIG_BLOCK failed
    code.push_back(0xd2800000 | (SIG_SETMASK << 5));
    code.push_back(0x91000001 | (base << 5) | (Registers::old_signal_mask_offset << 10));
    code.push_back(0xd2800002); // MOV X2, #0
    signal_mask_syscall();
    local_branch(skip, code.size(), 19, 5);
  }

  void block_sve_signals(const Site &site, const XState &xstate) {
    scalar(0xf9000000, 31, Registers::scalable_vl_offset, 19);
    std::optional<std::size_t> large_matrix, large_matrix_frame;
    code.push_back(0x04bf5020); // RDVL X0, #1
    if (xstate.sme) {
      scalar(0xf9400000, 2, svcr_offset, 19);
      const auto not_streaming = placeholder(0x36000002); // TBZ X2, #0
      code.push_back(0x04bf5820); // RDSVL X0, #1
      local_branch(not_streaming, code.size(), 14, 5);
      const auto no_za = placeholder(0x36080002); // TBZ X2, #1
      code.push_back(0x04bf5821); // RDSVL X1, #1
      code.push_back(0xf100003f | (Registers::max_signal_matrix_vl << 10)); // CMP X1, #max
      large_matrix = placeholder(0x54000008); // B.HI
      // Check the vector record plus the matrix and preserve room for
      // FPSIMD and the remaining optional records in the fixed frame.
      code.push_back(0x9b017c21); // MUL X1, X1, X1
      code.push_back(0xd2800442); // MOV X2, #34
      code.push_back(0x9b020401); // MADD X1, X0, X2, X1
      code.push_back(0xf100003f | (Registers::signal_payload_budget << 10)); // CMP X1, #budget
      large_matrix_frame = placeholder(0x54000008);
      local_branch(no_za, code.size(), 14, 5);
    }
    code.push_back(0xf100001f | (Registers::max_signal_vl << 10)); // CMP X0, #max
    const auto too_large = placeholder(0x54000008); // B.HI, signal records exceed fixed frame
    scalar(0xf9000000, 0, Registers::scalable_vl_offset, 19);
    block_signals(site, 19);
    local_branch(too_large, code.size(), 19, 5);
    if (large_matrix) {
      local_branch(*large_matrix, code.size(), 19, 5);
      local_branch(*large_matrix_frame, code.size(), 19, 5);
    }
  }

public:
  std::vector<std::uint32_t> code;
  std::size_t size() const { return code.size() * sizeof(std::uint32_t); }
  enum class Result { ignored, call_target, tail_target };

  void save_gate_context(State &watch) {
    code.push_back(0xa9be07e0); // STP X0, X1, [SP, #-32]!
    code.push_back(0xa90143e2); // STP X2, X16, [SP, #16]
    address(16, reinterpret_cast<Address>(&watch.static_gate));
  }

  void restore_gate_context() {
    code.push_back(0xa94143e2); // LDP X2, X16, [SP, #16]
    code.push_back(0xa8c207e0); // LDP X0, X1, [SP], #32
  }

  std::pair<std::size_t, std::size_t> static_entry(State &watch, GateWindow &window) {
    save_gate_context(watch);
    code.push_back(0xc85ffe00); // LDAXR X0, [X16]
    const auto closed = code.size();
    code.push_back(0xb6f80000); // TBZ X0, #63, closed
    code.push_back(0x91000401); // ADD X1, X0, #1
    code.push_back(0xc802fe01); // STLXR W2, X1, [X16]
    window.acquired = size();
    code.push_back(0x35ffff82); // CBNZ W2, .-16
    window.restoring = size();
    restore_gate_context();
    const auto body = size();
    window.branch = body;
    code.push_back(0);
    code[closed] |= static_cast<std::uint32_t>(code.size() - closed) << 5;
    code.push_back(0xd5033f5f); // CLREX
    window.closed = size();
    restore_gate_context();
    const auto fallback = size();
    window.closed_branch = fallback;
    code.push_back(0);
    return {body, fallback};
  }

  void static_exit(State &watch, ExitWindow &window) {
    window.start = size();
    save_gate_context(watch);
    code.push_back(0xc85f7e00); // LDXR X0, [X16]
    code.push_back(0xd1000401); // SUB X1, X0, #1
    code.push_back(0xc802fe01); // STLXR W2, X1, [X16]
    window.release = size();
    code.push_back(0x35ffffa2); // CBNZ W2, .-12
    window.restoring = size();
    restore_gate_context();
    window.branch = size();
  }

  void leave(State &watch, std::uint32_t instruction) {
    code.push_back(0xa9bf47f0); // STP X16, X17, [SP, #-16]!
    address(16, reinterpret_cast<Address>(&watch.runtime_inflight));
    const auto thunk = instruction == 0xd65f0bffU ? runtime_leave_a :
                       instruction == 0xd65f0fffU ? runtime_leave_b : runtime_leave;
    address(17, reinterpret_cast<Address>(thunk));
    code.push_back(0xd61f0220); // BR X17
  }

  template <typename Callback>
  std::optional<std::size_t> hook(const Site &site, Callback callback, const XState &xstate,
                                 Result result = Result::ignored, SignalWindow *window = nullptr) {
    constexpr auto frame = Registers::frame_size << 10;
    code.push_back(0xd10003ff | frame); // SUB SP, SP, #frame
    for (unsigned reg = 0; reg != 30; reg += 2)
      pair(0xa9000000, reg, reg);
    scalar(0xf9000000, 30, 240);
    code.push_back(0xd53b4200); // MRS X0, NZCV
    code.push_back(0xd53b4421); // MRS X1, FPSR
    code.push_back(0xd53b4402); // MRS X2, FPCR
    scalar(0xf9000000, 0, 248);
    // Linux syscalls can discard scalable vector state or leave streaming
    // mode. Still describe the windows where that state is fully intact.
    const bool mask_signals = window && site.watch->mask_async_signals && !xstate.sve && !xstate.sme;
    if (window && !mask_signals)
      window->block_begin = size();
    scalar(0xf9000000, 1, Registers::fpsr_offset);
    scalar(0xf9000000, 2, Registers::fpcr_offset);
    if (window) {
      if (mask_signals) {
        window->block_begin = size();
        block_signals(site);
      }
      window->block_end = size();
    }
    if (xstate.sme) {
      code.push_back(0x910003f3); // MOV X19, SP
      code.push_back(0xd53b4240); // MRS X0, SVCR
      scalar(0xf9000000, 0, svcr_offset);
      code.push_back(0xd53bd0a0); // MRS X0, TPIDR2_EL0
      scalar(0xf9000000, 0, tpidr2_offset);
    }
    vector_state(xstate, true);
    if (xstate.sme)
      save_sme(xstate);
    if (xstate.fpmr) {
      code.push_back(0xd53b4440); // MRS X0, FPMR
      scalar(0xf9000000, 0, fpmr_offset, xstate.sve || xstate.sme ? 19 : 31);
    }
    const bool mask_sve_signals = window && site.watch->mask_async_signals && xstate.sve;
    if (mask_sve_signals) {
      window->sme = xstate.sme;
      window->sme2 = xstate.sme2;
      window->fpmr = xstate.fpmr;
      window->fa64 = xstate.fa64;
      block_sve_signals(site, xstate);
    }
    address(0, reinterpret_cast<Address>(&site));
    code.push_back(xstate.sve || xstate.sme ? 0xaa1303e1 : 0x910003e1);
    address(16, reinterpret_cast<Address>(callback));
    code.push_back(0xd63f0200); // BLR X16
    const auto target_offset = result == Result::tail_target ? tail_target_offset : 16 * sizeof(Address);
    if (result == Result::call_target || (result == Result::tail_target && window)) {
      const unsigned frame_base = xstate.sve || xstate.sme ? 19 : 31;
      scalar(0xf9000000, 0, dispatch_destination_offset, frame_base);
      scalar(0xf9000000, 1, dispatch_destination_offset + sizeof(Address), frame_base);
      code.push_back(0xaa0103e0); // MOV X0, X1 (translated entry or original fallback)
    }
    if (xstate.fpmr) {
      scalar(0xf9400000, 1, fpmr_offset, xstate.sve || xstate.sme ? 19 : 31);
      code.push_back(0xd51b4441); // MSR FPMR, X1
    }
    if (result != Result::ignored && (xstate.sme || mask_sve_signals))
      scalar(0xf9000000, 0, target_offset, 19);
    if (mask_sve_signals) {
      scalar(0xf9400000, 0, Registers::scalable_vl_offset, 19);
      const auto skip = placeholder(0xb4000000); // CBZ X0, signal mask was unchanged
      window->spilled_begin = size();
      unblock_signals(19);
      code.push_back(0xd503201f); // NOP at the signal-mask syscall's return PC
      window->spilled_end = size();
      local_branch(skip, code.size(), 19, 5);
    }
    if (xstate.sme)
      restore_sme(xstate);
    vector_state(xstate, false);
    if (result != Result::ignored && !xstate.sme && !mask_sve_signals)
      scalar(0xf9000000, 0, target_offset);
    scalar(0xf9400000, 0, 248);
    scalar(0xf9400000, 1, Registers::fpsr_offset);
    scalar(0xf9400000, 2, Registers::fpcr_offset);
    code.push_back(0xd51b4200); // MSR NZCV, X0
    code.push_back(0xd51b4421); // MSR FPSR, X1
    code.push_back(0xd51b4402); // MSR FPCR, X2
    if (window) {
      window->unblock_begin = size();
      if (mask_signals)
        unblock_signals();
    }
    if (result == Result::tail_target) {
      scalar(0xf9400000, 16, tail_target_offset);
      const auto translated = placeholder(0xb5000010); // CBNZ X16, translated
      if (window)
        window->end = size();
      restore_general(Registers::frame_size);
      std::optional<std::size_t> fallback;
      if (site.instruction.value) {
        std::uint32_t branch;
        std::memcpy(&branch, site.instruction.bytes.data(), sizeof(branch));
        code.push_back(branch);
      } else {
        fallback = size();
        code.push_back(0);
      }
      local_branch(translated, code.size(), 19, 5);
      // Pass the target in IP0 and leave the original IP0 below the stack.
      // The destination adapter restores both before application execution.
      scalar(0xf9400000, 17, 16 * sizeof(Address));
      scalar(0xf9000000, 16, 16 * sizeof(Address));
      scalar(0xf9000000, 17, Registers::frame_size - 16);
      restore_general(Registers::frame_size - 16);
      code.push_back(0xd61f0200); // BR X16
      return fallback;
    }
    restore_general(Registers::frame_size);
    if (window)
      window->end = size();
    return std::nullopt;
  }
};

std::unique_ptr<Mapping> allocate_fragment(Address origin, Address resume, std::size_t size,
                                           std::size_t resume_offset) {
  const auto page = ::sysconf(_SC_PAGESIZE);
  if (page <= 0 || size > SIZE_MAX - static_cast<unsigned long>(page))
    throw std::runtime_error("invalid AArch64 code-cache mapping size");
  const auto mapping_size = (size + page - 1) / page * page;
  const auto aligned = origin / page * page;
  constexpr auto reach = Address{1} << 27;
  for (Address offset = page; offset < reach; offset += page) {
    for (auto address : {aligned + offset, aligned > offset ? aligned - offset : 0}) {
      if (!address || !direct_branch(origin, address) ||
          (resume && !direct_branch(address + resume_offset, resume)))
        continue;
      auto *const memory = ::mmap(reinterpret_cast<void *>(address), mapping_size, PROT_READ | PROT_WRITE,
          MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
      if (memory == MAP_FAILED) {
        if (errno != EEXIST && errno != EPERM && errno != ENOMEM)
          throw std::runtime_error("cannot allocate AArch64 code cache");
        continue;
      }
      if (reinterpret_cast<Address>(memory) == address)
        return std::unique_ptr<Mapping>(new Mapping{memory, mapping_size});
      ::munmap(memory, mapping_size);
      throw std::runtime_error("AArch64 code cache requires MAP_FIXED_NOREPLACE");
    }
  }
  throw std::runtime_error("no address within AArch64 code-cache branch range");
}

void make_executable(const Mapping &mapping, const Emitter &emitter) {
  auto *const code = static_cast<char *>(mapping.address);
  std::memcpy(code, emitter.code.data(), emitter.size());
  __builtin___clear_cache(code, code + emitter.size());
  if (::mprotect(mapping.address, mapping.size, PROT_READ | PROT_EXEC))
    throw std::runtime_error("cannot make AArch64 translated code executable");
}

void link_branch(Emitter &code, Address base, std::size_t offset, Address target) {
  const auto branch = direct_branch(base + offset, target);
  if (!branch)
    throw std::runtime_error("AArch64 static code-cache branch is out of range");
  code.code[offset / 4] = *branch;
}

std::unique_ptr<Mapping> make_tail_gate(Site &site, const XState &xstate) {
  Emitter out;
  const auto fallback = site.watch->async_signals ?
      out.hook(site, resolve_dispatch_call, xstate, Emitter::Result::tail_target, &site.dispatch_signals) :
      out.hook(site, resolve_runtime_call, xstate, Emitter::Result::tail_target);
  auto gate = allocate_fragment(site.instruction.pc, site.instruction.target, out.size(), fallback.value_or(0));
  site.gate = reinterpret_cast<Address>(gate->address);
  site.dispatch_signals.relocate(site.gate);
  if (fallback)
    link_branch(out, site.gate, *fallback, site.instruction.target);
  make_executable(*gate, out);
  return gate;
}

std::unique_ptr<Mapping> translate(std::span<Site> sites, Function &function, const XState &xstate,
                                   const StaticLink *links = nullptr, Address *tail_entry = nullptr) {
  Emitter out;
  std::map<Address, std::size_t> labels;
  std::vector<std::pair<std::size_t, Address>> branches;
  std::vector<std::pair<std::size_t, Address>> tail_branches;
  Address outgoing = links && function.resume ? links->exit : function.resume;
  std::size_t outgoing_offset = 0;
  for (auto &site : sites) {
    const auto &inst = site.instruction;
    if (inst.size != sizeof(std::uint32_t) || inst.rip_relative)
      throw std::runtime_error("invalid AArch64 code-cache instruction");
    site.label = out.size();
    labels.emplace(inst.pc, site.label);
    if (inst.flow == Flow::runtime_tail) {
      site.native = out.size();
      tail_branches.emplace_back(site.native, site.gate);
      out.code.push_back(0);
      continue;
    }
    if (inst.flow == Flow::runtime_call) {
      out.hook(site, resolve_dispatch_call, xstate, Emitter::Result::call_target,
               site.watch->async_signals ? &site.dispatch_signals : nullptr);
      if (!inst.value) {
        if (outgoing)
          throw std::runtime_error("AArch64 call gateway has two original continuations");
        // The direct fallback keeps the original branch type for BTI.
        out.code.push_back(0xb5000050); // CBNZ X16, .+8
        outgoing = inst.target;
        outgoing_offset = out.size();
        out.code.push_back(0);
      }
      out.code.push_back(0xd61f0200); // BR X16
      continue;
    }
    site.before_hook = inst.hook != Hook::none || inst.scope_entry || inst.collection_resume ||
                       inst.inline_mutation_entry;
    if (site.before_hook)
      out.hook(site, before, xstate, Emitter::Result::ignored,
               site.watch->async_signals ? &site.before_signals : nullptr);
    site.native = out.size();
    std::uint32_t original;
    std::memcpy(&original, inst.bytes.data(), sizeof(original));
    if ((links || !site.runtime_sites.empty()) &&
        (original == 0xd65f03c0U || original == 0xd65f0bffU || original == 0xd65f0fffU)) {
      if (links) {
        outgoing = links->exit;
        outgoing_offset = out.size();
        out.code.push_back(0);
      } else {
        out.leave(*site.watch, original);
      }
      continue;
    }
    if (inst.flow != Flow::copy)
      branches.emplace_back(out.code.size(), inst.target);
    out.code.push_back(original);
    if (inst.hook == Hook::move || inst.hook == Hook::xadd || inst.hook == Hook::cas) {
      site.after_label = out.size();
      out.hook(site, after, xstate, Emitter::Result::ignored,
               site.watch->async_signals ? &site.after_signals : nullptr);
    }
  }
  if (function.resume) {
    outgoing_offset = out.size();
    out.code.push_back(0);
  }
  const auto adapter = out.size();
  if (tail_entry) {
    out.code.push_back(0xf84107f0); // LDR X16, [SP], #16
    out.code.push_back(0);
  }
  auto map = allocate_fragment(links ? links->entry : function.near, outgoing, out.size(), outgoing_offset);
  const auto base = reinterpret_cast<Address>(map->address);
  if (outgoing)
    out.code[outgoing_offset / 4] = *direct_branch(base + outgoing_offset, outgoing);
  for (const auto &[offset, target] : tail_branches)
    link_branch(out, base, offset, target);
  if (tail_entry) {
    link_branch(out, base, adapter + sizeof(std::uint32_t), base);
    *tail_entry = base + adapter;
  }
  for (const auto &[index, target] : branches) {
    auto &word = out.code[index];
    const auto destination = labels.at(target);
    if ((word & 0xfc000000U) == 0x14000000U) {
      const auto branch = direct_branch(base + index * 4, base + destination);
      if (!branch)
        throw std::runtime_error("AArch64 translated jump is out of range");
      word = *branch;
    } else {
      const unsigned bits = (word & 0x7e000000U) == 0x36000000U ? 14 : 19;
      const auto span = std::int64_t{1} << (bits - 1);
      const auto delta = static_cast<std::int64_t>(destination / 4) - static_cast<std::int64_t>(index);
      if (delta < -span || delta >= span)
        throw std::runtime_error("AArch64 translated conditional branch is out of range");
      const auto mask = (std::uint32_t{1} << bits) - 1;
      word = (word & ~(mask << 5)) | ((static_cast<std::uint32_t>(delta) & mask) << 5);
    }
  }
  make_executable(*map, out);
  for (auto &site : sites) {
    site.label += base;
    if (site.after_label)
      site.after_label += base;
    site.before_signals.relocate(base);
    site.after_signals.relocate(base);
    if (site.instruction.flow != Flow::runtime_tail)
      site.dispatch_signals.relocate(base);
    if (site.instruction.flow != Flow::runtime_call)
      site.native += base;
  }
  return map;
}

void install_static_fragment(State &watch, std::span<Site> sites, Function &function,
                             const XState &xstate, RuntimeBlock *scope = nullptr) {
  auto &gates = scope ? scope->tail_gates : watch.mappings;
  auto &bodies = scope ? scope->scope_bodies : watch.static_bodies;
  if (sites.size() != 1 || sites.front().instruction.flow != Flow::copy) {
    auto mapping = translate(sites, function, xstate);
    function.translated = reinterpret_cast<Address>(mapping->address);
    gates.push_back(std::move(mapping));
    return;
  }
  auto &site = sites.front();
  Emitter gate;
  const auto [body_branch, fallback_branch] = gate.static_entry(watch, site.entry_gate);
  const auto exit = gate.size();
  auto &exit_window = site.exit_gates.emplace_back();
  exit_window.original_pc = function.resume ? function.resume : site.instruction.pc;
  gate.static_exit(watch, exit_window);
  const auto continuation_branch = gate.size();
  std::uint32_t terminal = 0;
  if (!function.resume) {
    std::memcpy(&terminal, site.instruction.bytes.data(), sizeof(terminal));
    if (terminal != 0xd65f03c0U && terminal != 0xd65f0bffU && terminal != 0xd65f0fffU)
      throw std::runtime_error("AArch64 static fragment has no verified continuation");
  }
  gate.code.push_back(terminal);
  auto mapping = allocate_fragment(function.near, site.instruction.pc, gate.size(), fallback_branch);
  const auto entry = reinterpret_cast<Address>(mapping->address);
  const StaticLink links{entry, entry + exit};
  auto body = translate(sites, function, xstate, &links);
  link_branch(gate, entry, body_branch, reinterpret_cast<Address>(body->address));
  link_branch(gate, entry, fallback_branch, site.instruction.pc);
  if (function.resume)
    link_branch(gate, entry, continuation_branch, function.resume);
  make_executable(*mapping, gate);
  site.gate = entry;
  site.entry_gate.relocate(entry);
  exit_window.relocate(entry);
  site.reclaimable = true;
  function.translated = entry;
  bodies.push_back(std::move(body));
  gates.push_back(std::move(mapping));
}

#else
[[gnu::naked]] void runtime_leave() {
  asm volatile("lea -128(%rsp), %rsp\n"
               "pushfq\n"
               "push %r11\n"
               "mov 136(%rsp), %r11\n"
               "lock decq (%r11)\n"
               "pop %r11\n"
               "popfq\n"
               "lea 128(%rsp), %rsp\n"
               "ret");
}

struct XState {
  std::uint32_t size = 512;
  std::uint32_t low = 0, high = 0;
  bool xsave = false;

  XState() {
    unsigned a, b, c, d;
    if (__get_cpuid(1, &a, &b, &c, &d) && (c & bit_OSXSAVE)) {
      asm volatile("xgetbv" : "=a"(low), "=d"(high) : "c"(0));
      __cpuid_count(0xd, 0, a, b, c, d);
      if (b < 576 || b > 65536)
        throw std::runtime_error("unsupported XSAVE area size");
      size = (b + 63) & ~63U;
      xsave = true;
    }
  }
};

class Emitter {
public:
  std::vector<std::uint8_t> code;
  void bytes(std::initializer_list<std::uint8_t> values) {
    code.insert(code.end(), values);
  }
  template <typename T> void immediate(T value) {
    const auto *data = reinterpret_cast<const std::uint8_t *>(&value);
    code.insert(code.end(), data, data + sizeof(value));
  }

  enum class Result { ignored, dispatch_target, prefix_open, tail_target };

  std::size_t absolute_jump(Address target) {
    bytes({0x3e, 0xff, 0x25, 0, 0, 0, 0}); // notrack jmp *0(%rip)
    const auto operand = code.size();
    immediate(target);
    return operand;
  }

  void reference_address(State &watch) {
    bytes({0x49, 0xbb}); // MOVABS R11, &static_gate
    immediate(reinterpret_cast<Address>(&watch.static_gate));
  }

  void save_gate_context() {
    bytes({0x48, 0x8d, 0x64, 0x24, 0x80, 0x9c, 0x41, 0x53}); // LEA; PUSHFQ; PUSH R11
  }

  void restore_gate_context() {
    bytes({0x41, 0x5b, 0x9d, 0x48, 0x8d, 0xa4, 0x24, 0x80, 0, 0, 0});
  }

  std::size_t static_entry(State &watch, Address fallback, GateWindow &window) {
    save_gate_context();
    bytes({0x50, 0x41, 0x52}); // PUSH RAX; PUSH R10
    reference_address(watch);
    bytes({0x49, 0x8b, 0x03}); // MOV RAX, [R11]
    const auto retry = code.size();
    bytes({0x48, 0x85, 0xc0, 0x0f, 0x89}); // TEST RAX, RAX; JNS closed
    const auto closed = code.size();
    immediate(0U);
    bytes({0x4c, 0x8d, 0x50, 0x01}); // LEA R10, [RAX+1]
    bytes({0xf0, 0x4d, 0x0f, 0xb1, 0x13}); // LOCK CMPXCHG [R11], R10
    window.acquired = code.size();
    bytes({0x75}); // JNZ retry
    immediate(static_cast<std::uint8_t>(static_cast<int>(retry) - static_cast<int>(code.size() + 1)));
    const auto restore_entry = [&] {
      bytes({0x41, 0x5a, 0x58}); // POP R10; POP RAX
      restore_gate_context();
    };
    window.restoring = code.size();
    restore_entry();
    window.branch = code.size();
    const auto body = absolute_jump(0);
    const auto delta = static_cast<std::int32_t>(code.size() - closed - sizeof(std::int32_t));
    std::memcpy(code.data() + closed, &delta, sizeof(delta));
    window.closed = code.size();
    restore_entry();
    window.closed_branch = code.size();
    absolute_jump(fallback);
    return body;
  }

  void static_exit(State &watch, Address continuation, ExitWindow &window) {
    window.start = code.size();
    save_gate_context();
    reference_address(watch);
    bytes({0xf0, 0x49, 0xff, 0x0b}); // LOCK DECQ [R11]
    window.release = window.restoring = code.size();
    restore_gate_context();
    window.branch = code.size();
    if (continuation)
      absolute_jump(continuation);
    else
      bytes({0xc3});
  }

  void leave(State &watch) {
    // The callee's red zone is dead at its return. Use it to pass the
    // reference address without modifying flags or application registers.
    const auto reference = reinterpret_cast<Address>(&watch.runtime_inflight);
    bytes({0xc7, 0x44, 0x24, 0xf8});
    immediate(static_cast<std::uint32_t>(reference));
    bytes({0xc7, 0x44, 0x24, 0xfc});
    immediate(static_cast<std::uint32_t>(reference >> 32));
    bytes({0x3e, 0xff, 0x25, 0, 0, 0, 0}); // notrack jmp *0(%rip)
    immediate(reinterpret_cast<Address>(runtime_leave));
  }

  void xsave_mask(const XState &xstate) {
    bytes({0xb8}); immediate(xstate.low);
    bytes({0xba}); immediate(xstate.high);
  }

  void block_signals(const Site &site, SignalWindow &window, bool tail = false) {
    window.block_begin = code.size();
    bytes({0x48, 0x8d, 0x64, 0x24, 0xe8}); // LEA -24(RSP), RSP
    bytes({0x48, 0xb8}); immediate(site.watch->async_signals);
    const unsigned reserved = tail ? sizeof(Address) : 0;
    const auto mask = static_cast<std::uint8_t>(-16 - reserved);
    const auto old_mask = static_cast<std::uint8_t>(-8 - reserved);
    const auto status = static_cast<std::uint8_t>(-24 - reserved);
    bytes({0x49, 0x89, 0x44, 0x24, mask});
    bytes({0x31, 0xff}); // XOR EDI, EDI (SIG_BLOCK)
    bytes({0x49, 0x8d, 0x74, 0x24, mask});
    bytes({0x49, 0x8d, 0x54, 0x24, old_mask});
    bytes({0x41, 0xba}); immediate(8U);
    bytes({0xb8}); immediate(static_cast<std::uint32_t>(SYS_rt_sigprocmask));
    bytes({0x0f, 0x05});
    bytes({0x49, 0x89, 0x44, 0x24, status});
    window.block_end = code.size();
  }

  void unblock_signals(SignalWindow &window, bool tail) {
    window.unblock_begin = code.size();
    const unsigned reserved = tail ? sizeof(Address) : 0;
    const auto status = static_cast<std::uint8_t>(-24 - reserved);
    const auto old_mask = static_cast<std::uint8_t>(-8 - reserved);
    bytes({0x49, 0x83, 0x7c, 0x24, status, 0x00});
    bytes({0x75}); // Skip SETMASK when SIG_BLOCK was unavailable.
    const auto skip = code.size();
    immediate(std::uint8_t{0});
    bytes({0xbf}); immediate(static_cast<std::uint32_t>(SIG_SETMASK));
    bytes({0x49, 0x8d, 0x74, 0x24, old_mask});
    bytes({0x31, 0xd2});
    bytes({0x41, 0xba}); immediate(8U);
    bytes({0xb8}); immediate(static_cast<std::uint32_t>(SYS_rt_sigprocmask));
    bytes({0x0f, 0x05});
    code[skip] = static_cast<std::uint8_t>(code.size() - skip - 1);
  }

  void restore(const XState &xstate, bool preserve_tail_slot = false, SignalWindow *window = nullptr,
               bool tail = false) {
    if (xstate.xsave) {
      xsave_mask(xstate);
      bytes({0x48, 0x0f, 0xae, 0x2c, 0x24}); // xrstor64 (%rsp)
    } else {
      bytes({0x48, 0x0f, 0xae, 0x0c, 0x24}); // fxrstor64 (%rsp)
    }
    if (window)
      unblock_signals(*window, tail);
    bytes({0x4c, 0x89, 0xe4}); // mov %r12,%rsp
    if (window)
      window->pops = code.size();
    for (int reg = 7; reg >= 0; --reg)
      bytes({0x41, static_cast<std::uint8_t>(0x58 + reg)});
    bytes({0x5f, 0x5e, 0x5d, 0x5b, 0x5a, 0x59, 0x58});
    if (preserve_tail_slot) {
      // Duplicate saved flags below RSP. The tail target slot stays in
      // the temporary red zone until the destination restores application SP.
      bytes({0xff, 0x34, 0x24, 0x9d}); // PUSHQ (RSP); POPFQ
    } else {
      bytes({0x9d, 0x48, 0x8d, 0xa4, 0x24, 0x80, 0x00, 0x00, 0x00});
    }
    if (window)
      window->end = code.size();
  }

  template <typename Callback>
  std::optional<std::size_t> hook(const Site &site, Callback callback, const XState &xstate,
                                  Result result = Result::ignored, SignalWindow *window = nullptr,
                                  SignalWindow *tail_fallback = nullptr) {
    // LEA keeps flags intact. Leave the complete SysV red zone untouched.
    bytes({0x48, 0x8d, 0x64, 0x24, 0x80, 0x9c});
    bytes({0x50, 0x51, 0x52, 0x53, 0x55, 0x56, 0x57});
    for (std::uint8_t reg = 0; reg != 8; ++reg)
      bytes({0x41, static_cast<std::uint8_t>(0x50 + reg)});
    bytes({0x49, 0x89, 0xe4}); // mov %rsp,%r12
    if (result == Result::tail_target)
      bytes({0x48, 0x8d, 0x64, 0x24, 0xf8}); // Reserve the target slot below the saved frame.
    if (window) {
      if (result == Result::tail_target)
        bytes({0x48, 0x8d, 0x64, 0x24, 0xf8}); // Reserve the original target slot.
      block_signals(site, *window, result == Result::tail_target);
    }
    bytes({0x48, 0x83, 0xe4, 0xc0}); // align the extended state area
    bytes({0x48, 0x81, 0xec});
    immediate(xstate.size);
    if (xstate.xsave) {
      // XRSTOR requires the reserved XSAVE header bytes to be zero.
      for (std::uint32_t offset = 512; offset < 576; offset += 8) {
        bytes({0x48, 0xc7, 0x84, 0x24}); immediate(offset); immediate(0U);
      }
      xsave_mask(xstate);
      bytes({0x48, 0x0f, 0xae, 0x24, 0x24}); // xsave64 (%rsp)
    } else {
      bytes({0x48, 0x0f, 0xae, 0x04, 0x24}); // fxsave64 (%rsp)
    }
    bytes({0xfc, 0x48, 0xbf}); // cld; movabs site,%rdi
    immediate(reinterpret_cast<Address>(&site));
    bytes({0x4c, 0x89, 0xe6, 0x48, 0xb8}); // mov %r12,%rsi; movabs callback,%rax
    immediate(reinterpret_cast<Address>(callback));
    bytes({0x3e, 0xff, 0xd0}); // notrack call *%rax
    if (result == Result::dispatch_target) {
      // The application CALL has already created the original return address.
      // The callee's red zone is scratch space at this ABI boundary.
      bytes({0x49, 0x89, 0x84, 0x24}); // MOV RAX (original), -16(application SP)
      immediate<std::uint32_t>(sizeof(Registers) + 128 - sizeof(DispatchDestination));
      bytes({0x49, 0x89, 0x94, 0x24}); // MOV RDX (entry), -8(application SP)
      immediate<std::uint32_t>(sizeof(Registers) + 128 - sizeof(Address));
    }
    if (result == Result::tail_target) {
      if (window) {
        bytes({0x49, 0x89, 0x44, 0x24, static_cast<std::uint8_t>(-tail_destination_offset)});
        bytes({0x49, 0x89, 0x54, 0x24, 0xf8}); // MOV RDX (entry), -8(R12)
        bytes({0x48, 0x89, 0xd0}); // MOV RDX, RAX
        tail_fallback->block_begin = window->block_begin;
        tail_fallback->block_end = window->block_end;
      }
      bytes({0x48, 0x85, 0xc0, 0x0f, 0x85}); // TEST RAX, RAX; JNZ translated
      const auto translated = code.size();
      immediate(0U);
      restore(xstate, false, tail_fallback, true);
      if (site.instruction.value)
        code.insert(code.end(), site.instruction.bytes.begin(),
                    site.instruction.bytes.begin() + site.instruction.size);
      else
        absolute_jump(site.instruction.target);
      const auto distance = static_cast<std::int32_t>(code.size() - translated - 4);
      std::memcpy(code.data() + translated, &distance, sizeof(distance));
      bytes({0x49, 0x89, 0x44, 0x24, 0xf8}); // MOV RAX, -8(R12)
      restore(xstate, true, window, true);
      if (!site.instruction.value)
        bytes({0x3e});
      bytes({0xff, 0x64, 0x24, 0x80}); // JMP *-128(RSP)
      return std::nullopt;
    }
    std::optional<std::size_t> fallback;
    if (result == Result::prefix_open) {
      bytes({0x85, 0xc0, 0x0f, 0x85}); // TEST EAX, EAX; JNZ normal restore
      const auto skip = code.size();
      immediate(0U);
      restore(xstate);
      bytes({0xe9});
      fallback = code.size();
      immediate(0U);
      const auto distance = static_cast<std::int32_t>(code.size() - skip - 4);
      std::memcpy(code.data() + skip, &distance, sizeof(distance));
    }
    restore(xstate, false, window);
    return fallback;
  }
};

unsigned enter_runtime_prefix(const Site *site, const Registers *) noexcept {
  const PreserveErrno preserve;
  auto &watch = *site->watch;
  if (::getpid() != watch.pid)
    return 1; // A fork child still owns its inherited redirect bytes.
  auto &gate = watch.channel->runtime_prefix_gate;
  auto value = gate.load(std::memory_order_acquire);
  while (value & enabled)
    if (gate.compare_exchange_weak(value, value + 1, std::memory_order_acq_rel))
      return 1;
  return 0;
}

struct StaticLink {
  Address entry = 0;
  std::map<Address, Address> exits;
};

void make_executable(const Mapping &mapping, const Emitter &code) {
  std::memcpy(mapping.address, code.code.data(), code.code.size());
  if (::mprotect(mapping.address, mapping.size, PROT_READ | PROT_EXEC))
    throw std::runtime_error("cannot make translated code executable");
}

std::unique_ptr<Mapping> make_tail_gate(Site &site, const XState &xstate) {
  Emitter out;
  if (site.watch->mask_async_signals)
    out.hook(site, resolve_dispatch_call, xstate, Emitter::Result::tail_target,
             &site.dispatch_signals, &site.tail_fallback_signals);
  else
    out.hook(site, resolve_runtime_call, xstate, Emitter::Result::tail_target);
  auto mapping = std::make_unique<Mapping>();
  std::string error;
  if (!native_detour::allocate_code_cache(site.instruction.pc, out.code.size(), mapping->address,
                                          mapping->size, error, {}))
    throw std::runtime_error(error);
  site.gate = reinterpret_cast<Address>(mapping->address);
  site.dispatch_signals.relocate(site.gate);
  site.tail_fallback_signals.relocate(site.gate);
  make_executable(*mapping, out);
  return mapping;
}

std::unique_ptr<Mapping> translate(std::span<Site> sites, Function &function,
                                   const XState &xstate, const StaticLink *links = nullptr,
                                   Address *tail_entry = nullptr) {
  Emitter out;
  std::map<Address, std::size_t> labels;
  std::vector<std::pair<std::size_t, Address>> branches;
  std::vector<native_detour::RipRelocation> relocations;
  std::optional<std::size_t> dispatch;
  std::string error;
  const auto &last = sites.back().instruction;
  const bool prefix = sites.size() > 1 && last.flow == Flow::runtime_call &&
                      (last.value || last.indirect_memory);
  if (prefix) {
    const auto fallback = *out.hook(sites.front(), enter_runtime_prefix, xstate, Emitter::Result::prefix_open);
    relocations.push_back({fallback, fallback + 4, sites.front().instruction.pc, 0});
  }
  for (auto &site : sites) {
    const auto &inst = site.instruction;
    site.label = out.code.size();
    labels.emplace(inst.pc, site.label);
    site.runtime_prefix = prefix;
    if (inst.flow == Flow::runtime_tail) {
      site.native = out.code.size();
      out.absolute_jump(site.gate);
      continue;
    }
    if (inst.flow == Flow::runtime_call) {
      if (inst.indirect_memory) {
        const auto operand = x86_indirect_call(inst);
        if (!operand || operand->reg)
          throw std::runtime_error("invalid x86-64 memory-indirect call operand");
        const auto offset = operand->opcode_offset;
        // Fetch the original target before the hardware CALL pushes its
        // return. If the load faults, the bridge restores the original SP
        // and forwards the fault at the original application CALL PC.
        out.bytes({0x48, 0x8d, 0x64, 0x24, 0x80, 0x41, 0x53}); // LEA; PUSH R11
        site.native = out.code.size();
        const auto rex = static_cast<std::uint8_t>(0x4c | (offset ? inst.bytes[0] & 3 : 0));
        const auto modrm = static_cast<std::uint8_t>((inst.bytes[offset + 1] & 0xc7) | 0x18);
        out.bytes({rex, 0x8b, modrm}); // MOV r/m64, R11
        out.code.insert(out.code.end(), inst.bytes.begin() + offset + 2, inst.bytes.begin() + inst.size);
        if (operand->rip_relative) {
          std::int32_t delta;
          std::memcpy(&delta, inst.bytes.data() + inst.size - sizeof(delta), sizeof(delta));
          relocations.push_back({out.code.size() - sizeof(delta), out.code.size(),
                                 inst.pc + inst.size + delta, 0});
        }
        // The following CALL gives the callee ownership of this red-zone slot.
        out.bytes({0x4c, 0x89, 0x5c, 0x24, 0x78, 0x41, 0x5b}); // MOV R11, 120(RSP); POP R11
        out.bytes({0x48, 0x8d, 0xa4, 0x24, 0x80, 0x00, 0x00, 0x00});
      }
      if (inst.value || inst.indirect_memory) {
        // Execute the original prefix, then let a CALL in the application
        // mapping push the original return address on both stacks.
        out.bytes({0xe9});
        const auto displacement = out.code.size();
        out.immediate(0U);
        relocations.push_back({displacement, out.code.size(), inst.pc + inst.size - 5, 0});
      }
      dispatch = out.code.size();
      out.hook(site, resolve_dispatch_call, xstate, Emitter::Result::dispatch_target,
               site.watch->mask_async_signals ? &site.dispatch_signals : nullptr);
      site.dispatch_branch = out.code.size();
      if (!inst.value && !inst.indirect_memory)
        out.bytes({0x3e}); // An original direct CALL did not require an IBT landing pad.
      out.bytes({0xff, 0x64, 0x24, 0xf8}); // jmp *-8(%rsp)
      continue;
    }
    site.before_hook = inst.hook != Hook::none || inst.dispatch_entry || inst.scope_entry ||
                       inst.collection_resume || inst.inline_mutation_entry;
    if (site.before_hook)
      out.hook(site, before, xstate, Emitter::Result::ignored,
               site.watch->mask_async_signals ? &site.before_signals : nullptr);
    site.native = out.code.size();
    if (inst.size == 1 && inst.bytes[0] == 0xc3 && (links || !site.runtime_sites.empty())) {
      if (links)
        out.absolute_jump(links->exits.at(inst.pc));
      else
        out.leave(*site.watch);
      continue;
    }
    if (inst.flow == Flow::copy) {
      if (inst.rip_relative) {
        native_detour::RipRelocation relocation;
        if (!native_detour::code_cache_rip_relocation(
                {inst.bytes.data(), inst.size}, inst.pc, relocation, error))
          throw std::runtime_error(error);
        relocation.displacement_offset += site.native;
        relocation.instruction_end += site.native;
        relocations.push_back(relocation);
      }
      out.code.insert(out.code.end(), inst.bytes.begin(), inst.bytes.begin() + inst.size);
    } else {
      if (inst.flow == Flow::jump)
        out.bytes({0xe9});
      else
        out.bytes({0x0f, static_cast<std::uint8_t>(0x80 | inst.condition)});
      branches.emplace_back(out.code.size(), inst.target);
      out.immediate(0U);
    }
    if (inst.hook == Hook::move || inst.hook == Hook::xadd || inst.hook == Hook::cas) {
      site.after_label = out.code.size();
      out.hook(site, after, xstate, Emitter::Result::ignored,
               site.watch->mask_async_signals ? &site.after_signals : nullptr);
    }
  }
  if (function.resume) {
    if (links) {
      out.absolute_jump(links->exits.at(function.resume));
    } else {
      out.bytes({0xe9});
      const auto displacement = out.code.size();
      out.immediate(0U);
      relocations.push_back({displacement, out.code.size(), function.resume, 0});
    }
  }
  for (const auto &[offset, target] : branches) {
    const auto destination = labels.find(target);
    if (destination == labels.end()) {
      relocations.push_back({offset, offset + 4, links ? links->exits.at(target) : target, 0});
      continue;
    }
    const auto delta = static_cast<std::int32_t>(destination->second) -
                       static_cast<std::int32_t>(offset + 4);
    std::memcpy(out.code.data() + offset, &delta, sizeof(delta));
  }
  const auto adapter = out.code.size();
  if (tail_entry) {
    constexpr std::array<std::uint8_t, 4> endbr{0xf3, 0x0f, 0x1e, 0xfa};
    const auto &first = sites.front().instruction;
    if (first.size == endbr.size() && std::equal(endbr.begin(), endbr.end(), first.bytes.begin()))
      out.bytes({0xf3, 0x0f, 0x1e, 0xfa});
    out.bytes({0x48, 0x8d, 0xa4, 0x24, 0x88, 0, 0, 0, 0xe9}); // LEA 136(RSP), RSP; JMP body
    out.immediate(-static_cast<std::int32_t>(out.code.size() + sizeof(std::int32_t)));
  }
  auto map = std::make_unique<Mapping>();
  if (!native_detour::allocate_code_cache(function.near, out.code.size(),
                                          map->address, map->size, error, relocations))
    throw std::runtime_error(error);
  const auto base = reinterpret_cast<Address>(map->address);
  for (const auto &relocation : relocations) {
    const auto delta = static_cast<std::int64_t>(relocation.target) -
                       static_cast<std::int64_t>(base + relocation.instruction_end);
    if (delta < std::numeric_limits<std::int32_t>::min() ||
        delta > std::numeric_limits<std::int32_t>::max())
      throw std::runtime_error("code-cache RIP-relative relocation is out of range");
    const auto encoded = static_cast<std::int32_t>(delta);
    std::memcpy(out.code.data() + relocation.displacement_offset, &encoded, sizeof(encoded));
  }
  make_executable(*map, out);
  for (auto &site : sites) {
    site.label += base;
    site.before_signals.relocate(base);
    site.after_signals.relocate(base);
    if (site.instruction.flow != Flow::runtime_tail)
      site.dispatch_signals.relocate(base);
    if (site.dispatch_branch)
      site.dispatch_branch += base;
    if (site.after_label)
      site.after_label += base;
    if (site.instruction.flow != Flow::runtime_call || site.instruction.indirect_memory)
      site.native += base;
  }
  if (dispatch)
    function.dispatch = base + *dispatch;
  if (tail_entry)
    *tail_entry = base + adapter;
  return map;
}

void install_static_fragment(State &watch, std::span<Site> sites, Function &function,
                             const XState &xstate, RuntimeBlock *scope = nullptr) {
  auto &gates = scope ? scope->tail_gates : watch.mappings;
  auto &bodies = scope ? scope->scope_bodies : watch.static_bodies;
  if (std::ranges::any_of(sites, [](const auto &site) {
        return site.instruction.flow == Flow::runtime_call || site.instruction.flow == Flow::runtime_tail;
      })) {
    auto mapping = translate(sites, function, xstate);
    function.translated = reinterpret_cast<Address>(mapping->address);
    gates.push_back(std::move(mapping));
    return;
  }
  Emitter gate;
  auto &entry_site = sites.front();
  const auto body_slot = gate.static_entry(watch, entry_site.instruction.pc, entry_site.entry_gate);
  StaticLink links;
  const auto add_exit = [&](Address target, bool returns = false) {
    if (links.exits.emplace(target, gate.code.size()).second) {
      auto &window = entry_site.exit_gates.emplace_back();
      window.original_pc = target;
      gate.static_exit(watch, returns ? 0 : target, window);
    }
  };
  if (function.resume)
    add_exit(function.resume);
  for (const auto &site : sites) {
    const auto &inst = site.instruction;
    if (inst.size == 1 && inst.bytes[0] == 0xc3)
      add_exit(inst.pc, true);
    if (inst.flow != Flow::copy &&
        std::ranges::none_of(sites, [&](const auto &other) { return other.instruction.pc == inst.target; }))
      add_exit(inst.target);
  }
  std::string error;
  auto mapping = std::make_unique<Mapping>();
  if (!native_detour::allocate_code_cache(function.near, gate.code.size(), mapping->address,
                                          mapping->size, error, {}))
    throw std::runtime_error(error);
  links.entry = reinterpret_cast<Address>(mapping->address);
  for (auto &[target, exit] : links.exits)
    exit += links.entry;
  auto body = translate(sites, function, xstate, &links);
  const auto entry = reinterpret_cast<Address>(body->address);
  std::memcpy(gate.code.data() + body_slot, &entry, sizeof(entry));
  make_executable(*mapping, gate);
  sites.front().gate = links.entry;
  entry_site.entry_gate.relocate(links.entry);
  for (auto &window : entry_site.exit_gates)
    window.relocate(links.entry);
  for (auto &site : sites)
    site.reclaimable = true;
  function.translated = links.entry;
  bodies.push_back(std::move(body));
  gates.push_back(std::move(mapping));
}

#endif

RuntimeLocation locate_runtime_target(Address target) {
  std::ifstream maps("/proc/self/maps");
  std::string line;
  RuntimeLocation location;
  while (std::getline(maps, line)) {
    unsigned long long begin = 0, end = 0, offset = 0, inode = 0;
    unsigned dev_major = 0, dev_minor = 0;
    char permissions[5]{};
    int path_offset = 0;
    if (std::sscanf(line.c_str(), "%llx-%llx %4s %llx %x:%x %llu %n", &begin, &end,
                    permissions, &offset, &dev_major, &dev_minor, &inode, &path_offset) != 7 ||
        target < begin || target >= end)
      continue;
    if (permissions[0] != 'r' || permissions[2] != 'x' || permissions[1] == 'w' ||
        !inode || line.size() <= static_cast<std::size_t>(path_offset) || line[path_offset] != '/' ||
        line.ends_with(" (deleted)") || target - begin > UINT64_MAX - offset)
      break;
    location.end = end;
    location.offset = offset + target - begin;
    location.device = makedev(dev_major, dev_minor);
    location.inode = inode;
    location.path = line.substr(path_offset);
    break;
  }
  if (location.path.empty())
    throw std::runtime_error("runtime target has no immutable file-backed executable mapping");

  struct Lookup {
    Address target;
    RuntimeLocation &location;
    std::optional<std::uint64_t> unloads = std::nullopt;
  } lookup{target, location};
  const auto found = ::dl_iterate_phdr([](dl_phdr_info *info, std::size_t size, void *argument) {
    auto &query = *static_cast<Lookup *>(argument);
    if (size < offsetof(dl_phdr_info, dlpi_subs) + sizeof(info->dlpi_subs))
      return 0;
    query.unloads = info->dlpi_subs;
    if (query.target < info->dlpi_addr)
      return 0;
    const auto address = query.target - info->dlpi_addr;
    for (const auto &segment : std::span(info->dlpi_phdr, info->dlpi_phnum)) {
      if (segment.p_type != PT_LOAD || !(segment.p_flags & PF_X) || address < segment.p_vaddr)
        continue;
      const auto delta = address - segment.p_vaddr;
      if (delta >= segment.p_filesz || query.location.offset < segment.p_offset ||
          query.location.offset - segment.p_offset != delta)
        continue;
      query.location.bias = info->dlpi_addr;
      // The main image is fixed for this address space. A loader unload
      // invalidates DSO translations, even when the same ELF is remapped.
      query.location.main_image = !info->dlpi_name[0];
      query.location.unload_generation = query.location.main_image ? 0 : info->dlpi_subs;
      return 1;
    }
    return 0;
  }, &lookup);
  if (found)
    return location;

  // dl_iterate_phdr exposes the caller's namespace. dladdr1 can identify
  // a loader object by address across namespaces. Its load bias must still
  // agree with the verified file mapping and executable segment.
  Dl_info info{};
  void *link = nullptr;
  if (lookup.unloads &&
      ::dladdr1(reinterpret_cast<const void *>(target), &info, &link, RTLD_DL_LINKMAP) &&
      link && read_self(reinterpret_cast<Address>(link) + offsetof(link_map, l_addr), location.bias) &&
      location.bias <= target && !::dlinfo(link, RTLD_DI_LMID, &location.name_space)) {
    location.unload_generation = *lookup.unloads;
    return location;
  }
  throw std::runtime_error("runtime target is outside a verified loader image");
}

void *pin_runtime_module(const RuntimeLocation &location) {
  auto *handle = ::dlmopen(location.name_space, location.path.c_str(), RTLD_NOW | RTLD_NOLOAD);
  link_map *module = nullptr;
  if (handle && !::dlinfo(handle, RTLD_DI_LINKMAP, &module) && module && module->l_addr == location.bias)
    return handle;
  if (handle)
    ::dlclose(handle);
  throw std::runtime_error("cannot hold the runtime nonleaf image through exact restoration");
}

RuntimeImage::RuntimeImage(const RuntimeLocation &location) {
  struct File {
    int fd;
    ~File() { if (fd >= 0) ::close(fd); }
  } file{::open(location.path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK)};
  struct stat before{}, after{};
  if (file.fd < 0 || ::fstat(file.fd, &before) || !S_ISREG(before.st_mode) ||
      before.st_dev != location.device || before.st_ino != location.inode ||
      before.st_size < static_cast<off_t>(sizeof(Elf64_Ehdr)))
    throw std::runtime_error("cannot identify the mapped runtime ELF file");
  auto buffer = llvm::WritableMemoryBuffer::getNewUninitMemBuffer(before.st_size, location.path);
  if (!buffer)
    throw std::runtime_error("cannot allocate runtime ELF byte copy");
  for (std::size_t offset = 0; offset < buffer->getBufferSize();) {
    const auto count = ::pread(file.fd, buffer->getBufferStart() + offset,
                                buffer->getBufferSize() - offset, offset);
    if (count < 0 && errno == EINTR)
      continue;
    if (count <= 0)
      throw std::runtime_error("cannot read the complete runtime ELF file");
    offset += count;
  }
  if (::fstat(file.fd, &after) || before.st_size != after.st_size ||
      before.st_mtim.tv_sec != after.st_mtim.tv_sec || before.st_mtim.tv_nsec != after.st_mtim.tv_nsec)
    throw std::runtime_error("runtime ELF file changed while reading it");
  auto elf = llvm::object::createBinary(buffer->getMemBufferRef());
  if (!elf) {
    llvm::consumeError(elf.takeError());
    throw std::runtime_error("cannot parse runtime ELF");
  }
  const auto *object = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(elf->get());
#if defined(__aarch64__)
  constexpr auto native_arch = llvm::Triple::aarch64;
#else
  constexpr auto native_arch = llvm::Triple::x86_64;
#endif
  if (!object || object->getArch() != native_arch)
    throw std::runtime_error("runtime callee is outside a native ELF image");
  binary = {std::move(*elf), std::move(buffer)};
}

std::pair<std::string, Address> RuntimeImage::function(Address address) const {
  const auto &object = *llvm::cast<llvm::object::ELF64LEObjectFile>(binary.getBinary());
  Address size = 0;
  const auto find_entry = [&](const llvm::object::SymbolRef &symbol) {
    const llvm::object::ELFSymbolRef elf(symbol);
    if (elf.getELFType() != STT_FUNC || !elf.getSize())
      return;
    auto start = symbol.getAddress();
    if (!start) {
      llvm::consumeError(start.takeError());
      return;
    }
    if (*start == address) {
      if (size && size != elf.getSize())
        throw std::runtime_error("runtime callee has ambiguous ELF symbol extents");
      size = elf.getSize();
    }
  };
  for (const auto &symbol : object.symbols())
    find_entry(symbol);
  for (const auto &symbol : object.getDynamicSymbolIterators())
    find_entry(symbol);
  if (!size)
    throw std::runtime_error("runtime callee has no concrete ELF symbol extent");
  auto headers = object.getELFFile().program_headers();
  if (!headers) {
    llvm::consumeError(headers.takeError());
    throw std::runtime_error("runtime executable has invalid program headers");
  }
  const auto file = object.getMemoryBufferRef().getBuffer();
  for (const auto &segment : *headers) {
    if (segment.p_type != PT_LOAD || !(segment.p_flags & PF_X) ||
        address < segment.p_vaddr || segment.p_offset > file.size() ||
        segment.p_filesz > file.size() - segment.p_offset)
      continue;
    const Address delta = address - segment.p_vaddr;
    if (delta <= segment.p_filesz && size <= segment.p_filesz - delta) {
      const Address offset = segment.p_offset + delta;
      return {file.substr(offset, size).str(), offset};
    }
  }
  throw std::runtime_error("runtime callee exceeds executable ELF segment");
}

bool matches_runtime_bytes(Address target, const RuntimeLocation &location, const RuntimeBlock &block) {
  if (block.file_offset != location.offset || block.bytes.size() > location.end - target)
    return false;
  std::array<char, 256> live{};
  for (std::size_t offset = 0; offset < block.bytes.size();) {
    const auto size = std::min(live.size(), block.bytes.size() - offset);
    if (!read_self_memory(target + offset, live.data(), size) ||
        std::memcmp(live.data(), block.bytes.data() + offset, size))
      return false;
    offset += size;
  }
  return true;
}

void prepare_runtime_scope(RuntimeBlock &block, State &watch, Address start,
                           std::uint32_t module, const native_entry::Preview &body) {
  std::set<Address> targets;
  for (const auto &inst : body.instructions) {
    if (inst.flow == NativeFlow::branch && !inst.target)
      throw std::runtime_error("runtime scope has an unresolved branch");
    if (inst.target && *inst.target >= start && *inst.target - start < block.bytes.size())
      targets.insert(*inst.target);
  }
  struct Fragment {
    std::vector<Instruction> code;
    Address resume = 0;
    bool call = false;
  };
  std::vector<Fragment> fragments;
  const auto copy = [&](const native_entry::Instruction &inst) {
    Instruction out;
    out.pc = inst.address;
    out.size = inst.size;
    out.rip_relative = inst.cache_rip_relative;
    std::memcpy(out.bytes.data(), block.bytes.data() + (inst.address - start), inst.size);
    return out;
  };
  const auto store = [&](const native_entry::Instruction &inst) {
    return inst.store32 && (watch.atomic || inst.store32->kind == native_entry::ScalarStore::Kind::move);
  };
  std::set<Address> covered_stores;
  for (auto it = body.instructions.begin(); it != body.instructions.end(); ++it) {
    const auto &inst = *it;
    if (inst.flow == NativeFlow::call) {
      auto route = copy(inst);
      route.flow = Flow::runtime_call;
      std::vector<Instruction> prefix;
#if defined(__aarch64__)
      std::uint32_t word;
      std::memcpy(&word, route.bytes.data(), sizeof(word));
      if (inst.target && (word & 0xfc000000U) == 0x94000000U)
        route.target = *inst.target;
      else if (!inst.target && (word & 0xfffffc1fU) == 0xd63f0000U && ((word >> 5) & 31) != 31)
        route.value = ((word >> 5) & 31) + 1;
      else {
        block.partial = true;
        continue;
      }
#else
      if (inst.target && inst.size == 5 && route.bytes[0] == 0xe8) {
        route.target = *inst.target;
      } else if (!inst.target && inst.indirect_near_call) {
        if (const auto operand = x86_indirect_call(route)) {
          route.value = operand->reg;
          route.indirect_memory = !operand->reg;
          route.rip_relative = operand->rip_relative;
          const auto end = inst.address + inst.size;
          auto start = inst.address;
          for (auto previous = it; previous != body.instructions.begin();) {
            const auto &candidate = *--previous;
            if (candidate.flow != NativeFlow::ordinary || !candidate.falls_through ||
                !(candidate.cache_copyable || candidate.cache_rip_relative) ||
                candidate.address + candidate.size != start || candidate.text == "endbr64" ||
                covered_stores.count(candidate.address))
              break;
            start = candidate.address;
            prefix.push_back(copy(candidate));
            if (end - start >= 10)
              break;
          }
          const auto interior = targets.upper_bound(start);
          if (end - start < 10 || end - start > RuntimePatch{}.original.size() ||
              (interior != targets.end() && *interior < end) ||
              std::ranges::any_of(fragments, [&](const auto &prior) {
                return prior.code.front().pc < end &&
                    start < prior.code.back().pc + prior.code.back().size;
              }))
            prefix.clear();
          std::reverse(prefix.begin(), prefix.end());
        }
      }
      if (!route.target && prefix.empty()) {
        block.partial = true;
        continue;
      }
#endif
      prefix.push_back(route);
      fragments.push_back({std::move(prefix), 0, true});
    } else if (store(inst) && !covered_stores.count(inst.address)) {
      Fragment fragment;
      const auto overlaps = [&](Address begin, Address end) {
        return std::ranges::any_of(fragments, [&](const auto &prior) {
          return prior.code.front().pc < end && begin < prior.code.back().pc + prior.code.back().size;
        });
      };
#if defined(__aarch64__)
      if (inst.size == 4 && inst.cache_copyable) {
        fragment.code.push_back(copy(inst));
        fragment.resume = inst.address + inst.size;
      }
#else
      for (auto first = it;; --first) {
        std::vector<Instruction> window;
        auto next = first->address;
        for (auto current = first; current != body.instructions.end(); ++current) {
          if (current->address != next || current->flow != NativeFlow::ordinary ||
              !(current->cache_copyable || current->cache_rip_relative) ||
              (current != first && targets.count(next)))
            break;
          window.push_back(copy(*current));
          next += current->size;
          if (next - first->address >= 5)
            break;
        }
        if (next > inst.address && next - first->address >= 5 && next - first->address <= 15 &&
            !overlaps(first->address, next)) {
          fragment = {std::move(window), next, false};
          break;
        }
        if (first == body.instructions.begin() || inst.address - (first - 1)->address >= 15 ||
            first->flow != NativeFlow::ordinary)
          break;
      }
#endif
      if (fragment.code.empty() || overlaps(fragment.code.front().pc, fragment.resume)) {
        block.partial = true;
        continue;
      }
      auto decoded = std::ranges::lower_bound(body.instructions, fragment.code.front().pc, {},
                                              &native_entry::Instruction::address);
      for (auto &out : fragment.code) {
        if (store(*decoded)) {
          instrument_store(out, *decoded->store32);
          covered_stores.insert(out.pc);
        }
        ++decoded;
      }
      fragments.push_back(std::move(fragment));
    } else if (inst.flow == NativeFlow::ordinary && inst.may_store && !store(inst) &&
               !inst.changes_stack && !inst.text.starts_with("push")) {
      block.partial = true;
    } else if (inst.flow == NativeFlow::branch && inst.target && !targets.count(*inst.target)) {
      block.partial = true;
    }
  }
  if (fragments.empty())
    throw std::runtime_error("runtime scope has no verified store or call redirect");
  const auto site_count = std::accumulate(fragments.begin(), fragments.end(), std::size_t{0},
      [](std::size_t total, const auto &fragment) { return total + fragment.code.size(); });
  block.sites.reserve(site_count);
  for (const auto &fragment : fragments)
    for (const auto &inst : fragment.code) {
      auto &site = block.sites.emplace_back(Site{inst, &watch, start});
      site.runtime_module = module;
      site.native_scope = true;
    }
  const XState xstate;
  std::size_t offset = 0;
  for (const auto &fragment : fragments) {
    const auto pc = fragment.code.front().pc;
    const auto size = fragment.code.back().pc + fragment.code.back().size - pc;
    auto sites = std::span(block.sites).subspan(offset, fragment.code.size());
    for (auto &site : sites)
      site.runtime_sites = sites;
    Function function{nullptr, fragment.code.size(), pc, 0, fragment.resume};
    install_static_fragment(watch, sites, function, xstate, &block);
    auto &patch = block.scope_patches.emplace_back();
    patch.pc = pc;
    patch.gateway = function.translated;
#if defined(__x86_64__)
    if (fragment.call && fragment.code.size() > 1)
      patch.dispatch = function.dispatch;
#endif
    patch.size = size;
    patch.call = fragment.call;
    std::memcpy(patch.original.data(), block.bytes.data() + (pc - start), size);
    offset += fragment.code.size();
  }
}

bool request_runtime_scope(State &watch, RuntimeBlock &block, Address target,
                           const RuntimeLocation &location) noexcept {
  auto &admission = watch.channel->admission;
  if (admission.phase.load(std::memory_order_acquire) != RuntimeAdmission::idle)
    return false;
  admission.function = target;
  admission.patches = reinterpret_cast<Address>(block.scope_patches.data());
  admission.count = block.scope_patches.size();
  admission.scope = block.scope_index;
  admission.width = block.bytes.size();
  admission.module = block.sites.front().runtime_module;
  admission.device = location.device;
  admission.inode = location.inode;
  admission.file_offset = location.offset;
  unsigned idle = RuntimeAdmission::idle;
  if (!admission.phase.compare_exchange_strong(idle, RuntimeAdmission::ready, std::memory_order_release))
    return false;
  const timespec timeout{0, 250000000};
  for (;;) {
    auto phase = admission.phase.load(std::memory_order_acquire);
    if (phase != RuntimeAdmission::ready && phase != RuntimeAdmission::committing)
      break;
    if (::syscall(SYS_futex, &admission.phase, FUTEX_WAIT, phase,
                  phase == RuntimeAdmission::ready ? &timeout : nullptr, nullptr, 0) == -1 &&
        errno == ETIMEDOUT && phase == RuntimeAdmission::ready &&
        admission.phase.compare_exchange_strong(phase, RuntimeAdmission::idle, std::memory_order_acq_rel))
      break;
  }
  return watch.channel->scope_status(watch.event_buffer, watch.runtime_limit, block.scope_index)
      ->load(std::memory_order_acquire) == RuntimeAdmission::accepted;
}

Address original_call_target(const Site &site, const Registers &regs) noexcept {
  const auto reg = site.instruction.value;
  auto target = reg ? regs.value(reg) : site.instruction.target;
#if defined(__x86_64__)
  if (reg == rsp && site.instruction.flow != Flow::runtime_tail)
    target += sizeof(Address);
  if (site.instruction.indirect_memory)
    std::memcpy(&target, reinterpret_cast<const void *>(regs.value(rsp) - sizeof(Address)), sizeof(target));
#endif
  return target;
}

Address resolve_runtime_target(const Site *site, Address target) noexcept {
  const PreserveErrno preserve;
  const bool tail = site->instruction.flow == Flow::runtime_tail;
#if defined(__aarch64__)
  const auto fallback = site->instruction.value && !tail ? target : 0;
#else
  const auto fallback = tail ? 0 : target;
#endif
  auto &watch = *site->watch;
  // A tail gate lives outside its body. Keep the old reference until the
  // destination has acquired its own reference or selected original code.
  const struct TailHandoff {
    State &watch;
    bool tail;
    ~TailHandoff() {
      if (tail)
        watch.runtime_inflight.fetch_sub(1, std::memory_order_release);
    }
  } handoff{watch, tail};
  if (::getpid() != watch.pid)
    return fallback;
#if defined(__x86_64__)
  if (site->runtime_prefix)
    finish_runtime_prefix(watch);
#endif
  const Activity activity(*watch.channel);
  if (!activity.active)
    return fallback;
  if (watch.channel->reclaim_request.load(std::memory_order_acquire))
    return fallback;
  if (std::ranges::binary_search(watch.static_scopes, target))
    return fallback;
  std::uint8_t gap = 1;
  if (!inside_helper) {
    const HelperGuard guard(site);
    try {
      // Query the loader before taking the watch lock. Loader callbacks can
      // enter application code on a different thread.
      const auto location = locate_runtime_target(target);
      const RuntimeImageKey image_key{location.device, location.inode, location.bias,
                                      location.unload_generation};
      const auto target_key = std::pair{image_key, target};
      const auto scope_key = std::tuple{location.device, location.inode, location.bias, target};
      std::unique_lock lock(watch.runtime_mutex);
      const auto enter = [&](const RuntimeBlock &block) {
        if (watch.channel->reclaim_request.load(std::memory_order_acquire))
          return fallback;
        watch.runtime_inflight.fetch_add(1, std::memory_order_relaxed);
        if (tail)
          return block.tail_entry;
        return reinterpret_cast<Address>(block.mapping->address);
      };
      if (watch.channel->reclaim_request.load(std::memory_order_acquire))
        return fallback;
      if (const auto cached = watch.runtime_scopes.find(scope_key); cached != watch.runtime_scopes.end()) {
        const auto &scope = *cached->second;
        if (watch.channel->scope_status(watch.event_buffer, watch.runtime_limit, scope.scope_index)
                ->load(std::memory_order_acquire) == RuntimeAdmission::accepted &&
            !scope.partial)
          return fallback;
        throw std::runtime_error("runtime scope has incomplete coverage");
      }
      const auto cached = watch.runtime_targets.find(target_key);
      if (cached != watch.runtime_targets.end()) {
        if (!cached->second->mapping) {
          const auto &scope = *cached->second;
          if (watch.channel->scope_status(watch.event_buffer, watch.runtime_limit, scope.scope_index)
                  ->load(std::memory_order_acquire) == RuntimeAdmission::accepted &&
              !scope.partial)
            return fallback;
          throw std::runtime_error("runtime scope has incomplete coverage");
        }
        if (matches_runtime_bytes(target, location, *cached->second))
          return enter(*cached->second);
        watch.runtime_targets.erase(cached);
      }
      if (watch.runtime_count == watch.runtime_limit) {
        gap = 2;
        throw std::runtime_error("runtime translation limit reached");
      }
      const auto known = watch.runtime_images.find(image_key);
      std::optional<RuntimeImage> fresh;
      if (known == watch.runtime_images.end()) {
        if (location.path.size() >= RuntimeModule{}.path.size())
          throw std::runtime_error("runtime module path exceeds the event format");
        fresh.emplace(location);
      }
      const auto &image = fresh ? *fresh : known->second;
      auto block = std::make_unique<RuntimeBlock>();
      std::tie(block->bytes, block->file_offset) = image.function(target - location.bias);
      if (!matches_runtime_bytes(target, location, *block))
        throw std::runtime_error("runtime callee bytes disagree with the mapped ELF");
      const auto decoded = native_entry::scan(target, block->bytes);
      const bool nonleaf = !decoded.decode_failed && decoded.diagnostic.empty() &&
          std::ranges::any_of(decoded.instructions, [](const auto &inst) {
            return inst.flow == NativeFlow::call;
          });
      const auto module_index = fresh ? watch.runtime_modules + 1 : image.module_index;
      if (nonleaf) {
        const auto first_slot = watch.runtime_count;
        prepare_runtime_scope(*block, watch, target, module_index, decoded);
        if (!location.main_image) {
          lock.unlock();
          block->module_pin = pin_runtime_module(location);
          lock.lock();
          if (watch.channel->reclaim_request.load(std::memory_order_acquire) ||
              watch.runtime_count != first_slot)
            throw std::runtime_error("runtime scope changed while pinning the image");
        }
        block->scope_index = first_slot;
      } else {
        const auto code = decode_leaf(target, block->bytes, true,
          [&](const native_entry::Instruction &inst, Instruction &out) {
            if (inst.store32 && (watch.atomic ||
                inst.store32->kind == native_entry::ScalarStore::Kind::move))
              instrument_store(out, *inst.store32);
            else if (inst.may_store && inst.flow == NativeFlow::ordinary)
              throw std::runtime_error("runtime callee has an unsupported memory write");
          }, true);
        for (const auto &instruction : code) {
          auto &entry = block->sites.emplace_back(Site{instruction, &watch, target});
          entry.runtime_module = module_index;
        }
        const XState xstate;
        for (auto &entry : block->sites) {
          entry.runtime_sites = block->sites;
          if (entry.instruction.flow == Flow::runtime_tail)
            block->tail_gates.push_back(make_tail_gate(entry, xstate));
        }
        Function function{nullptr, code.size(), target};
        block->mapping = translate(block->sites, function, xstate, nullptr, &block->tail_entry);
      }
      if (!watch.recycler_started) {
        std::thread(reclaim_code, &watch).detach();
        watch.recycler_started = true;
      }
      if (fresh) {
        auto *module = watch.channel->module(watch.event_buffer, module_index - 1);
        const auto bytes = fresh->binary.getBinary()->getMemoryBufferRef().getBuffer();
        module->bias = location.bias;
        module->sha256 = llvm::SHA256::hash(llvm::ArrayRef<std::uint8_t>(
            reinterpret_cast<const std::uint8_t *>(bytes.data()), bytes.size()));
        std::memcpy(module->path.data(), location.path.c_str(), location.path.size() + 1);
        fresh->module_index = module_index;
        watch.runtime_images.emplace(image_key, std::move(*fresh));
        watch.runtime_modules = module_index;
        module->generation.store(module_index, std::memory_order_release);
      }
      block->previous = watch.runtime_blocks.load(std::memory_order_relaxed);
      watch.runtime_targets.emplace(target_key, block.get());
      ++watch.runtime_count;
      auto *const ready = block.release();
      watch.runtime_blocks.store(ready, std::memory_order_release);
      if (nonleaf) {
        const bool installed = request_runtime_scope(watch, *ready, target, location);
        if (installed) {
          try {
            watch.runtime_scopes.emplace(scope_key, ready);
          } catch (const std::bad_alloc &) {
            // The original generation remains in runtime_targets. Its
            // verified patches and retained loader handle remain owned.
          }
        }
        if (!installed && ready->module_pin &&
            watch.channel->admission.phase.load(std::memory_order_acquire) != RuntimeAdmission::disabled) {
          const auto handle = std::exchange(ready->module_pin, nullptr);
          lock.unlock();
          ::dlclose(handle);
        }
        if (installed && !ready->partial)
          return fallback;
        throw std::runtime_error(installed ? "runtime scope has incomplete coverage" :
                                             "runtime scope redirect transaction was rejected");
      }
      watch.channel->runtime_translations.fetch_add(1, std::memory_order_release);
      return enter(*ready);
    } catch (...) {
      // The real application call still enters its original callee.
    }
  }
  Observation event{};
  event.pc = site->instruction.pc;
  event.destination = target;
  event.hook = Hook::dispatch_gap;
  event.gap = gap;
  emit(watch, event);
  return fallback;
}

Address resolve_runtime_call(const Site *site, const Registers *regs) noexcept {
  return resolve_runtime_target(site, original_call_target(*site, *regs));
}

DispatchDestination resolve_dispatch_call(const Site *site, const Registers *regs) noexcept {
  const auto target = original_call_target(*site, *regs);
  return {target, resolve_runtime_target(site, target)};
}

std::unique_ptr<Mapping> map_channel(const char *path, std::size_t size) {
  const int fd = ::open(path, O_RDWR | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0)
    throw std::runtime_error("cannot open private code-cache event channel");
  struct stat st{};
  void *shared = MAP_FAILED;
  if (!::fstat(fd, &st) && S_ISREG(st.st_mode) && st.st_uid == ::geteuid() &&
      (st.st_mode & 077) == 0 && st.st_size == static_cast<off_t>(size))
    shared = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  ::close(fd);
  if (shared == MAP_FAILED)
    throw std::runtime_error("cannot map private code-cache event channel");
  return std::unique_ptr<Mapping>(new Mapping{shared, size});
}

void initialize(Init &init
#if defined(__aarch64__)
                , const arm_snapshot::Init *snapshot = nullptr
#endif
                ) {
  const std::lock_guard lock(install_mutex);
  auto watch = std::make_unique<State>();
  if (init.event_buffer < 2 || init.event_buffer > max_event_buffer)
    throw std::runtime_error("invalid code-cache event buffer size");
  if (init.runtime_limit > max_runtime_limit)
    throw std::runtime_error("invalid runtime code-cache translation limit");
  const auto size = Channel::mapping_size(init.event_buffer, init.runtime_limit);
  watch->channel_mapping = map_channel(init.path, size);
  watch->channel = static_cast<Channel *>(watch->channel_mapping->address);
#if defined(__aarch64__)
  if (snapshot) {
    watch->snapshot_mapping = map_channel(snapshot->queue, sizeof(arm_snapshot::Queue));
    clangtracer::v1::ObjectWatchRequest specification;
    if (!snapshot->specifications || snapshot->specifications_size > 1024 * 1024 ||
        snapshot->read_only_count > snapshot->read_only.size() ||
        !specification.ParseFromArray(snapshot->specifications, snapshot->specifications_size))
      throw std::runtime_error("invalid prepared snapshot codec request");
    for (const auto &prepared : specification.methods()) {
      clangtracer::v1::ProbeSpec spec;
      *spec.mutable_prepared() = prepared;
      *spec.mutable_signature() = prepared.signature();
      *spec.mutable_debug_info() = prepared.header_types();
      spec.set_snapshot_report_unavailable(true);
      spec.set_snapshot_max_items(specification.max_elements());
      spec.set_snapshot_max_bytes(specification.snapshot_max_bytes());
      spec.set_snapshot_max_string_bytes(specification.max_string_bytes());
      spec.set_snapshot_max_depth(specification.snapshot_max_depth());
      for (const auto &source : std::span(snapshot->read_only).first(snapshot->read_only_count)) {
        auto &range = *spec.add_snapshot_rtti_ranges();
        range.set_begin(source.begin);
        range.set_end(source.end);
      }
      watch->snapshot_codecs.push_back(arm_snapshot::compile(spec));
    }
  }
#endif
  watch->pid = ::getpid();
  watch->offset = init.scalar;
  watch->union_tag_offset = init.union_tag_offset;
  watch->receiver_typeinfo = init.receiver_typeinfo;
  watch->async_signals = init.async_signals;
  if (watch->async_signals) {
    std::ifstream status("/proc/self/status");
    for (std::string line; std::getline(status, line);) {
      unsigned seccomp;
      if (std::sscanf(line.c_str(), "Seccomp:%u", &seccomp) == 1) {
        watch->mask_async_signals = seccomp == 0;
        break;
      }
    }
  }
  watch->tagged_union = init.tagged_union;
  watch->stop_on_overflow = init.stop_on_overflow;
  watch->cfi_stack = init.cfi_stack;
  watch->stack_depth = init.stack_depth;
  watch->max_instances = init.max_instances;
  watch->writer_tid = init.writer_tid;
  watch->event_buffer = init.event_buffer;
  watch->serialize_wait_us = init.serialize_wait_us;
  watch->runtime_limit = init.runtime_limit;
  watch->shared_file = init.shared_file;
  watch->max_events = init.max_events;
  watch->atomic = init.atomic;
  watch->no_values = init.no_values;
  watch->first_seen = init.first_seen;
  watch->borrow = init.borrow;
  watch->collection = init.collection;
  if (init.static_scope_count)
    watch->static_scopes.assign(init.static_scopes, init.static_scopes + init.static_scope_count);
  if (init.collection.map_key_size > init.collection.map_key.size() ||
      init.collection.pointer_hops > init.collection.pointer_offsets.size())
    throw std::runtime_error("code-cache range description exceeds its verified bound");
  if (init.module[0] && !init.permanent_startup_module) {
    // Keep the loader's reference for as long as a retained translated block
    // can return into this image, including after redirects are restored.
    watch->module_pin = ::dlopen(init.module, RTLD_NOW | RTLD_NOLOAD);
    struct link_map *module = nullptr;
    if (!watch->module_pin || ::dlinfo(watch->module_pin, RTLD_DI_LINKMAP, &module) ||
        !module || module->l_addr != init.module_bias)
      throw std::runtime_error("cannot pin the verified shared image at its live load bias");
  }
  const std::span functions(init.functions, init.function_count);
  for (const auto &function : functions)
    for (std::size_t i = 0; i < function.count; ++i)
      watch->sites.push_back({function.instructions[i], watch.get(), function.instructions[0].pc});
  const XState xstate;
  std::size_t first = 0;
  for (auto &function : functions) {
    if (!function.count)
      continue;
    auto sites = std::span(watch->sites).subspan(first, function.count);
    install_static_fragment(*watch, sites, function, xstate);
    first += function.count;
  }
  install_bridge(watch->async_signals);
  if (!watch->static_bodies.empty())
    watch->channel->static_fragments.store(watch->static_bodies.size(), std::memory_order_release);
  if (!watch->static_bodies.empty() || (watch->module_pin && watch->runtime_limit)) {
    std::thread(reclaim_code, watch.get()).detach();
    watch->recycler_started = true;
  }
  watch->previous = retained.load(std::memory_order_relaxed);
  retained.store(watch.release(), std::memory_order_release);
}
} // namespace
} // namespace tracer::object_cache

extern "C" int tracer_agent_object_cache_v33(tracer::object_cache::Init *request) {
  try {
    tracer::object_cache::initialize(*request);
    return 0;
  } catch (const std::exception &error) {
    std::snprintf(request->error, sizeof(request->error), "%s", error.what());
    return -1;
  }
}

#if defined(__aarch64__)
extern "C" int tracer_agent_arm_snapshot_v1(tracer::arm_snapshot::Init *request) {
  try {
    tracer::object_cache::initialize(request->code, request);
    return 0;
  } catch (const std::exception &error) {
    std::snprintf(request->code.error, sizeof(request->code.error), "%s", error.what());
    return -1;
  }
}
#endif

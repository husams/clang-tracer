#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "tracer/compat.hpp"

namespace tracer::context_engine {

inline constexpr std::size_t max_capture_depth = 64;
inline constexpr std::size_t max_capture_items = 65536;

enum class CaptureStatus {
  ok,
  bytes_exceeded,
  string_exceeded,
  invalid_field,
  depth_exceeded,
  visits_exceeded,
  items_exceeded,
  invalid_cap,
  incomplete_message,
  work_exceeded
};
// Messages that may be open at once. An event nests the envelope and the record
// in 32 of them and each level of followed members in two more (kMaxFollowDepth).
inline constexpr std::size_t kMaxMessageDepth = 128;

struct CaptureLimits {
  std::size_t bytes = 256 * 1024;
  std::size_t string_bytes = 64 * 1024;
  std::size_t visits = 16384;
  std::size_t items = 4096;
  std::size_t depth = 32;
  std::size_t byte_work = 8 * 1024 * 1024;
  std::size_t item_cap = 4096;
};

// Native capture primitive for generated codecs. All memory is supplied before
// payload locks; no method allocates, calls user code or performs output I/O.
// A sticky failure invalidates the entire capture, including earlier fields,
// unless the codec returns to a mark it took before the part that failed.
class BoundedProtoWriter final {
public:
  // Inline so generated JIT code needs no hidden constructor symbol.
  explicit BoundedProtoWriter(::tracer::compat::span<std::byte> storage,
                              CaptureLimits limits = {}) noexcept
      : storage_(storage.first(std::min(storage.size(), limits.bytes))),
        limits_(limits) {}
  BoundedProtoWriter(const BoundedProtoWriter &) = delete;
  BoundedProtoWriter &operator=(const BoundedProtoWriter &) = delete;
  BoundedProtoWriter(BoundedProtoWriter &&) = delete;
  BoundedProtoWriter &operator=(BoundedProtoWriter &&) = delete;
  CaptureStatus status() const noexcept { return status_; }
  std::size_t remaining() const noexcept { return storage_.size() - used_; }

  // Fixed-format metadata has no generated codec traversal. Each field
  // consumes a byte, so the remaining storage also bounds its field visits.
  void bound_remaining_visits_by_bytes() noexcept {
    limits_.visits = visits_ + (storage_.size() - used_);
  }

  ::tracer::compat::span<const std::byte> finish() noexcept;
  bool u64(std::uint32_t field, std::uint64_t) noexcept;
  bool s64(std::uint32_t field, std::int64_t) noexcept;
  bool boolean(std::uint32_t field, bool) noexcept;
  bool f32(std::uint32_t field, float) noexcept;
  bool f64(std::uint32_t field, double) noexcept;
  bool string(std::uint32_t field, std::string_view) noexcept;
  // A short text the codec itself supplies (not captured data): written like a
  // string, without the limit on the bytes of a captured string.
  bool label(std::uint32_t field, std::string_view) noexcept;
  bool begin_message(std::uint32_t field) noexcept;
  bool end_message() noexcept;
  bool container_items(std::uint64_t total, std::size_t cap,
                       std::size_t &selected) noexcept;

  // A position the capture can return to. A codec takes one before a part it
  // can leave out (the pointee of a smart pointer); rewind drops what was
  // written after it and clears a failure that part ran into, when the failure
  // is a limit of the payload: bytes, string bytes, nesting depth or elements.
  // The visits and the byte work already spent stay spent, so the limits on
  // effort still bound a capture that leaves parts out.
  struct Mark {
    std::size_t used = 0, depth = 0, items = 0;
  };
  Mark mark() const noexcept { return {used_, depth_, items_}; }
  bool rewind(const Mark &mark) noexcept {
    if (status_ != CaptureStatus::ok && status_ != CaptureStatus::bytes_exceeded &&
        status_ != CaptureStatus::string_exceeded &&
        status_ != CaptureStatus::depth_exceeded && status_ != CaptureStatus::items_exceeded)
      return false;
    if (mark.used > used_ || mark.depth > depth_)
      return false;
    used_ = mark.used;
    depth_ = mark.depth;
    items_ = mark.items;
    status_ = CaptureStatus::ok;
    return true;
  }

private:
  bool fail(CaptureStatus) noexcept;
  bool reserve(std::size_t) noexcept;
  bool charge(std::size_t) noexcept;
  bool tag(std::uint32_t field, unsigned wire) noexcept;
  bool varint(std::uint64_t) noexcept;
  bool fixed(std::uint64_t, unsigned bytes) noexcept;
  struct Message {
    std::size_t length_position, payload_position;
  };

  const ::tracer::compat::span<std::byte> storage_;
  CaptureLimits limits_;
  CaptureStatus status_ = CaptureStatus::ok;
  std::size_t used_ = 0, visits_ = 0, items_ = 0, depth_ = 0, work_ = 0;
  std::array<Message, kMaxMessageDepth> messages_{};
};
} // namespace tracer::context_engine

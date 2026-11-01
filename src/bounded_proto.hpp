#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace tracer::context_engine {

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
struct CaptureLimits {
  std::size_t bytes = 256 * 1024;
  std::size_t string_bytes = 64 * 1024;
  std::size_t visits = 16384;
  std::size_t items = 4096;
  std::size_t depth = 32;
  std::size_t byte_work = 8 * 1024 * 1024;
};

// Native capture primitive for generated codecs. All memory is supplied before
// payload locks; no method allocates, calls user code or performs output I/O.
// A sticky failure invalidates the entire capture, including earlier fields.
class BoundedProtoWriter final {
public:
  explicit BoundedProtoWriter(std::span<std::byte>,
                              CaptureLimits = {}) noexcept;
  BoundedProtoWriter(const BoundedProtoWriter &) = delete;
  BoundedProtoWriter &operator=(const BoundedProtoWriter &) = delete;
  BoundedProtoWriter(BoundedProtoWriter &&) = delete;
  BoundedProtoWriter &operator=(BoundedProtoWriter &&) = delete;
  CaptureStatus status() const noexcept { return status_; }
  std::span<const std::byte> finish() noexcept;
  bool u64(std::uint32_t field, std::uint64_t) noexcept;
  bool s64(std::uint32_t field, std::int64_t) noexcept;
  bool boolean(std::uint32_t field, bool) noexcept;
  bool f32(std::uint32_t field, float) noexcept;
  bool f64(std::uint32_t field, double) noexcept;
  bool string(std::uint32_t field, std::string_view) noexcept;
  bool begin_message(std::uint32_t field) noexcept;
  bool end_message() noexcept;
  bool container_items(std::uint64_t total, std::size_t cap,
                       std::size_t &selected) noexcept;

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
  const std::span<std::byte> storage_;
  const CaptureLimits limits_;
  CaptureStatus status_ = CaptureStatus::ok;
  std::size_t used_ = 0, visits_ = 0, items_ = 0, depth_ = 0, work_ = 0;
  std::array<Message, 32> messages_{};
};
} // namespace tracer::context_engine

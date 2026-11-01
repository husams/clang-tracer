#include "bounded_proto.hpp"

#include <algorithm>
#include <bit>

namespace tracer::context_engine {
namespace {
void copy_bytes(void *destination, const void *source,
                std::size_t bytes) noexcept {
  auto *to = static_cast<volatile unsigned char *>(destination);
  auto *from = static_cast<const volatile unsigned char *>(source);
  for (std::size_t i = 0; i < bytes; ++i)
    to[i] = from[i];
}
void move_bytes(void *destination, const void *source,
                std::size_t bytes) noexcept {
  const auto to_address = reinterpret_cast<std::uintptr_t>(destination);
  const auto from_address = reinterpret_cast<std::uintptr_t>(source);
  if (to_address <= from_address || to_address - from_address >= bytes) {
    copy_bytes(destination, source, bytes);
    return;
  }
  auto *to = static_cast<volatile unsigned char *>(destination);
  auto *from = static_cast<const volatile unsigned char *>(source);
  while (bytes) {
    --bytes;
    to[bytes] = from[bytes];
  }
}
unsigned encode_varint(std::uint64_t value, std::byte *output) noexcept {
  unsigned size = 0;
  do {
    output[size++] =
        static_cast<std::byte>((value & 127) | (value > 127 ? 128 : 0));
    value >>= 7;
  } while (value);
  return size;
}
} // namespace
BoundedProtoWriter::BoundedProtoWriter(std::span<std::byte> storage,
                                       CaptureLimits limits) noexcept
    : storage_(storage.first(std::min(storage.size(), limits.bytes))),
      limits_(limits) {}
bool BoundedProtoWriter::fail(CaptureStatus status) noexcept {
  if (status_ == CaptureStatus::ok)
    status_ = status;
  return false;
}
bool BoundedProtoWriter::charge(std::size_t work) noexcept {
  if (status_ != CaptureStatus::ok)
    return false;
  if (work > limits_.byte_work - std::min(work_, limits_.byte_work))
    return fail(CaptureStatus::work_exceeded);
  work_ += work;
  return true;
}
bool BoundedProtoWriter::reserve(std::size_t size) noexcept {
  if (status_ != CaptureStatus::ok)
    return false;
  if (size > storage_.size() - used_)
    return fail(CaptureStatus::bytes_exceeded);
  return charge(size);
}
bool BoundedProtoWriter::varint(std::uint64_t value) noexcept {
  std::array<std::byte, 10> encoded{};
  const auto size = encode_varint(value, encoded.data());
  if (!reserve(size))
    return false;
  copy_bytes(storage_.data() + used_, encoded.data(), size);
  used_ += size;
  return true;
}
bool BoundedProtoWriter::tag(std::uint32_t field, unsigned wire) noexcept {
  if (status_ != CaptureStatus::ok)
    return false;
  if (!field || field >= (1U << 29) || (field >= 19000 && field <= 19999))
    return fail(CaptureStatus::invalid_field);
  if (visits_ >= limits_.visits)
    return fail(CaptureStatus::visits_exceeded);
  ++visits_;
  return varint((static_cast<std::uint64_t>(field) << 3) | wire);
}
bool BoundedProtoWriter::u64(std::uint32_t field,
                             std::uint64_t value) noexcept {
  return tag(field, 0) && varint(value);
}
bool BoundedProtoWriter::s64(std::uint32_t field, std::int64_t value) noexcept {
  const auto bits = std::bit_cast<std::uint64_t>(value);
  return u64(field, (bits << 1) ^ (std::uint64_t{0} - (bits >> 63)));
}
bool BoundedProtoWriter::boolean(std::uint32_t field, bool value) noexcept {
  return u64(field, value ? 1 : 0);
}
bool BoundedProtoWriter::fixed(std::uint64_t bits, unsigned bytes) noexcept {
  if (!reserve(bytes))
    return false;
  for (unsigned i = 0; i < bytes; ++i) {
    storage_[used_++] = static_cast<std::byte>(bits & 255);
    bits >>= 8;
  }
  return true;
}
bool BoundedProtoWriter::f32(std::uint32_t field, float value) noexcept {
  return tag(field, 5) && fixed(std::bit_cast<std::uint32_t>(value), 4);
}
bool BoundedProtoWriter::f64(std::uint32_t field, double value) noexcept {
  return tag(field, 1) && fixed(std::bit_cast<std::uint64_t>(value), 8);
}
bool BoundedProtoWriter::string(std::uint32_t field,
                                std::string_view value) noexcept {
  if (value.size() > limits_.string_bytes)
    return fail(CaptureStatus::string_exceeded);
  if (!tag(field, 2) || !varint(value.size()) || !reserve(value.size()))
    return false;
  if (!value.empty())
    copy_bytes(storage_.data() + used_, value.data(), value.size());
  used_ += value.size();
  return true;
}
bool BoundedProtoWriter::begin_message(std::uint32_t field) noexcept {
  if (depth_ >= std::min(messages_.size(), limits_.depth))
    return fail(CaptureStatus::depth_exceeded);
  if (!tag(field, 2) || !reserve(1))
    return false;
  messages_[depth_++] = {used_, used_ + 1};
  ++used_;
  return true;
}
bool BoundedProtoWriter::end_message() noexcept {
  if (status_ != CaptureStatus::ok)
    return false;
  if (!depth_)
    return fail(CaptureStatus::incomplete_message);
  const auto message = messages_[--depth_];
  const auto payload_size = used_ - message.payload_position;
  std::array<std::byte, 10> length{};
  const auto length_size = encode_varint(payload_size, length.data());
  if (!reserve(length_size - 1))
    return false;
  if (!charge(payload_size + length_size))
    return false;
  move_bytes(storage_.data() + message.length_position + length_size,
             storage_.data() + message.payload_position, payload_size);
  copy_bytes(storage_.data() + message.length_position, length.data(),
             length_size);
  used_ += length_size - 1;
  return true;
}
bool BoundedProtoWriter::container_items(std::uint64_t total, std::size_t cap,
                                         std::size_t &selected) noexcept {
  selected = 0;
  if (status_ != CaptureStatus::ok)
    return false;
  if (cap > 4096)
    return fail(CaptureStatus::invalid_cap);
  const auto count =
      static_cast<std::size_t>(std::min<std::uint64_t>(total, cap));
  if (count > limits_.items - std::min(items_, limits_.items))
    return fail(CaptureStatus::items_exceeded);
  if (visits_ >= limits_.visits)
    return fail(CaptureStatus::visits_exceeded);
  ++visits_;
  items_ += count;
  selected = count;
  return true;
}
std::span<const std::byte> BoundedProtoWriter::finish() noexcept {
  if (depth_)
    fail(CaptureStatus::incomplete_message);
  return status_ == CaptureStatus::ok ? storage_.first(used_)
                                      : std::span<const std::byte>{};
}
} // namespace tracer::context_engine

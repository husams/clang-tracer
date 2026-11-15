#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace tracer::object_watch {

enum class ValueWriteKind : std::uint8_t { none, string, variant, any };
enum class ValueWriteRegion : std::uint8_t { none, representation, string_data, any_payload };

// Offsets come from the target's DWARF. The agent never invokes a standard
// library member, a type-erasure manager or any application code to read them.
struct StringWriteLayout {
  std::uint64_t pointer = 0, length = 0, local = 0, local_size = 0, capacity = 0;
};

struct ValueWriteLayout {
  ValueWriteKind kind = ValueWriteKind::none;
  std::uint64_t size = 0, selector = 0, any_storage = 0;
  unsigned selector_width = 0;
  StringWriteLayout string;
  bool active() const noexcept { return kind != ValueWriteKind::none; }
};

struct ValueWriteAlternative {
  std::uint64_t selector = 0, offset = 0, size = 0;
  bool indirect = false;
  StringWriteLayout string;
};

inline constexpr unsigned max_value_store_width = 16;
using ValueStoreBytes = std::array<char, max_value_store_width>;

struct ValueWrite {
  ValueWriteRegion region = ValueWriteRegion::none;
  std::uint64_t address = 0, offset = 0;
  unsigned size = 0;
};

struct ValueWriteMatch {
  ValueWrite write;
  bool partial = false;
};

inline ValueWrite intersect_value_store(ValueWriteRegion region, std::uint64_t start,
    std::uint64_t size, std::uint64_t target, unsigned width) noexcept {
  if (!start || !width || !size ||
      size > UINT64_MAX - start || width > UINT64_MAX - target)
    return {};
  const auto first = std::max(start, target);
  const auto last = std::min(start + size, target + width);
  return first < last ? ValueWrite{region, first, first - start,
                                   static_cast<unsigned>(last - first)} : ValueWrite{};
}

template <typename Reader>
ValueWriteMatch match_value_store(const ValueWriteLayout &layout,
    const ValueWriteAlternative *alternatives, std::size_t count, unsigned string_limit,
    std::uint64_t field, std::uint64_t target, unsigned width, Reader read) noexcept {
  if (!field || !layout.active() || layout.size > UINT64_MAX - field ||
      width > max_value_store_width)
    return {};
  const auto representation = intersect_value_store(ValueWriteRegion::representation,
      field, layout.size, target, width);
  const auto string = [&](const StringWriteLayout &str, std::uint64_t object) {
    std::uint64_t data = 0, length = 0, capacity = 0;
    if (!read(object + str.pointer, &data, sizeof(data)) ||
        !read(object + str.length, &length, sizeof(length)) || !data)
      return ValueWriteMatch{representation, true};
    if (data == object + str.local) {
      capacity = str.local_size - 1;
    } else if (!read(object + str.capacity, &capacity, sizeof(capacity)) ||
               (data >= object && data - object < str.local + str.local_size)) {
      return ValueWriteMatch{representation, true};
    }
    if (length > capacity || length >= UINT64_MAX - data)
      return ValueWriteMatch{representation, true};
    const auto range = intersect_value_store(ValueWriteRegion::string_data,
        data, length + 1, target, width);
    if (range.region == ValueWriteRegion::none)
      return ValueWriteMatch{representation, false};
    if (range.offset > string_limit || range.size > string_limit - range.offset)
      return ValueWriteMatch{range.size == width ? ValueWrite{} : representation, true};
    if (range.size < width && representation.region != ValueWriteRegion::none)
      return ValueWriteMatch{representation, false};
    return ValueWriteMatch{range, false};
  };
  if (layout.kind == ValueWriteKind::string)
    return string(layout.string, field);
  std::uint64_t selector = 0;
  if (!read(field + layout.selector, &selector, layout.selector_width))
    return {representation, true};
  if (layout.kind == ValueWriteKind::any && !selector)
    return {representation, false};
  const auto *found = alternatives;
  for (; count && found->selector != selector; ++found, --count) {}
  if (!count) {
    const bool valueless = layout.kind == ValueWriteKind::variant &&
        selector == ((std::uint64_t{1} << (8 * layout.selector_width)) - 1);
    return {representation, !valueless};
  }
  auto value = field + found->offset;
  if (found->indirect &&
      (!read(field + layout.any_storage, &value, sizeof(value)) || !value ||
       found->size > UINT64_MAX - value))
    return {representation, true};
  if (found->string.local_size) {
    auto match = string(found->string, value);
    if (match.write.region != ValueWriteRegion::none || match.partial)
      return match;
  }
  if (representation.region != ValueWriteRegion::none || !found->indirect)
    return {representation, false};
  return {intersect_value_store(ValueWriteRegion::any_payload, value, found->size,
                                target, width), false};
}

// These types cross the private server-agent channel as plain data.
static_assert(std::is_trivially_copyable_v<ValueWriteLayout>);
static_assert(std::is_trivially_copyable_v<ValueWriteAlternative>);

} // namespace tracer::object_watch

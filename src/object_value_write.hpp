#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <optional>

namespace tracer::object_watch {

enum class ValueWriteKind : std::uint8_t { none, string, variant, any, union_storage };
enum class ValueWriteRegion : std::uint8_t { none, representation, string_data, any_payload, union_tag };

// Offsets come from the target's DWARF. The agent never invokes a standard
// library member, a type-erasure manager or any application code to read them.
struct StringWriteLayout {
  std::uint64_t pointer = 0, length = 0, local = 0, local_size = 0, capacity = 0;
};

struct ValueWriteLayout {
  ValueWriteKind kind = ValueWriteKind::none;
  std::uint64_t size = 0, selector = 0, any_storage = 0;
  unsigned selector_width = 0;
  bool selector_before = false;
  StringWriteLayout string;
  bool active() const noexcept { return kind != ValueWriteKind::none; }
};

struct ValueWriteAlternative {
  std::uint64_t selector = 0, offset = 0, size = 0;
  bool indirect = false, scalar = false;
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

inline std::uint64_t union_tag_address(const ValueWriteLayout &layout, std::uint64_t field) noexcept {
  if (!layout.selector_width)
    return 0;
  if (layout.selector_before)
    return field > layout.selector ? field - layout.selector : 0;
  return layout.selector <= UINT64_MAX - field ? field + layout.selector : 0;
}

inline ValueWrite match_union_store(const ValueWriteLayout &layout, std::uint64_t field,
    std::uint64_t destination, unsigned width) noexcept {
  const auto storage = intersect_value_store(ValueWriteRegion::representation, field, layout.size,
                                             destination, width);
  if (storage.size)
    return storage;
  return intersect_value_store(ValueWriteRegion::union_tag, union_tag_address(layout, field),
                               layout.selector_width, destination, width);
}

template <typename Lifetimes, typename Ambiguous>
auto match_live_union(const ValueWriteLayout &layout, const Lifetimes &live, std::uint64_t offset,
                      std::uint64_t destination, unsigned width, Ambiguous ambiguous) {
  using Result = std::pair<decltype(live.begin()), ValueWrite>;
  if (!width || width > max_value_store_width || width > UINT64_MAX - destination)
    return Result{live.end(), {}};
  const auto tag = !layout.selector_width ? 0 :
      layout.selector_before ? offset - layout.selector : offset + layout.selector;
  const auto first = layout.selector_width ? std::min(offset, tag) : offset;
  const auto end = std::max(offset + layout.size, tag + layout.selector_width);
  const auto lower = destination < end ? 0 : destination - end + 1;
  const auto last = destination + width - 1;
  if (last < first)
    return Result{live.end(), {}};
  Result match{live.end(), {}};
  for (auto item = live.lower_bound(lower); item != live.end() && item->first <= last - first; ++item) {
    if (item->first > UINT64_MAX - offset)
      continue;
    const auto write = match_union_store(layout, item->first + offset, destination, width);
    if (!write.size)
      continue;
    if (match.second.size) {
      ambiguous();
      return Result{live.end(), {}};
    }
    match = {item, write};
  }
  return match;
}

struct UnionWriteState {
  std::optional<std::uint64_t> tag;
  std::optional<std::uint64_t> scalar;
};

struct UnionActivation {
  std::optional<std::uint64_t> tag;
  unsigned initialized = 0;
  bool pending = false;
};

inline void confirm_union_payload(UnionActivation &activation, const ValueWriteAlternative *alternatives,
    std::size_t count, std::uint64_t field, const ValueWrite &write, bool verified,
    UnionWriteState &before, UnionWriteState &after) noexcept {
  const bool changed = before.tag != after.tag;
  if (!verified || !before.tag || !after.tag || changed ||
      (activation.tag && activation.tag != before.tag)) {
    activation.pending = true;
    activation.initialized = 0;
  }
  activation.tag = after.tag;
  if (!verified || changed) {
    before.scalar.reset();
    after.scalar.reset();
    return;
  }
  if (!activation.pending)
    return;
  // Bytes left by the preceding alternative cannot supply the new value.
  // Track coverage of the new scalar across its admitted stores.
  before.scalar.reset();
  if (!after.scalar)
    return;
  for (; count; --count, ++alternatives) {
    const auto &alternative = *alternatives;
    if (alternative.selector != after.tag || !alternative.scalar)
      continue;
    const auto touched = intersect_value_store(ValueWriteRegion::representation,
        field + alternative.offset, alternative.size, write.address, write.size);
    if (touched.size)
      activation.initialized |= ((1u << touched.size) - 1) << touched.offset;
    activation.pending = activation.initialized != (1u << alternative.size) - 1;
    break;
  }
  if (activation.pending)
    after.scalar.reset();
}

template <typename Reader>
UnionWriteState sample_union_write(const ValueWriteLayout &layout,
    const ValueWriteAlternative *alternatives, std::size_t count,
    std::uint64_t field, const ValueWrite &write, Reader read) noexcept {
  if (layout.kind != ValueWriteKind::union_storage || !layout.selector_width || !field)
    return {};
  std::uint64_t tag = 0;
  const auto address = union_tag_address(layout, field);
  if (!address || !read(address, &tag, layout.selector_width))
    return {};
  UnionWriteState result{tag, std::nullopt};
  if (write.region != ValueWriteRegion::representation)
    return result;
  for (; count; --count, ++alternatives) {
    const auto &alternative = *alternatives;
    if (alternative.selector != tag)
      continue;
    if (alternative.scalar &&
        intersect_value_store(ValueWriteRegion::representation, field + alternative.offset,
                              alternative.size, write.address, write.size).size) {
      std::uint64_t bits = 0;
      if (read(field + alternative.offset, &bits, alternative.size))
        result.scalar = bits;
    }
    break;
  }
  return result;
}

template <typename Reader>
ValueWriteMatch match_value_store(const ValueWriteLayout &layout,
    const ValueWriteAlternative *alternatives, std::size_t count, unsigned string_limit,
    std::uint64_t field, std::uint64_t target, unsigned width, Reader read) noexcept {
  if (!field || !layout.active() || layout.size > UINT64_MAX - field ||
      width > max_value_store_width)
    return {};
  if (layout.kind == ValueWriteKind::union_storage)
    return {match_union_store(layout, field, target, width), false};
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

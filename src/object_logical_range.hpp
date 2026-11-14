#pragma once

#include <cstdint>
#include <optional>

namespace tracer::object_watch {
struct LogicalRangeSample {
  std::uint64_t destination = 0;
  std::optional<std::uint64_t> index;
  std::optional<std::uint64_t> size;
  std::optional<std::uint64_t> value;
};

enum class LogicalChange : unsigned char {
  none, inserted, erased, moved, value_changed, resized, destroyed
};

inline LogicalChange logical_change(const LogicalRangeSample &before,
                                    const LogicalRangeSample &after, bool destroyed) noexcept {
  if (destroyed)
    return LogicalChange::destroyed;
  if (!before.destination && after.destination)
    return LogicalChange::inserted;
  if (before.destination && !after.destination)
    return LogicalChange::erased;
  if (before.index && after.index && before.index != after.index)
    return LogicalChange::moved;
  if (before.value && after.value && before.value != after.value)
    return LogicalChange::value_changed;
  if (before.size && after.size && before.size != after.size)
    return LogicalChange::resized;
  return LogicalChange::none;
}
} // namespace tracer::object_watch

#pragma once

#include <cstdint>

namespace tracer::object_cache::identity {

// The result is a snapshot of one target-side lifetime registration. Method
// callbacks may use it to gate PRE/POST delivery while still invoking the
// application's original method.
enum class Status : std::uint8_t {
  unregistered,
  staged,
  absent,
  provisional,
  active,
  destroying,
  stale,
  retired,
};

struct Identity {
  Status status = Status::unregistered;
  std::uint64_t object_id = 0;
  std::uint64_t generation = 0;
};

// receiver_type_verified must be true only when the caller has independently
// admitted the exact prepared receiver type. It permits a provisional result
// for a pre-attach object with no observed constructor history; it never
// upgrades an address with a retired generation.
Identity lookup(std::uint64_t watch_id, std::uint64_t receiver,
                bool receiver_type_verified = false) noexcept;

} // namespace tracer::object_cache::identity

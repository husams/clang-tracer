#pragma once

#include <cstdint>
#include <optional>

namespace tracer::object_cache::identity {

struct Identity {
  std::uint64_t object_id = 0;
  std::uint64_t generation = 0;
};

// Only a method with an independently verified exact receiver may call this.
// Zero identity denotes a provisional receiver with no active lifetime proof:
// This includes a copy in reused storage, an ambiguously observed construction,
// or a verified construction that exceeded the identity admission limit.
std::optional<Identity> lookup(std::uint64_t watch_id, std::uint64_t receiver) noexcept;

} // namespace tracer::object_cache::identity

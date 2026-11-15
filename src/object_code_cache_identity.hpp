#pragma once

#include <cstdint>
#include <optional>

namespace tracer::object_cache::identity {

struct Identity {
  std::uint64_t object_id = 0;
  std::uint64_t generation = 0;
};

// Only a method with an independently verified exact receiver may call this.
// No value closes admission. Zero identity denotes a provisional receiver
// whose construction was never observed by this registration.
std::optional<Identity> lookup(std::uint64_t watch_id, std::uint64_t receiver) noexcept;

} // namespace tracer::object_cache::identity

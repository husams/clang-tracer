#pragma once
#include "tracer/compat.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace tracer::object_watch {
constexpr unsigned max_pointee_hops = 32;
using RangeHeader = std::array<std::uint64_t, max_pointee_hops>;

template <typename ReadPointer>
bool read_pointee_chain(std::uint64_t owner, ::tracer::compat::span<const std::uint64_t> offsets,
                        RangeHeader &header, ReadPointer read) {
  for (std::size_t index = 0; index < offsets.size(); ++index) {
    if (!owner || offsets[index] > UINT64_MAX - owner ||
        owner + offsets[index] > UINT64_MAX - (sizeof(owner) - 1) ||
        !read(owner + offsets[index], header[index]) || !header[index])
      return false;
    owner = header[index];
  }
  return true;
}
} // namespace tracer::object_watch

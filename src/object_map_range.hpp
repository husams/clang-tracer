#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

namespace tracer::object_watch {
struct MapLayout {
  std::uint64_t header = 0, root = 0, count = 0;
  std::uint64_t left = 0, right = 0, parent = 0;
  std::uint64_t key_pointer = 0, key_length = 0;
  std::uint64_t next = 0, key_offset = 0, key_bits = 0;
  unsigned key_width = 0;
  bool hash = false;
};

template <typename Reader>
bool map_key_matches(const MapLayout &layout, std::uint64_t node, std::string_view key,
                     unsigned max_string_bytes, Reader read, bool &matches) {
  matches = false;
  if (layout.key_width) {
    std::uint64_t bits = 0;
    if (node > UINT64_MAX - layout.key_offset ||
        !read(node + layout.key_offset, &bits, layout.key_width))
      return false;
    matches = bits == layout.key_bits;
    return true;
  }
  const auto field = [&](auto offset, std::uint64_t &value) {
    return node <= UINT64_MAX - offset && read(node + offset, &value, sizeof(value));
  };
  std::uint64_t length;
  if (!field(layout.key_length, length) || length > max_string_bytes)
    return false;
  if (length != key.size())
    return true;
  std::uint64_t pointer;
  if (!field(layout.key_pointer, pointer) || !pointer || pointer > UINT64_MAX - length)
    return false;
  for (std::size_t pos = 0; pos < length;) {
    std::array<char, 64> bytes;
    const auto size = std::min(bytes.size(), key.size() - pos);
    if (!read(pointer + pos, bytes.data(), size))
      return false;
    if (key.substr(pos, size) != std::string_view(bytes.data(), size))
      return true;
    pos += size;
  }
  matches = true;
  return true;
}

// Walk the verified libstdc++ tree via parent links without allocating in the
// target. Each node is visited once on descent, and the count bounds all links.
template <typename Reader>
std::uint64_t map_destination(const MapLayout &layout, const std::array<std::uint64_t, 3> &header,
                              std::uint64_t scalar, std::string_view key,
                              unsigned max_elements, unsigned max_string_bytes, Reader read) {
  const auto [root, count, sentinel] = header;
  if (!root || root == sentinel || !count || count > max_elements)
    return 0;
  const auto field = [&](auto node, auto offset, std::uint64_t &value) {
    return node <= UINT64_MAX - offset && read(node + offset, &value, sizeof(value));
  };
  auto previous = sentinel;
  auto node = root;
  bool descending = true;
  std::uint64_t visited = 0, destination = 0;
  const auto admit = [&] {
    bool matches;
    if (!map_key_matches(layout, node, key, max_string_bytes, read, matches))
      return false;
    if (matches) {
      if (destination || node > UINT64_MAX - scalar)
        return false;
      destination = node + scalar;
    }
    return true;
  };
  if (layout.hash) {
    for (std::uint64_t i = 0; i < count; ++i) {
      if (!node || node == sentinel || !admit() || !field(node, layout.next, node))
        return 0;
    }
    return node ? 0 : destination;
  }
  for (std::uint64_t step = 0; step < 2 * count; ++step) {
    std::uint64_t left, right, parent;
    if (!field(node, layout.left, left) || !field(node, layout.right, right) ||
        !field(node, layout.parent, parent) || !parent || parent == node ||
        left == node || right == node || left == sentinel || right == sentinel ||
        (left && left == right) || (parent == sentinel) != (node == root))
      return 0;
    auto next = parent;
    if (descending) {
      if (previous != parent)
        return 0;
      if (++visited > count)
        return 0;
      if (!admit())
        return 0;
      if (left || right)
        next = left ? left : right;
    } else if (previous == left) {
      if (right)
        next = right;
    } else if (previous != right) {
      return 0;
    }
    if (next == sentinel)
      return visited == count ? destination : 0;
    descending = next != parent;
    previous = node;
    node = next;
  }
  return 0;
}
} // namespace tracer::object_watch

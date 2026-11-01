#pragma once

#include <cstddef>
#include <functional>
#include <unordered_set>

namespace context_schema_user_hash {
struct Record {
  int value;
};
} // namespace context_schema_user_hash

namespace std {
template <> struct hash<context_schema_user_hash::Record> {
  size_t operator()(const context_schema_user_hash::Record &) const noexcept {
    return 0;
  }
};
} // namespace std

using ContextSchemaUserHashPayload =
    std::unordered_set<context_schema_user_hash::Record>;

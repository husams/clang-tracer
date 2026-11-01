#pragma once

#include "context_receiver.hpp"

#include <google/protobuf/message.h>

#include <cstddef>
#include <set>
#include <string>
#include <string_view>

namespace tracer::context_query {

struct FieldPaths final {
  std::set<std::string> paths;
  bool truncated = false;
};

inline constexpr std::size_t kDefaultFieldPathLimit = 8192;
inline constexpr std::size_t kQueryOutputLimit = 256 * 1024;

FieldPaths discover_field_paths(
    const context_engine::ReceiverSchemaView &schema,
    std::size_t max_paths = kDefaultFieldPathLimit,
    unsigned max_depth = 32);

// Dotted paths name C++ fields. Sequence elements and optional payloads are
// traversed transparently; maps aggregate values while preserving typed keys.
std::string query_field(const google::protobuf::Message &message,
                       const context_engine::ReceiverSchemaView &schema,
                       std::string_view path);

} // namespace tracer::context_query

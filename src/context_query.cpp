#include "context_query.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "tracer/compat.hpp"

namespace tracer::context_query {
namespace {
using FD = google::protobuf::FieldDescriptor;
using Descriptor = google::protobuf::Descriptor;
using Message = google::protobuf::Message;
using context_engine::ReceiverSchemaView;

class RoleIndex final {
public:
  explicit RoleIndex(::tracer::compat::span<const std::string> roles)
      : roles_(roles) {}

  std::string_view find(std::string_view path) const noexcept {
    for (const auto &role : roles_) {
      const auto equal = role.find('=');
      if (equal != std::string::npos &&
          std::string_view(role).substr(0, equal) == path)
        return std::string_view(role).substr(equal + 1);
    }
    return {};
  }

private:
  ::tracer::compat::span<const std::string> roles_;
};

bool sequence_role(std::string_view role) noexcept {
  return role == "vector" || role == "deque" || role == "list" || role == "set" ||
         role == "unordered_set" || ::tracer::compat::starts_with(role, "array:");
}

bool map_role(std::string_view role) noexcept {
  return role == "map" || role == "unordered_map";
}

std::string_view cpp_field_name(const FD *field) noexcept {
  if (!field)
    return {};
  const auto &options = field->options();
  for (int i = 0; i < options.uninterpreted_option_size(); ++i) {
    const auto &option = options.uninterpreted_option(i);
    if (option.name_size() == 1 &&
        option.name(0).name_part() == "tracer_cpp_field_name" &&
        option.has_string_value())
      return option.string_value();
  }
  return field->name();
}

void append_path_component(std::string &path, std::string_view component) {
  if (!path.empty())
    path.push_back('.');
  path.append(component);
}

void append_role_field(std::string &path, int field_number) {
  if (!path.empty())
    path.push_back('.');
  path += std::to_string(field_number);
}

void discover_message(const Descriptor *, const RoleIndex &, std::string,
                      std::string, unsigned, unsigned, std::size_t,
                      FieldPaths &, std::set<const Descriptor *> &);

void discover_field(const FD *field, const RoleIndex &roles,
                    std::string role_path, std::string user_path,
                    unsigned depth, unsigned max_depth,
                    std::size_t max_paths, FieldPaths &result,
                    std::set<const Descriptor *> &ancestors) {
  if (!field || result.truncated)
    return;
  if (depth >= max_depth) {
    result.truncated = true;
    return;
  }
  const auto role = roles.find(role_path);
  if (role == "optional") {
    if (field->cpp_type() != FD::CPPTYPE_MESSAGE) {
      result.truncated = true;
      return;
    }
    const auto *value = field->message_type()->FindFieldByNumber(1);
    discover_field(value, roles, role_path + "/value", std::move(user_path),
                   depth + 1, max_depth, max_paths, result, ancestors);
    return;
  }
  if (map_role(role)) {
    if (field->cpp_type() != FD::CPPTYPE_MESSAGE || !field->is_repeated()) {
      result.truncated = true;
      return;
    }
    const auto *value = field->message_type()->FindFieldByNumber(2);
    discover_field(value, roles, role_path + "/value", std::move(user_path),
                   depth + 1, max_depth, max_paths, result, ancestors);
    return;
  }
  if (sequence_role(role)) {
    const auto value_path = role_path + "/value";
    const auto value_role = roles.find(value_path);
    if (sequence_role(value_role) || map_role(value_role) ||
        value_role == "optional") {
      if (field->cpp_type() != FD::CPPTYPE_MESSAGE) {
        result.truncated = true;
        return;
      }
      const auto *items = field->message_type()->FindFieldByNumber(1);
      discover_field(items, roles, value_path, std::move(user_path),
                     depth + 1, max_depth, max_paths, result, ancestors);
    } else if (field->cpp_type() == FD::CPPTYPE_MESSAGE) {
      discover_message(field->message_type(), roles, value_path,
                       std::move(user_path), depth + 1, max_depth, max_paths,
                       result, ancestors);
    }
    return;
  }
  if (field->cpp_type() == FD::CPPTYPE_MESSAGE)
    discover_message(field->message_type(), roles, std::move(role_path),
                     std::move(user_path), depth + 1, max_depth, max_paths,
                     result, ancestors);
}

void discover_message(const Descriptor *descriptor, const RoleIndex &roles,
                      std::string role_prefix, std::string user_prefix,
                      unsigned depth, unsigned max_depth,
                      std::size_t max_paths, FieldPaths &result,
                      std::set<const Descriptor *> &ancestors) {
  if (!descriptor || result.truncated)
    return;
  if (depth >= max_depth || !ancestors.insert(descriptor).second) {
    result.truncated = true;
    return;
  }
  for (int i = 0; i < descriptor->field_count(); ++i) {
    if (result.paths.size() >= max_paths) {
      result.truncated = true;
      break;
    }
    const auto *field = descriptor->field(i);
    auto role_path = role_prefix;
    append_role_field(role_path, field->number());
    auto user_path = user_prefix;
    append_path_component(user_path, cpp_field_name(field));
    if (user_path.size() > 4096 || !result.paths.insert(user_path).second) {
      if (user_path.size() > 4096)
        result.truncated = true;
      continue;
    }
    discover_field(field, roles, std::move(role_path), std::move(user_path),
                   depth + 1, max_depth, max_paths, result, ancestors);
    if (result.truncated)
      break;
  }
  ancestors.erase(descriptor);
}

void append_bounded(std::string &output, std::string_view text) {
  if (output.size() >= kQueryOutputLimit)
    return;
  const auto available = kQueryOutputLimit - output.size();
  if (text.size() <= available) {
    output.append(text);
    return;
  }
  constexpr std::string_view marker = "...<output-truncated>";
  if (available >= marker.size()) {
    output.append(text.substr(0, available - marker.size()));
    output.append(marker);
  } else {
    output.append(text.substr(0, available));
  }
}

void append_number(std::string &output, std::uint64_t value) {
  append_bounded(output, std::to_string(value));
}

std::string scalar_value(const Message &message, const FD *field,
                         int repeated_index = -1) {
  const auto *reflection = message.GetReflection();
  const bool repeated = repeated_index >= 0;
  switch (field->cpp_type()) {
  case FD::CPPTYPE_INT32:
    return "int32:" + std::to_string(repeated
        ? reflection->GetRepeatedInt32(message, field, repeated_index)
        : reflection->GetInt32(message, field));
  case FD::CPPTYPE_INT64:
    return "int64:" + std::to_string(repeated
        ? reflection->GetRepeatedInt64(message, field, repeated_index)
        : reflection->GetInt64(message, field));
  case FD::CPPTYPE_UINT32:
    return "uint32:" + std::to_string(repeated
        ? reflection->GetRepeatedUInt32(message, field, repeated_index)
        : reflection->GetUInt32(message, field));
  case FD::CPPTYPE_UINT64:
    return "uint64:" + std::to_string(repeated
        ? reflection->GetRepeatedUInt64(message, field, repeated_index)
        : reflection->GetUInt64(message, field));
  case FD::CPPTYPE_DOUBLE:
    return "double:" + std::to_string(repeated
        ? reflection->GetRepeatedDouble(message, field, repeated_index)
        : reflection->GetDouble(message, field));
  case FD::CPPTYPE_FLOAT:
    return "float:" + std::to_string(repeated
        ? reflection->GetRepeatedFloat(message, field, repeated_index)
        : reflection->GetFloat(message, field));
  case FD::CPPTYPE_BOOL:
    return std::string("bool:") + (repeated
        ? reflection->GetRepeatedBool(message, field, repeated_index) ? "true" : "false"
        : reflection->GetBool(message, field) ? "true" : "false");
  case FD::CPPTYPE_ENUM: {
    const auto *value = repeated
        ? reflection->GetRepeatedEnum(message, field, repeated_index)
        : reflection->GetEnum(message, field);
    return "enum:" + std::string(value->name());
  }
  case FD::CPPTYPE_STRING:
    return "string:\"" + (repeated
        ? reflection->GetRepeatedString(message, field, repeated_index)
        : reflection->GetString(message, field)) + "\"";
  case FD::CPPTYPE_MESSAGE:
    break;
  }
  return "<missing>";
}

using Parts = std::vector<std::string_view>;
void emit_message(std::string &, const Message &, const RoleIndex &,
                  std::string, const Parts &, std::size_t, unsigned);
void emit_field(std::string &, const Message &, const FD *, const RoleIndex &,
                std::string, const Parts &, std::size_t, unsigned);

void emit_sequence(std::string &output, const Message &message, const FD *field,
                   const RoleIndex &roles, std::string role_path,
                   const Parts &parts, std::size_t part_index,
                   unsigned depth) {
  const int count = field->is_repeated()
      ? message.GetReflection()->FieldSize(message, field) : 1;
  constexpr int item_limit = 4096;
  if (count > item_limit) {
    append_bounded(output, "repeated<count=");
    append_number(output, static_cast<std::uint64_t>(count));
    append_bounded(output, ">");
    return;
  }
  append_bounded(output, "[");
  const auto value_path = role_path + "/value";
  const auto nested_role = roles.find(value_path);
  const bool wrapped_container = sequence_role(nested_role) ||
                                 map_role(nested_role) ||
                                 nested_role == "optional";
  for (int i = 0; i < count && output.size() < kQueryOutputLimit; ++i) {
    if (i)
      append_bounded(output, ", ");
    const Message *element = nullptr;
    if (field->is_repeated() &&
        field->cpp_type() == FD::CPPTYPE_MESSAGE)
      element = &message.GetReflection()->GetRepeatedMessage(message, field, i);
    else if (!field->is_repeated() &&
             field->cpp_type() == FD::CPPTYPE_MESSAGE)
      element = &message.GetReflection()->GetMessage(message, field);

    if (wrapped_container && element) {
      const auto *items = element->GetDescriptor()->FindFieldByNumber(1);
      const auto element_role = roles.find(value_path);
      const auto inner_path = element_role == "optional"
          ? value_path + "/value" : value_path;
      emit_field(output, *element, items, roles, inner_path, parts, part_index,
                 depth + 1);
    } else if (field->cpp_type() == FD::CPPTYPE_MESSAGE && element) {
      if (part_index < parts.size())
        emit_message(output, *element, roles, value_path, parts, part_index,
                     depth + 1);
      else
        emit_message(output, *element, roles, value_path, {}, 0, depth + 1);
    } else if (part_index < parts.size()) {
      append_bounded(output, "<missing>");
    } else {
      const auto scalar = scalar_value(message, field,
          field->is_repeated() ? i : -1);
      append_bounded(output, scalar);
    }
  }
  append_bounded(output, "]");
}

void emit_field(std::string &output, const Message &message, const FD *field,
                const RoleIndex &roles, std::string role_path,
                const Parts &parts, std::size_t part_index, unsigned depth) {
  if (!field || depth >= 32) {
    append_bounded(output, "<missing>");
    return;
  }
  const auto role = roles.find(role_path);
  const auto *reflection = message.GetReflection();
  if (role == "optional") {
    if (field->cpp_type() != FD::CPPTYPE_MESSAGE) {
      append_bounded(output, "<unset>");
      return;
    }
    if (field->is_repeated()) {
      const auto count = reflection->FieldSize(message, field);
      if (count > 4096) {
        append_bounded(output, "optional<count=");
        append_number(output, static_cast<std::uint64_t>(count));
        append_bounded(output, ">");
        return;
      }
      append_bounded(output, "[");
      for (int i = 0; i < count && output.size() < kQueryOutputLimit; ++i) {
        if (i)
          append_bounded(output, ", ");
        const auto &wrapper = reflection->GetRepeatedMessage(message, field, i);
        emit_field(output, wrapper, wrapper.GetDescriptor()->FindFieldByNumber(1),
                   roles, role_path + "/value", parts, part_index, depth + 1);
      }
      append_bounded(output, "]");
    } else {
      if (!reflection->HasField(message, field)) {
        append_bounded(output, "<unset>");
        return;
      }
      const auto &wrapper = reflection->GetMessage(message, field);
      emit_field(output, wrapper, wrapper.GetDescriptor()->FindFieldByNumber(1),
                 roles, role_path + "/value", parts, part_index, depth + 1);
    }
    return;
  }
  if (map_role(role)) {
    const int count = reflection->FieldSize(message, field);
    constexpr int item_limit = 4096;
    if (count > item_limit) {
      append_bounded(output, "map<count=");
      append_number(output, static_cast<std::uint64_t>(count));
      append_bounded(output, ">");
      return;
    }
    append_bounded(output, "[");
    for (int i = 0; i < count && output.size() < kQueryOutputLimit; ++i) {
      if (i)
        append_bounded(output, ", ");
      const auto &entry = reflection->GetRepeatedMessage(message, field, i);
      const auto *key = entry.GetDescriptor()->FindFieldByNumber(1);
      const auto *value = entry.GetDescriptor()->FindFieldByNumber(2);
      append_bounded(output, scalar_value(entry, key));
      append_bounded(output, "=");
      emit_field(output, entry, value, roles, role_path + "/value", parts,
                 part_index, depth + 1);
    }
    append_bounded(output, "]");
    return;
  }
  if (sequence_role(role)) {
    emit_sequence(output, message, field, roles, std::move(role_path), parts,
                  part_index, depth + 1);
    return;
  }
  if (field->is_repeated()) {
    emit_sequence(output, message, field, roles, std::move(role_path), parts,
                  part_index, depth + 1);
    return;
  }
  if (field->cpp_type() == FD::CPPTYPE_MESSAGE) {
    if (!reflection->HasField(message, field)) {
      append_bounded(output, "<unset>");
      return;
    }
    emit_message(output, reflection->GetMessage(message, field), roles,
                 std::move(role_path), parts, part_index, depth + 1);
    return;
  }
  if (part_index < parts.size()) {
    append_bounded(output, "<missing>");
    return;
  }
  append_bounded(output, scalar_value(message, field));
}

void emit_message(std::string &output, const Message &message,
                  const RoleIndex &roles, std::string role_prefix,
                  const Parts &parts, std::size_t part_index, unsigned depth) {
  if (depth >= 32) {
    append_bounded(output, "<missing>");
    return;
  }
  const auto *descriptor = message.GetDescriptor();
  if (part_index < parts.size()) {
    const FD *field = nullptr;
    for (int i = 0; i < descriptor->field_count(); ++i) {
      const auto *candidate = descriptor->field(i);
      if (cpp_field_name(candidate) == parts[part_index]) {
        field = candidate;
        break;
      }
    }
    if (!field) {
      append_bounded(output, "<missing>");
      return;
    }
    auto role_path = role_prefix;
    append_role_field(role_path, field->number());
    emit_field(output, message, field, roles, std::move(role_path), parts,
               part_index + 1, depth + 1);
    return;
  }
  append_bounded(output, "{");
  for (int i = 0; i < descriptor->field_count() &&
                  output.size() < kQueryOutputLimit; ++i) {
    if (i)
      append_bounded(output, ", ");
    const auto *field = descriptor->field(i);
    append_bounded(output, field->name());
    append_bounded(output, "=");
    auto role_path = role_prefix;
    append_role_field(role_path, field->number());
    emit_field(output, message, field, roles, std::move(role_path), {}, 0,
               depth + 1);
  }
  append_bounded(output, "}");
}

} // namespace

FieldPaths discover_field_paths(const ReceiverSchemaView &schema,
                                std::size_t max_paths, unsigned max_depth) {
  FieldPaths result;
  if (max_paths == 0 || max_depth == 0) {
    result.truncated = true;
    return result;
  }
  const auto *root = schema.root_descriptor();
  if (!root) {
    result.truncated = true;
    return result;
  }
  const RoleIndex roles(schema.container_roles());
  std::set<const Descriptor *> ancestors;
  discover_message(root, roles, {}, {}, 0, max_depth, max_paths, result,
                   ancestors);
  return result;
}

std::string query_field(const Message &message,
                        const ReceiverSchemaView &schema,
                        std::string_view path) {
  if (path.empty() || path.size() > 4096 || !schema.root_descriptor() ||
      message.GetDescriptor() != schema.root_descriptor())
    return "<missing>";
  Parts parts;
  std::size_t begin = 0;
  while (begin < path.size()) {
    const auto dot = path.find('.', begin);
    const auto end = dot == std::string_view::npos ? path.size() : dot;
    if (end == begin || parts.size() >= 32)
      return "<missing>";
    parts.push_back(path.substr(begin, end - begin));
    if (dot == std::string_view::npos)
      break;
    begin = dot + 1;
  }
  if (parts.empty() || path.back() == '.')
    return "<missing>";
  std::string output;
  const RoleIndex roles(schema.container_roles());
  emit_message(output, message, roles, {}, parts, 0, 0);
  return output;
}

} // namespace tracer::context_query

#include "object_snapshot.hpp"
#include "tracer/protocol.hpp"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/wire_format_lite.h>

namespace tracer_repl {
namespace {
using Descriptor = google::protobuf::FieldDescriptor;

template <class Options>
std::string snapshot_option(const Options &options, const char *name) {
  for (const auto &option : options.uninterpreted_option())
    if (option.name_size() == 1 && option.name(0).name_part() == name &&
        option.has_string_value())
      return option.string_value();
  return {};
}

} // namespace

// Shared-context schemas name messages using the canonical C++ type.
std::string context_descriptor_type(std::string_view cpp_type) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (unsigned char byte : cpp_type) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  std::ostringstream name;
  name << "tracer.generated.state.M_" << std::hex << hash;
  return name.str();
}

namespace {
std::string field_path(const std::string &parent, int number) {
  return (parent.empty() ? "" : parent + ".") + std::to_string(number);
}

class SnapshotDecoder {
  std::map<std::string, std::string> roles_;
  std::map<std::string, std::string> types_;
  std::size_t visited_ = 0;
  const std::string &payload_;
  bool context_state_;
  static std::uint64_t
  read_integer(google::protobuf::io::CodedInputStream &input,
               std::string_view bytes, int end) {
    const int begin = input.CurrentPosition();
    std::uint64_t value = 0;
    if (!input.ReadVarint64(&value) || input.CurrentPosition() > end)
      throw std::runtime_error("invalid snapshot integer payload");
    const int count = input.CurrentPosition() - begin;
    if (count < 1 || count > 10 ||
        (count == 10 && static_cast<unsigned char>(
                            bytes[static_cast<std::size_t>(begin + 9)]) > 1))
      throw std::runtime_error("snapshot varint exceeds 64 bits");
    return value;
  }
  static void validate_integer(const Descriptor &field, std::uint64_t value) {
    if ((field.type() == Descriptor::TYPE_BOOL && value > 1) ||
        ((field.type() == Descriptor::TYPE_SINT32 ||
          field.type() == Descriptor::TYPE_UINT32) &&
         value > std::numeric_limits<std::uint32_t>::max()) ||
        ((field.type() == Descriptor::TYPE_INT32 ||
          field.type() == Descriptor::TYPE_ENUM) &&
         value > std::numeric_limits<std::int32_t>::max() &&
         value < std::numeric_limits<std::uint64_t>::max() -
                     std::numeric_limits<std::int32_t>::max()))
      throw std::runtime_error(
          "snapshot integer payload is outside its schema range");
  }

  void validate(const google::protobuf::Descriptor &type,
                std::string_view bytes, const std::string &path, bool wrapper,
                bool map_entry, unsigned depth) {
    if (depth > 32 || ++visited_ > 16384)
      throw std::runtime_error("event snapshot exceeds validation limits");
    using Wire = google::protobuf::internal::WireFormatLite;
    google::protobuf::io::CodedInputStream input(
        reinterpret_cast<const std::uint8_t *>(bytes.data()),
        static_cast<int>(bytes.size()));
    std::set<int> present;
    const auto wire_path = [&](int number) {
      if (map_entry)
        return path + (number == 1 ? "/key" : "/value");
      return wrapper && number == 1 ? path
                                    : field_path(path, number);
    };
    for (std::uint32_t tag; (tag = input.ReadTag()) != 0;) {
      const auto *field = type.FindFieldByNumber(static_cast<int>(tag >> 3));
      if (!field) {
        if (!Wire::SkipField(&input, tag))
          throw std::runtime_error("invalid snapshot wire data");
        continue;
      }
      const auto wire = tag & 7;
      const auto expected = Wire::WireTypeForFieldType(
          static_cast<Wire::FieldType>(field->type()));
      if (wire != static_cast<unsigned>(expected) &&
          !(field->is_repeated() && field->is_packable() &&
            wire == Wire::WIRETYPE_LENGTH_DELIMITED))
        throw std::runtime_error("snapshot field has the wrong wire type");
      if (!field->is_repeated() && !present.insert(field->number()).second)
        throw std::runtime_error("duplicate singular snapshot field");
      present.insert(field->number());
      if (field->cpp_type() == Descriptor::CPPTYPE_MESSAGE) {
        std::uint32_t length = 0;
        if (!input.ReadVarint32(&length) ||
            length > bytes.size() -
                         static_cast<std::size_t>(input.CurrentPosition()))
          throw std::runtime_error("truncated snapshot message");
        const auto body = bytes.substr(
            static_cast<std::size_t>(input.CurrentPosition()), length);
        if (!input.Skip(static_cast<int>(length)))
          throw std::runtime_error("invalid snapshot message length");
        auto child = wire_path(field->number());
        const auto role = roles_.find(child);
        const bool child_map =
            field->is_repeated() && role != roles_.end() &&
            (role->second == "map" || role->second == "unordered_map");
        if (field->is_repeated() && !child_map)
          child += "/value";
        const auto child_role = roles_.find(child);
        bool child_wrapper = false;
        if (!child_map && child_role != roles_.end()) {
          if (child_role->second == "optional") {
            child += "/value";
            child_wrapper = true;
          } else if (field->message_type()->field_count() == 1 &&
                     field->message_type()->field(0)->is_repeated())
            child_wrapper = true;
        }
        validate(*field->message_type(), body, child, child_wrapper, child_map,
                 depth + 1);
      } else if (wire == Wire::WIRETYPE_VARINT) {
        const auto value =
            read_integer(input, bytes, static_cast<int>(bytes.size()));
        validate_integer(*field, value);
      } else if (wire == Wire::WIRETYPE_LENGTH_DELIMITED &&
                 expected == Wire::WIRETYPE_VARINT) {
        std::uint32_t length = 0;
        if (!input.ReadVarint32(&length) ||
            length > bytes.size() -
                         static_cast<std::size_t>(input.CurrentPosition()))
          throw std::runtime_error("invalid packed snapshot length");
        const int end = input.CurrentPosition() + static_cast<int>(length);
        while (input.CurrentPosition() < end) {
          const auto value = read_integer(input, bytes, end);
          validate_integer(*field, value);
        }
      } else if (!Wire::SkipField(&input, tag))
        throw std::runtime_error("invalid snapshot scalar payload");
    }
    if (!input.ConsumedEntireMessage())
      throw std::runtime_error("invalid snapshot wire ending");
    for (int i = 0; i < type.field_count(); ++i) {
      const auto *field = type.field(i);
      const auto role = roles_.find(wire_path(field->number()));
      if (!field->is_repeated() &&
          (role == roles_.end() || role->second != "optional") &&
          !present.count(field->number()))
        throw std::runtime_error("event snapshot is missing field: " +
                                 std::string(field->name()));
    }
  }

  EventValue item(const google::protobuf::Message &message,
                  const Descriptor &field, int index, const std::string &path,
                  unsigned depth) {
    if (depth > 32 || ++visited_ > 16384)
      throw std::runtime_error("event snapshot exceeds decoding limits");
    const auto *r = message.GetReflection();
    const bool repeated = index >= 0;
    switch (field.cpp_type()) {
    case Descriptor::CPPTYPE_BOOL:
      return EventValue{repeated ? r->GetRepeatedBool(message, &field, index)
                                 : r->GetBool(message, &field)};
    case Descriptor::CPPTYPE_INT32:
      return EventValue{
          std::int64_t(repeated ? r->GetRepeatedInt32(message, &field, index)
                                : r->GetInt32(message, &field))};
    case Descriptor::CPPTYPE_INT64:
      return EventValue{
          std::int64_t(repeated ? r->GetRepeatedInt64(message, &field, index)
                                : r->GetInt64(message, &field))};
    case Descriptor::CPPTYPE_UINT32:
      return EventValue{
          std::uint64_t(repeated ? r->GetRepeatedUInt32(message, &field, index)
                                 : r->GetUInt32(message, &field))};
    case Descriptor::CPPTYPE_UINT64:
      return EventValue{
          std::uint64_t(repeated ? r->GetRepeatedUInt64(message, &field, index)
                                 : r->GetUInt64(message, &field))};
    case Descriptor::CPPTYPE_FLOAT:
      return EventValue{double(repeated
                                   ? r->GetRepeatedFloat(message, &field, index)
                                   : r->GetFloat(message, &field))};
    case Descriptor::CPPTYPE_DOUBLE:
      return EventValue{repeated ? r->GetRepeatedDouble(message, &field, index)
                                 : r->GetDouble(message, &field)};
    case Descriptor::CPPTYPE_ENUM:
      return EventValue{std::int64_t(
          repeated ? r->GetRepeatedEnumValue(message, &field, index)
                   : r->GetEnumValue(message, &field))};
    case Descriptor::CPPTYPE_STRING:
      return EventValue{repeated ? r->GetRepeatedString(message, &field, index)
                                 : r->GetString(message, &field)};
    case Descriptor::CPPTYPE_MESSAGE: {
      const auto &nested = repeated
                               ? r->GetRepeatedMessage(message, &field, index)
                               : r->GetMessage(message, &field);
      const auto *type = nested.GetDescriptor();
      const auto parent_path =
          path.size() >= 6 && path.compare(path.size() - 6, 6, "/value") == 0
              ? path.substr(0, path.size() - 6)
              : std::string();
      const auto parent_role = roles_.find(parent_path);
      if (repeated && parent_role != roles_.end() &&
          (parent_role->second == "map" ||
           parent_role->second == "unordered_map")) {
        const auto *key = type->FindFieldByNumber(1),
                   *value = type->FindFieldByNumber(2);
        if (!key || !value)
          throw std::runtime_error("invalid event map schema");
        return EventValue{EventValue::Object{
            {"key",
             decode_field(nested, *key, parent_path + "/key", depth + 1)},
            {"value",
             decode_field(nested, *value, parent_path + "/value", depth + 1)}}};
      }
      const auto role = roles_.find(path);
      if (role != roles_.end() && role->second == "optional") {
        if (!repeated && !r->HasField(message, &field))
          return {};
        const auto *value = type->FindFieldByNumber(1);
        if (!value)
          throw std::runtime_error("invalid optional snapshot schema");
        return decode_field(nested, *value, path + "/value", depth + 1);
      }
      if (role != roles_.end() && role->second != "optional" &&
          type->field_count() == 1 && type->field(0)->is_repeated())
        return decode_field(nested, *type->field(0), path, depth + 1);
      const auto mapped = types_.find(std::string(type->full_name()));
      return EventValue{
          record_fields(nested, path, depth),
          mapped == types_.end() ? std::string() : mapped->second,
          context_state_ ? std::string(type->full_name()) : std::string()};
    }
    }
    throw std::runtime_error("unsupported snapshot field");
  }
  EventValue decode_field(const google::protobuf::Message &message,
                          const Descriptor &field, const std::string &path,
                          unsigned depth) {
    if (!field.is_repeated())
      return item(message, field, -1, path, depth);
    const auto count = message.GetReflection()->FieldSize(message, &field);
    if (count > 4096)
      throw std::runtime_error("event container exceeds decoding limit");
    EventValue::Array values;
    values.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
      values.push_back(item(message, field, i, path + "/value", depth + 1));
    const auto role = roles_.find(path);
    return EventValue{std::move(values), {}, {},
                      role == roles_.end() ? std::string{} : role->second};
  }

  EventValue::Object record_fields(const google::protobuf::Message &message,
                                   const std::string &path, unsigned depth) {
    EventValue::Object fields;
    const auto *type = message.GetDescriptor();
    for (int i = 0; i < type->field_count(); ++i) {
      const auto &member = *type->field(i);
      auto name = snapshot_option(member.options(), "tracer_cpp_field_name");
      if (name.empty())
        name = member.has_json_name() ? member.json_name() : member.name();
      fields.emplace(std::move(name),
                     decode_field(message, member, field_path(path, member.number()),
                                  depth + 1));
    }
    return fields;
  }

public:
  SnapshotDecoder(const std::string &payload,
                  const google::protobuf::RepeatedPtrField<std::string> &roles,
                  const google::protobuf::RepeatedPtrField<std::string> &types,
                  bool context_state)
      : payload_(payload), context_state_(context_state) {
    for (const auto &type : types) {
      const auto split = type.find('=');
      if (split == std::string::npos ||
          !types_.emplace(type.substr(0, split), type.substr(split + 1)).second)
        throw std::runtime_error("invalid snapshot record types");
    }
    for (const auto &role : roles) {
      const auto split = role.find('=');
      if (split == std::string::npos ||
          !roles_.emplace(role.substr(0, split), role.substr(split + 1)).second)
        throw std::runtime_error("invalid snapshot container roles");
    }
  }

  EventValue decode(const google::protobuf::Message &message,
                    const std::string &type, bool wrapped) {
    if (!wrapped) {
      validate(*message.GetDescriptor(), payload_, "", false, false, 0);
      return EventValue{record_fields(message, "", 0), type};
    }
    validate(*message.GetDescriptor(), payload_, "1", true, false, 0);
    const auto *field = message.GetDescriptor()->FindFieldByNumber(1);
    if (!field || message.GetDescriptor()->field_count() != 1)
      throw std::runtime_error("invalid snapshot root schema");
    auto result = decode_field(message, *field, "1", 0);
    return EventValue{result.data(), type, {}, result.container_role()};
  }
};

} // namespace

EventValue decode_owned_value(
    const std::string &cpp_type, const std::string &root_type,
    const std::string &descriptors, const std::string &payload,
    const google::protobuf::RepeatedPtrField<std::string> &roles,
    const google::protobuf::RepeatedPtrField<std::string> &types,
    bool context_state, bool wrapped) {
  const auto descriptor_limit = context_state ? 1024 * 1024 : 16384;
  const auto payload_limit = context_state ? 256 * 1024 : 65536;
  if (cpp_type.empty() || root_type.empty() ||
      descriptors.size() > static_cast<std::size_t>(descriptor_limit) ||
      payload.size() > static_cast<std::size_t>(payload_limit))
    throw std::runtime_error("invalid or oversized event snapshot");
  google::protobuf::FileDescriptorSet files;
  if (!files.ParseFromString(descriptors) ||
      files.file_size() > 16)
    throw std::runtime_error("invalid event snapshot descriptors");
  google::protobuf::DescriptorPool pool;
  std::vector<bool> built(static_cast<std::size_t>(files.file_size()));
  std::size_t remaining = built.size();
  while (remaining) {
    bool progress = false;
    for (int i = 0; i < files.file_size(); ++i) {
      if (built[static_cast<std::size_t>(i)])
        continue;
      bool ready = true;
      for (const auto &dependency : files.file(i).dependency())
        ready &= pool.FindFileByName(dependency) != nullptr;
      if (ready) {
        if (!pool.BuildFile(files.file(i)))
          throw std::runtime_error("invalid event snapshot schema");
        built[static_cast<std::size_t>(i)] = true;
        --remaining;
        progress = true;
      }
    }
    if (!progress)
      throw std::runtime_error("unresolved snapshot schema dependencies");
  }
  const auto *root = pool.FindMessageTypeByName(root_type);
  if (!root)
    throw std::runtime_error("snapshot root type not found");
  google::protobuf::DynamicMessageFactory factory(&pool);
  const auto *prototype = factory.GetPrototype(root);
  if (!prototype)
    throw std::runtime_error("invalid snapshot root prototype");
  std::unique_ptr<google::protobuf::Message> value(prototype->New());
  if (!value->ParseFromString(payload))
    throw std::runtime_error("invalid event snapshot payload");
  return SnapshotDecoder(payload, roles, types, context_state).decode(*value, cpp_type, wrapped);
}

EventValue decode_snapshot(const clangtracer::context::v1::ValueSnapshot &snapshot,
                           bool include_members) {
  using Snapshot = clangtracer::context::v1::ValueSnapshot;
  if (snapshot.status() != Snapshot::COMPLETE)
    throw std::runtime_error("event snapshot unavailable: " + snapshot.detail());
  auto value = decode_owned_value(snapshot.cpp_type(), snapshot.root_type(),
                                 snapshot.descriptor_set(), snapshot.payload(),
                                 snapshot.container_roles(), snapshot.record_types(),
                                 false, true);
  if (snapshot.members().empty())
    return value;
  if (!include_members ||
      snapshot.members_size() > static_cast<int>(tracer::kMaxSnapshotMembers))
    throw std::runtime_error("invalid event snapshot member count or depth");
  auto fields = value.fields();
  std::set<std::string> names;
  for (const auto &member : snapshot.members()) {
    if (member.name().empty() || !member.has_snapshot() ||
        fields.count(member.name()) || !names.insert(member.name()).second)
      throw std::runtime_error("invalid or duplicate event snapshot member");
    if (member.snapshot().status() == Snapshot::COMPLETE)
      fields.emplace(member.name(), decode_snapshot(member.snapshot(), false));
  }
  return EventValue{std::move(fields), value.cpp_type()};
}

namespace {
std::optional<std::string> delta_scalar(const EventValue &value) {
  const auto &data = value.data();
  if (value.is_null())
    return "null";
  if (const auto *text = std::get_if<std::string>(&data))
    return "\"" + google::protobuf::CEscape(*text) + "\"";
  if (const auto *boolean = std::get_if<bool>(&data))
    return *boolean ? "true" : "false";
  if (const auto *integer = std::get_if<std::int64_t>(&data))
    return std::to_string(*integer);
  if (const auto *integer = std::get_if<std::uint64_t>(&data))
    return std::to_string(*integer);
  if (const auto *real = std::get_if<double>(&data)) {
    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<double>::max_digits10) << *real;
    return out.str();
  }
  return std::nullopt;
}

class ReceiverDelta {
  using Array = EventValue::Array;
  using KeyIndex = std::map<std::string, std::size_t>;
  std::map<std::string, std::string> keys_;
  std::vector<SnapshotChange> changes_;

  void change(const std::string &path, std::string detail,
              const EventValue *before = nullptr, const EventValue *after = nullptr) {
    SnapshotChange change{path, std::move(detail), false};
    if (before && delta_scalar(*before))
      change.before = *before;
    if (after && delta_scalar(*after))
      change.after = *after;
    changes_.push_back(std::move(change));
  }

  void gap(const std::string &path, std::string_view reason) {
    changes_.push_back({path, std::string(reason), true});
  }

  static std::optional<KeyIndex> index(const Array &items, const std::string &member,
                                       bool associative) {
    KeyIndex result;
    for (std::size_t i = 0; i < items.size(); ++i) {
      const auto *record = std::get_if<EventValue::Object>(&items[i].data());
      if (!record || (associative && !record->count("value")))
        return std::nullopt;
      const auto field = record->find(member);
      if (field == record->end() || field->second.is_null())
        return std::nullopt;
      const auto key = delta_scalar(field->second);
      if (!key || !result.emplace(*key, i).second)
        return std::nullopt;
    }
    return result;
  }

  void array(const std::string &path, const Array &before, const Array &after,
             bool associative) {
    const auto policy = keys_.find(path);
    if (policy == keys_.end()) {
      if (associative) {
        gap(path, "key identity policy unavailable");
        return;
      }
      for (std::size_t i = 0; i < std::min(before.size(), after.size()); ++i)
        compare(path + "[" + std::to_string(i) + "]", before[i], after[i]);
      if (before.size() != after.size())
        change(path + ".size", std::to_string(before.size()) + " -> " +
                                  std::to_string(after.size()));
      return;
    }
    const auto &member = policy->second;
    if (associative && member != "key") {
      gap(path, "associative identity must select key");
      return;
    }
    const auto old_index = index(before, member, associative);
    const auto new_index = index(after, member, associative);
    if (!old_index || !new_index) {
      gap(path, "identity missing, unsupported, or duplicate");
      return;
    }
    const auto member_path = [&](const std::string &key) {
      return path + "[" + (associative ? key : member + "=" + key) + "]";
    };
    for (const auto &[key, i] : *old_index) {
      const auto found = new_index->find(key);
      const auto child = member_path(key);
      if (found == new_index->end()) {
        change(child, "removed", associative ? &before[i].field("value") : &before[i]);
      } else {
        const auto j = found->second;
        if (!associative && i != j)
          change(child, "moved " + std::to_string(i) + " -> " + std::to_string(j));
        compare(child, associative ? before[i].field("value") : before[i],
                       associative ? after[j].field("value") : after[j]);
      }
    }
    for (const auto &[key, i] : *new_index)
      if (!old_index->count(key))
        change(member_path(key), "added", nullptr, associative ? &after[i].field("value") : &after[i]);
  }

  bool compare_tagged(const std::string &path, const EventValue &before, const EventValue &after) {
    const auto &type = before.cpp_type();
    if (type.starts_with("std::variant<") || type.starts_with("::std::variant<")) {
      const auto &old = before.field("index");
      const auto &next = after.field("index");
      const auto old_index = old.as<std::uint64_t>(), next_index = next.as<std::uint64_t>();
      if (old_index != next_index) {
        const auto text = [](std::uint64_t index) {
          return index == UINT64_MAX ? "valueless" : "index " + std::to_string(index);
        };
        change(path, text(old_index) + " -> " + text(next_index), &old, &next);
      } else if (old_index != UINT64_MAX) {
        const auto member = "alternative_" + std::to_string(old_index);
        compare(path + "." + member, before.field(member), after.field(member));
      }
      return true;
    }
    if (type == "std::any" || type == "::std::any") {
      const auto &old = before.field("kind");
      const auto &next = after.field("kind");
      const auto old_kind = old.as<std::string>(), next_kind = next.as<std::string>();
      if (old_kind != next_kind)
        change(path, old_kind + " -> " + next_kind, &old, &next);
      if (old_kind == "opaque" || next_kind == "opaque") {
        gap(path, "opaque std::any payload");
      } else if (old_kind == next_kind && old_kind != "empty") {
        compare(path + "." + old_kind, before.field(old_kind + "_value"),
                                       after.field(old_kind + "_value"));
      }
      return true;
    }
    const auto *old = std::get_if<EventValue::Object>(&before.data());
    const auto *next = std::get_if<EventValue::Object>(&after.data());
    const bool owning_pointer = type.starts_with("std::shared_ptr<") || type.starts_with("std::unique_ptr<") ||
        type.starts_with("::std::shared_ptr<") || type.starts_with("::std::unique_ptr<");
    if (owning_pointer && old && next && old->count("suppressed") && next->count("suppressed") &&
        (before.field("suppressed").as<bool>() || after.field("suppressed").as<bool>())) {
      compare(path + ".address", before.field("address"), after.field("address"));
      gap(path + ".value", "pointee alias or capture graph budget exhausted");
      return true;
    }
    return false;
  }

  void compare(const std::string &path, const EventValue &before, const EventValue &after) {
    if (before.cpp_type() != after.cpp_type()) {
      change(path, "type " + before.cpp_type() + " -> " + after.cpp_type());
      return;
    }
    if (compare_tagged(path, before, after))
      return;
    if (const auto *old = std::get_if<EventValue::Object>(&before.data())) {
      if (const auto *next = std::get_if<EventValue::Object>(&after.data())) {
        for (const auto &[name, value] : *old) {
          if ((value.cpp_type() == "std::any" || value.cpp_type() == "::std::any") &&
              old->count(name + "_snapshot") && next->count(name + "_snapshot"))
            continue;
          const auto member = path.empty() ? name : path + "." + name;
          const auto found = next->find(name);
          if (found == next->end())
            gap(member, "missing return sample");
          else
            compare(member, value, found->second);
        }
        for (const auto &[name, value] : *next)
          if (!old->count(name))
            gap(path.empty() ? name : path + "." + name, "missing entry sample");
        return;
      }
    }
    if (const auto *old = std::get_if<Array>(&before.data())) {
      if (const auto *next = std::get_if<Array>(&after.data())) {
        const auto &role = before.container_role();
        if (role != after.container_role())
          gap(path, "incompatible container roles");
        else
          array(path, *old, *next, role == "map" || role == "unordered_map");
        return;
      }
    }
    const auto old = delta_scalar(before), next = delta_scalar(after);
    if (old && next) {
      if (old != next || before.data().index() != after.data().index())
        change(path, *old + " -> " + *next, &before, &after);
    } else {
      gap(path, "incompatible snapshot shapes");
    }
  }

public:
  explicit ReceiverDelta(const std::vector<std::string> &keys) {
    for (const auto &rule : keys) {
      const auto split = rule.find('=');
      if (split == 0 || split == std::string::npos || split + 1 == rule.size() ||
          !keys_.emplace(rule.substr(0, split), rule.substr(split + 1)).second)
        throw std::invalid_argument("--key requires one unique PATH=MEMBER rule per container");
    }
  }

  std::vector<SnapshotChange> run(const EventValue &before, const EventValue &after) {
    compare("", before, after);
    return std::move(changes_);
  }
};
} // namespace

std::vector<SnapshotChange> compare_snapshots(const Snapshot &before, const Snapshot &after,
                                             const std::vector<std::string> &keys,
                                             const std::vector<std::string> &fields) {
  ReceiverDelta diff(keys);
  try {
    const auto old = decode_snapshot(before);
    const auto next = decode_snapshot(after);
    auto changes = diff.run(old, next);
    const auto contains_path = [](const std::string &parent, const std::string &path) {
      return path == parent || (path.size() > parent.size() &&
          path.compare(0, parent.size(), parent) == 0 &&
          (path[parent.size()] == '.' || path[parent.size()] == '['));
    };
    if (!fields.empty()) {
      changes.erase(std::remove_if(changes.begin(), changes.end(), [&](const auto &change) {
        return !change.path.empty() && std::none_of(fields.begin(), fields.end(),
            [&](const auto &field) {
              return contains_path(field, change.path) || contains_path(change.path, field);
            });
      }), changes.end());
      const auto exists = [](const EventValue &root, std::string_view path) {
        const EventValue *value = &root;
        while (!path.empty()) {
          const auto split = path.find_first_of(".[");
          const auto *record = std::get_if<EventValue::Object>(&value->data());
          if (!record)
            return false;
          const auto found = record->find(std::string(path.substr(0, split)));
          if (found == record->end())
            return false;
          if (split == std::string_view::npos || path[split] == '[')
            return true;
          value = &found->second;
          path.remove_prefix(split + 1);
        }
        return true;
      };
      for (const auto &field : fields)
        if ((!exists(old, field) || !exists(next, field)) &&
            std::none_of(changes.begin(), changes.end(), [&](const auto &change) {
              return change.gap && contains_path(change.path, field);
            }))
          changes.push_back({field, "selected field unavailable in one or both samples", true});
    }
    std::map<std::string, std::string> unavailable;
    for (const auto *sample : {&before, &after})
      for (const auto &member : sample->members())
        if (member.snapshot().status() != Snapshot::COMPLETE &&
            (fields.empty() || std::any_of(fields.begin(), fields.end(), [&](const auto &field) {
              return contains_path(member.name(), field);
            })))
          unavailable.try_emplace(member.name(), member.snapshot().detail());
    changes.erase(std::remove_if(changes.begin(), changes.end(), [&](const auto &change) {
      return change.gap && unavailable.count(change.path);
    }), changes.end());
    for (const auto &[path, detail] : unavailable)
      changes.push_back({path, detail.empty() ? "selected snapshot member is unavailable" : detail, true});
    return changes;
  } catch (const std::exception &error) {
    return {{"receiver", error.what(), true}};
  }
}

void validate_snapshot_keys(const std::vector<std::string> &keys) {
  (void)ReceiverDelta(keys);
}

// The native REPL JIT needs a stable destructor for snapshot temporaries.
EventValue::~EventValue() = default;

bool EventValue::matches_record_type(std::string_view name) const {
  if (!type_.empty())
    return std::string_view(type_) == name;
  return !descriptor_type_.empty() && descriptor_type_ == context_descriptor_type(name);
}

} // namespace tracer_repl

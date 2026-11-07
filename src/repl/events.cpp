#include "events.hpp"
#include "../prepared_contract.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>

#include <fcntl.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/wire_format_lite.h>
#include <sqlite3.h>
#include <unistd.h>

namespace tracer_repl {
namespace api = clangtracer::v1;

namespace {
constexpr std::uint32_t max_frame = 32 * 1024 * 1024;

void write_all(int fd, const void *data, std::size_t size) {
  auto *next = static_cast<const char *>(data);
  while (size) {
    const auto n = ::write(fd, next, size);
    if (n < 0 && errno == EINTR) {
      continue;
    }
    if (n <= 0) {
      throw std::runtime_error("recording write failed: " +
                               std::string(std::strerror(errno)));
    }
    next += n;
    size -= static_cast<std::size_t>(n);
  }
}

template <class Value>
std::string scalar_text(const Value &value, unsigned type) {
  std::ostringstream out;
  switch (type) {
  case api::VOID:
    return "void";
  case api::BOOL:
    return value.unsigned_value() || value.signed_value() ? "true" : "false";
  case api::INT32:
  case api::INT64:
    out << value.signed_value();
    break;
  case api::FLOAT32:
  case api::FLOAT64:
    out << std::setprecision(17) << value.real_value();
    break;
  case api::POINTER:
  case api::OBJECT:
    out << "0x" << std::hex << value.unsigned_value();
    break;
  case api::UINT64:
    out << value.unsigned_value();
    break;
  default:
    return "unavailable";
  }
  return out.str();
}

std::string
context_value(const clangtracer::context::v1::TypedTraceValue &value) {
  return scalar_text(value, value.value_type());
}

auto scalar_payload_case(const api::Value &value) { return value.data_case(); }

auto scalar_payload_case(
    const clangtracer::context::v1::TypedTraceValue &value) {
  return value.value_case();
}

template <class Value>
ScalarValue typed_scalar(const Value &value, unsigned type) {
  const auto payload = scalar_payload_case(value);
  switch (type) {
  case api::BOOL:
    if (payload == Value::kUnsignedValue && value.unsigned_value() <= 1)
      return value.unsigned_value() != 0;
    if (payload == Value::kSignedValue && value.signed_value() >= 0 &&
        value.signed_value() <= 1)
      return value.signed_value() != 0;
    break;
  case api::INT32:
  case api::INT64:
    if (payload == Value::kSignedValue &&
        (type == api::INT64 ||
         (value.signed_value() >= std::numeric_limits<std::int32_t>::min() &&
          value.signed_value() <= std::numeric_limits<std::int32_t>::max())))
      return static_cast<std::int64_t>(value.signed_value());
    break;
  case api::UINT64:
    if (payload == Value::kUnsignedValue)
      return static_cast<std::uint64_t>(value.unsigned_value());
    break;
  case api::FLOAT32:
  case api::FLOAT64:
    if (payload == Value::kRealValue &&
        (type == api::FLOAT64 || !std::isfinite(value.real_value()) ||
         (value.real_value() >= std::numeric_limits<float>::lowest() &&
          value.real_value() <= std::numeric_limits<float>::max())))
      return value.real_value();
    break;
  default:
    throw std::invalid_argument(
        "event value is not a bool or numeric scalar (type=" +
        std::to_string(type) + ")");
  }
  throw std::invalid_argument(
      "event scalar has a missing, invalid, or mismatched payload");
}

using Descriptor = google::protobuf::FieldDescriptor;

template <class Options>
std::string snapshot_option(const Options &options, const char *name) {
  for (const auto &option : options.uninterpreted_option())
    if (option.name_size() == 1 && option.name(0).name_part() == name &&
        option.has_string_value())
      return option.string_value();
  return {};
}

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
    return EventValue{std::move(values)};
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
    return EventValue{result.data(), type};
  }
};

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

EventValue decode_snapshot(const clangtracer::context::v1::ValueSnapshot &snapshot) {
  using Snapshot = clangtracer::context::v1::ValueSnapshot;
  if (snapshot.status() != Snapshot::COMPLETE)
    throw std::runtime_error("event snapshot unavailable: " + snapshot.detail());
  return decode_owned_value(snapshot.cpp_type(), snapshot.root_type(),
                            snapshot.descriptor_set(), snapshot.payload(),
                            snapshot.container_roles(), snapshot.record_types(),
                            false, true);
}

void decoded_states(const api::TraceEvent &event, std::ostream &out) {
  if (!event.has_context()) {
    return;
  }
  for (const auto &state : event.context().event().states()) {
    for (const auto &schema : event.context().schemas()) {
      const bool matches =
          state.epoch_schema_id()
              ? schema.epoch_schema_id() == state.epoch_schema_id()
              : !state.schema_identity().empty() &&
                    schema.schema_identity() == state.schema_identity();
      if (!matches) {
        continue;
      }
      google::protobuf::FileDescriptorSet descriptors;
      if (!descriptors.ParseFromString(
              schema.serialized_file_descriptor_set())) {
        out << "state decode failed: invalid descriptor set\n";
        continue;
      }
      google::protobuf::DescriptorPool pool;
      std::vector<bool> built(
          static_cast<std::size_t>(descriptors.file_size()));
      std::size_t remaining = built.size();
      while (remaining) {
        bool progress = false;
        for (int i = 0; i < descriptors.file_size(); ++i) {
          if (built[static_cast<std::size_t>(i)]) {
            continue;
          }
          const auto &file = descriptors.file(i);
          bool ready = true;
          for (const auto &dep : file.dependency()) {
            if (!pool.FindFileByName(dep)) {
              ready = false;
            }
          }
          if (ready && pool.BuildFile(file)) {
            built[static_cast<std::size_t>(i)] = true;
            --remaining;
            progress = true;
          }
        }
        if (!progress) {
          break;
        }
      }
      const auto *descriptor = pool.FindMessageTypeByName(schema.root_type());
      if (!descriptor) {
        out << "state decode failed: missing type " << schema.root_type()
            << '\n';
        continue;
      }
      google::protobuf::DynamicMessageFactory factory(&pool);
      const auto *prototype = factory.GetPrototype(descriptor);
      if (!prototype) {
        continue;
      }
      std::unique_ptr<google::protobuf::Message> message(prototype->New());
      if (!message->ParseFromString(state.payload())) {
        out << "state decode failed: invalid payload\n";
        continue;
      }
      std::string json;
      if (google::protobuf::util::MessageToJsonString(*message, &json).ok()) {
        out << "decoded state " << schema.cpp_state_type() << " " << json
            << '\n';
      }
    }
  }
}
} // namespace

Event::Event(std::shared_ptr<const EventData> data) : data_(std::move(data)) {
  if (!data_) {
    throw std::invalid_argument("empty event");
  }
}

std::uint64_t Event::sequence() const { return data_->message.sequence(); }

std::uint64_t Event::probe_id() const { return data_->message.probe_id(); }

std::uint64_t Event::call_id() const { return data_->message.call_id(); }

std::string Event::phase() const {
  return api::TracePhase_Name(data_->message.phase());
}

std::string Event::function_name() const {
  return data_->message.function_name();
}

bool Event::has_result() const {
  const auto &event = data_->message;
  return (event.has_context() && event.context().event().has_result()) ||
         event.has_result();
}

ScalarValue Event::argument_value(std::size_t index) const {
  const auto &event = data_->message;
  if (event.has_context()) {
    for (const auto &arg : event.context().event().arguments()) {
      if (arg.index() == index)
        return typed_scalar(arg.value(), arg.value().value_type());
    }
    throw std::out_of_range("argument index");
  }
  if (index >= static_cast<std::size_t>(event.arguments_size()))
    throw std::out_of_range("argument index");
  const auto &arg = event.arguments(static_cast<int>(index));
  return typed_scalar(arg, arg.type());
}

ScalarValue Event::result_value() const {
  const auto &event = data_->message;
  if (event.has_context() && event.context().event().has_result()) {
    const auto &value = event.context().event().result();
    return typed_scalar(value, value.value_type());
  }
  if (!event.has_result())
    throw std::runtime_error("event result unavailable; inspect a POST event");
  return typed_scalar(event.result(), event.result().type());
}

// The native REPL JIT needs a stable destructor for snapshot temporaries.
EventValue::~EventValue() = default;

bool EventValue::matches_record_type(std::string_view name) const {
  if (!type_.empty())
    return std::string_view(type_) == name;
  return !descriptor_type_.empty() && descriptor_type_ == context_descriptor_type(name);
}


EventValue Event::argument_snapshot(std::size_t index) const {
  const auto &event = data_->message;
  if (event.has_context()) {
    for (const auto &argument : event.context().event().arguments()) {
      if (argument.index() == index) {
        if (!argument.value().has_snapshot())
          return std::visit(
              [](auto value) { return EventValue{value}; },
              typed_scalar(argument.value(), argument.value().value_type()));
        return decode_snapshot(argument.value().snapshot());
      }
    }
    throw std::out_of_range("argument index");
  }
  if (index >= static_cast<std::size_t>(event.arguments_size()))
    throw std::out_of_range("argument index");
  const auto &argument = event.arguments(static_cast<int>(index));
  if (!argument.has_snapshot())
    return std::visit([](auto value) { return EventValue{value}; },
                      typed_scalar(argument, argument.type()));
  return decode_snapshot(argument.snapshot());
}

EventValue Event::state_snapshot(std::size_t index) const {
  const auto &event = data_->message;
  if (!event.has_context() ||
      index >= static_cast<std::size_t>(event.context().event().states_size()))
    throw std::out_of_range("event state index");
  const auto &state = event.context().event().states(static_cast<int>(index));
  if (!state.truncations().empty())
    throw std::runtime_error("event state was truncated at capture");
  for (const auto &schema : event.context().schemas()) {
    const bool matches = state.epoch_schema_id()
        ? schema.epoch_schema_id() == state.epoch_schema_id()
        : !state.schema_identity().empty() &&
              schema.schema_identity() == state.schema_identity();
    if (!matches)
      continue;
    const auto &type = schema.cpp_state_type();
    const bool wrapped = schema.root_type() ==
        context_descriptor_type("__tracer_payload_root<" + type + ">");
    if (!wrapped && schema.root_type() != context_descriptor_type(type))
      throw std::runtime_error("event state type does not match its schema");
    return decode_owned_value(type, schema.root_type(),
                              schema.serialized_file_descriptor_set(),
                              state.payload(), schema.container_roles(), {},
                              true, wrapped);
  }
  throw std::runtime_error("event state schema is unavailable");
}

EventValue Event::receiver_snapshot() const {
  const auto &event = data_->message;
  if (!event.has_receiver_snapshot())
    throw std::runtime_error("event has no receiver snapshot");
  return decode_snapshot(event.receiver_snapshot());
}

EventValue Event::result_snapshot() const {
  const auto &event = data_->message;
  if (event.has_context() && event.context().event().has_result()) {
    const auto &result = event.context().event().result();
    if (!result.has_snapshot())
      return std::visit([](auto value) { return EventValue{value}; },
                        typed_scalar(result, result.value_type()));
    return decode_snapshot(result.snapshot());
  }
  if (!event.has_result())
    throw std::runtime_error("event result unavailable; inspect a POST event");
  if (!event.result().has_snapshot())
    return std::visit([](auto value) { return EventValue{value}; },
                      typed_scalar(event.result(), event.result().type()));
  return decode_snapshot(event.result().snapshot());
}

std::string Event::argument(std::size_t index) const {
  const auto &event = data_->message;
  if (event.has_context()) {
    for (const auto &arg : event.context().event().arguments()) {
      if (arg.index() == index) {
        return context_value(arg.value());
      }
    }
    throw std::out_of_range("argument index");
  }
  if (index >= static_cast<std::size_t>(event.arguments_size())) {
    throw std::out_of_range("argument index");
  }
  return value_text(event.arguments(static_cast<int>(index)));
}

std::string Event::result() const {
  const auto &event = data_->message;
  if (event.has_context() && event.context().event().has_result()) {
    return context_value(event.context().event().result());
  }
  return event.has_result() ? value_text(event.result()) : "unavailable";
}

std::string Event::describe() const {
  std::ostringstream out;
  out << event_summary(data_->message) << '\n' << data_->message.DebugString();
  decoded_states(data_->message, out);
  return out.str();
}

std::string value_text(const api::Value &value) {
  return scalar_text(value, value.type());
}

std::string event_summary(const api::TraceEvent &event) {
  std::ostringstream out;
  out << "cursor=" << event.sequence() << " session=" << event.session_id()
      << " pid=" << event.pid() << " tid=" << event.tid()
      << " probe=" << event.probe_id()
      << " phase=" << api::TracePhase_Name(event.phase())
      << " call=" << event.call_id() << " function=" << event.function_name()
      << " args=[";
  if (event.has_context()) {
    bool first = true;
    for (const auto &arg : event.context().event().arguments()) {
      if (!first) {
        out << ", ";
      }
      first = false;
      out << arg.name() << '=' << context_value(arg.value());
    }
  } else {
    for (int i = 0; i < event.arguments_size(); ++i) {
      if (i) {
        out << ", ";
      }
      for (const auto &named : event.named_arguments()) {
        if (named.argument_index() == static_cast<unsigned>(i)) {
          out << named.name() << '=';
        }
      }
      out << value_text(event.arguments(i));
    }
  }
  out << "]";
  for (const auto &field : event.object_fields()) {
    out << " " << field.name() << '=' << value_text(field.value());
  }
  if (event.phase() == api::POST) {
    if (event.has_context() && event.context().event().has_result()) {
      out << " result=" << context_value(event.context().event().result());
    } else if (event.has_result()) {
      out << " result=" << value_text(event.result());
    }
  }
  out << " lost(agent=" << event.agent_dropped_events()
      << ",server=" << event.server_dropped_events() << ')';
  return out.str();
}

EventBuffer::EventBuffer(std::size_t events, std::size_t bytes)
    : max_events_(events), max_bytes_(bytes) {
  if (!events || !bytes) {
    throw std::invalid_argument("buffer limits must be positive");
  }
}

void EventBuffer::push(const api::TraceEvent &event) {
  ++received;
  auto item = std::make_shared<EventData>();
  item->message = event;
  // Include Protobuf's resident allocation estimate, not just its smaller
  // wire representation. Container/control-block overhead is separately small
  // and bounded by the event-count limit.
  const auto size = item->message.SpaceUsedLong() + sizeof(std::size_t);
  item->retained_bytes = size;
  if (size > max_bytes_) {
    ++evicted;
    return;
  }
  while (!events_.empty() &&
         (events_.size() >= max_events_ || bytes_ > max_bytes_ - size)) {
    bytes_ -= events_.front()->retained_bytes;
    events_.pop_front();
    ++evicted;
  }
  bytes_ += size;
  events_.push_back(std::move(item));
}

Collection EventBuffer::snapshot() const {
  Collection result;
  result.received = received;
  result.evicted = evicted;
  result.complete = complete;
  result.cancelled = cancelled;
  result.timed_out = timed_out;
  for (const auto &event : events_) {
    result.events.emplace_back(event);
  }
  return result;
}

Collection::~Collection() = default;

void event_statistics(const EventBuffer &buffer, std::ostream &out,
                      bool calls) {
  using Key = std::tuple<std::string, std::string, std::uint64_t, std::uint32_t,
                         std::uint64_t>;

  struct Pair {
    const api::TraceEvent *pre = nullptr, *post = nullptr;
  };

  std::map<Key, Pair> pairs;
  using ProbeKey = std::tuple<std::string, std::string, std::uint64_t>;
  std::map<ProbeKey, std::uint64_t> agent_losses;
  std::map<std::string, std::uint64_t> server_losses;
  std::uint64_t pre = 0, post = 0, other = 0, agent = 0, server = 0;
  for (const auto &item : buffer.events()) {
    const auto &e = item->message;
    auto &probe_loss =
        agent_losses[{e.server_instance_id(), e.session_id(), e.probe_id()}];
    probe_loss = std::max(probe_loss, e.agent_dropped_events());
    auto &server_loss = server_losses[e.server_instance_id()];
    server_loss = std::max(server_loss, e.server_dropped_events());
    if (!e.has_context() && (e.phase() == api::PRE || e.phase() == api::POST)) {
      auto &pair = pairs[{e.server_instance_id(), e.session_id(), e.probe_id(),
                          e.tid(), e.call_id()}];
      if (e.phase() == api::PRE) {
        pair.pre = &e;
      }
      if (e.phase() == api::POST) {
        pair.post = &e;
      }
    }
    if (e.phase() == api::PRE) {
      ++pre;
    }
    if (e.phase() == api::POST) {
      ++post;
    }
    if (e.phase() != api::PRE && e.phase() != api::POST)
      ++other;
  }
  const auto add_loss = [](std::uint64_t &total, std::uint64_t value) {
    total = value > std::numeric_limits<std::uint64_t>::max() - total
                ? std::numeric_limits<std::uint64_t>::max()
                : total + value;
  };
  for (const auto &entry : agent_losses)
    add_loss(agent, entry.second);
  for (const auto &entry : server_losses)
    add_loss(server, entry.second);
  std::size_t matched = 0;
  for (const auto &entry : pairs) {
    const auto &pair = entry.second;
    if (pair.pre && pair.post) {
      ++matched;
    }
    if (calls) {
      const auto *e = pair.pre ? pair.pre : pair.post;
      out << "session=" << e->session_id() << " probe=" << e->probe_id()
          << " tid=" << e->tid() << " call=" << e->call_id()
          << " PRE=" << (pair.pre ? "present" : "missing")
          << " POST=" << (pair.post ? "present" : "missing");
      if (pair.pre && pair.post && pair.pre->monotonic_time_ns() &&
          pair.post->monotonic_time_ns() >= pair.pre->monotonic_time_ns()) {
        out << " duration_ns="
            << pair.post->monotonic_time_ns() - pair.pre->monotonic_time_ns();
      }
      out << '\n';
    }
  }
  out << "received=" << buffer.received
      << " retained=" << buffer.events().size() << " bytes=" << buffer.bytes()
      << " evicted=" << buffer.evicted << " PRE=" << pre << " POST=" << post
      << " other_phases=" << other << " paired=" << matched
      << " incomplete=" << pairs.size() - matched << " lost(agent=" << agent
      << ",server=" << server << ")\n";
  if (calls) {
    out << "shared-context events retain invocation/task identities in event "
           "details; they are not "
           "paired by native thread/call fields\n";
  }
}

int create_recording(const std::string &path) {
  const int fd =
      ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
  if (fd < 0) {
    throw std::runtime_error("cannot create new recording: " + path + ": " +
                             std::strerror(errno));
  }
  return fd;
}

void write_record(int fd, const api::TraceEvent &event) {
  const auto bytes = event.SerializeAsString();
  if (bytes.empty() || bytes.size() > max_frame) {
    throw std::runtime_error("event exceeds recording frame limit");
  }
  const auto size = static_cast<std::uint32_t>(bytes.size());
  unsigned char prefix[4];
  for (unsigned i = 0; i < 4; ++i) {
    prefix[i] = static_cast<unsigned char>(size >> (8 * i));
  }
  write_all(fd, prefix, sizeof(prefix));
  write_all(fd, bytes.data(), bytes.size());
}

namespace {
void read_recorded_types(sqlite3 *db, EventBuffer &buffer) {
  const auto statement = [&](const char *sql) {
    sqlite3_stmt *raw = nullptr;
    const auto status = sqlite3_prepare_v2(db, sql, -1, &raw, nullptr);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> rows(raw, sqlite3_finalize);
    if (status != SQLITE_OK)
      throw std::runtime_error(std::string("cannot read recorded types: ") + sqlite3_errmsg(db));
    return rows;
  };
  const auto step = [&](sqlite3_stmt *rows) {
    const auto status = sqlite3_step(rows);
    if (status != SQLITE_ROW && status != SQLITE_DONE)
      throw std::runtime_error(std::string("cannot read recorded types: ") + sqlite3_errmsg(db));
    return status == SQLITE_ROW;
  };
  if (std::none_of(buffer.events().begin(), buffer.events().end(), [](const auto &event) {
        return !event->message.context().event().states().empty();
      }))
    return;
  const auto table = statement("SELECT 1 FROM sqlite_master WHERE type = 'table' "
                               "AND name = 'trace_event_types'");
  if (!step(table.get()))
    return; // Recordings made before portable types remain readable.
  const auto types = statement("SELECT adapter FROM trace_event_types "
                               "WHERE server_instance_id = ? AND session_id = ? AND probe_id = ?");
  std::set<std::tuple<std::string, std::string, std::uint64_t>> seen_probes;
  std::set<std::string> seen_adapters;
  for (const auto &data : buffer.events()) {
    const auto &event = data->message;
    if (event.context().event().states().empty() ||
        !seen_probes.emplace(event.server_instance_id(), event.session_id(), event.probe_id()).second)
      continue;
    sqlite3_reset(types.get());
    const auto bind = [&](int index, const std::string &value) {
      if (sqlite3_bind_text64(types.get(), index, value.data(), value.size(),
                              SQLITE_TRANSIENT, SQLITE_UTF8) != SQLITE_OK)
        throw std::runtime_error(std::string("cannot bind recorded types: ") + sqlite3_errmsg(db));
    };
    bind(1, event.server_instance_id());
    bind(2, event.session_id());
    bind(3, std::to_string(event.probe_id()));
    if (!step(types.get()) || sqlite3_column_type(types.get(), 0) != SQLITE_BLOB)
      continue;
    const auto size = sqlite3_column_bytes(types.get(), 0);
    if (size <= 0 || size > static_cast<int>(tracer::prepared::max_replay_type_bytes))
      continue;
    const auto *bytes = static_cast<const char *>(sqlite3_column_blob(types.get(), 0));
    if (!bytes || !seen_adapters.emplace(bytes, size).second)
      continue;
    api::PreparedInjector adapter;
    if (adapter.ParseFromArray(bytes, size))
      buffer.replay_types.push_back(std::move(adapter));
  }
}

void read_sqlite_recording(const std::string &path, const EventQuery &query,
                           api::TracePhase phase, EventBuffer &buffer) {
  // Prefix relative paths so they are literal filenames even in SQLite builds
  // that enable URI interpretation globally.
  const auto filename = path.front() == '/' ? path : "./" + path;
  sqlite3 *handle = nullptr;
  const auto open = sqlite3_open_v2(filename.c_str(), &handle,
                                    SQLITE_OPEN_READONLY, nullptr);
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(handle, sqlite3_close);
  const auto failure = [&] {
    return std::runtime_error("cannot read SQLite recording: " + path + ": " +
                              (db ? sqlite3_errmsg(db.get()) : "allocation failed"));
  };
  if (open != SQLITE_OK)
    throw failure();
  if (sqlite3_busy_timeout(db.get(), 5000) != SQLITE_OK)
    throw failure();

  std::string sql = "SELECT payload FROM trace_events WHERE 1";
  if (query.pid)
    sql += " AND pid = ?1";
  if (!query.session_id.empty())
    sql += " AND session_id = ?2";
  if (!query.phase.empty())
    sql += " AND phase = ?3";
  if (!query.names.empty()) {
    std::string names = "(?4";
    for (std::size_t i = 1; i < query.names.size(); ++i)
      names += ",?" + std::to_string(i + 4);
    names += ')';
    sql += " AND (function_name IN " + names + " OR event_name IN " + names + ")";
  }
  sql += " ORDER BY rowid";

  sqlite3_stmt *statement = nullptr;
  const auto prepare = sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &statement, nullptr);
  const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> rows(statement,
                                                                     sqlite3_finalize);
  if (prepare != SQLITE_OK)
    throw failure();
  const auto check = [&](int result) {
    if (result != SQLITE_OK)
      throw failure();
  };
  const auto bind_text = [&](int index, const std::string &value) {
    check(sqlite3_bind_text64(rows.get(), index, value.data(), value.size(),
                             SQLITE_STATIC, SQLITE_UTF8));
  };
  if (query.pid)
    check(sqlite3_bind_int64(rows.get(), 1, query.pid));
  if (!query.session_id.empty())
    bind_text(2, query.session_id);
  if (!query.phase.empty())
    check(sqlite3_bind_int(rows.get(), 3, phase));
  for (std::size_t i = 0; i < query.names.size(); ++i)
    bind_text(static_cast<int>(i) + 4, query.names[i]);

  while (!query.count || buffer.received < query.count) {
    const auto status = sqlite3_step(rows.get());
    if (status == SQLITE_DONE)
      break;
    if (status != SQLITE_ROW)
      throw failure();
    const auto size = sqlite3_column_type(rows.get(), 0) == SQLITE_BLOB
                          ? sqlite3_column_bytes(rows.get(), 0)
                          : 0;
    api::TraceEvent event;
    if (size <= 0 || size > static_cast<int>(max_frame) ||
        !event.ParseFromArray(sqlite3_column_blob(rows.get(), 0), size))
      throw std::runtime_error("invalid event payload in SQLite recording: " + path);
    buffer.push(event);
  }
  read_recorded_types(db.get(), buffer);
}
} // namespace

EventBuffer open_recording(const std::string &path, std::size_t max_events,
                           std::size_t max_bytes, const EventQuery &query) {
  api::TracePhase phase = api::TRACE_PHASE_UNSPECIFIED;
  if (!query.phase.empty() && !api::TracePhase_Parse(query.phase, &phase))
    throw std::invalid_argument("unknown event phase: " + query.phase);
  const auto invalid_name = [](const std::string &name) {
    return name.empty() || name.find('\0') != std::string::npos;
  };
  if (query.names.size() > 64 ||
      std::any_of(query.names.begin(), query.names.end(), invalid_name))
    throw std::invalid_argument("replay accepts up to 64 nonempty event names");
  if (path.empty() || path.find('\0') != std::string::npos)
    throw std::invalid_argument("replay requires a recording path");
  if (query.session_id.find('\0') != std::string::npos)
    throw std::invalid_argument("replay session ID contains a null byte");
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("cannot open recording: " + path);
  }
  EventBuffer buffer(max_events, max_bytes);
  unsigned char prefix[4];
  bool first_frame = true;
  while ((!query.count || buffer.received < query.count) &&
         (input.read(reinterpret_cast<char *>(prefix), sizeof(prefix)) ||
          input.gcount())) {
    if (input.gcount() != 4) {
      throw std::runtime_error("truncated recording header");
    }
    // The SQLite signature cannot be a valid Protobuf frame length.
    if (first_frame && std::memcmp(prefix, "SQLi", sizeof(prefix)) == 0) {
      char rest[12];
      if (!input.read(rest, sizeof(rest)) ||
          std::memcmp(rest, "te format 3", sizeof(rest)) != 0)
        throw std::runtime_error("invalid SQLite recording header: " + path);
      read_sqlite_recording(path, query, phase, buffer);
      break;
    }
    first_frame = false;
    std::uint32_t size = 0;
    for (unsigned i = 0; i < 4; ++i) {
      size |= static_cast<std::uint32_t>(prefix[i]) << (8 * i);
    }
    if (!size || size > max_frame) {
      throw std::runtime_error("invalid recording frame size");
    }
    std::string bytes(size, '\0');
    if (!input.read(bytes.data(), size)) {
      throw std::runtime_error("truncated recording body");
    }
    api::TraceEvent event;
    if (!event.ParseFromString(bytes)) {
      throw std::runtime_error("invalid event in recording");
    }
    if ((query.pid && event.pid() != query.pid) ||
        (!query.session_id.empty() && event.session_id() != query.session_id) ||
        (!query.phase.empty() && event.phase() != phase) ||
        (!query.names.empty() &&
         std::none_of(query.names.begin(), query.names.end(), [&](const auto &name) {
           return name == event.function_name() || name == event.context().event().name();
         })))
      continue;
    buffer.push(event);
  }
  if (input.bad()) {
    throw std::runtime_error("recording read failed");
  }
  buffer.complete = true;
  return buffer;
}

void export_json(const EventBuffer &buffer, const std::string &path) {
  const int fd = create_recording(path);
  try {
    write_all(fd, "[\n", 2);
    bool first = true;
    for (const auto &event : buffer.events()) {
      std::string json;
      const auto status =
          google::protobuf::util::MessageToJsonString(event->message, &json);
      if (!status.ok()) {
        throw std::runtime_error("cannot encode event as JSON");
      }
      if (!first) {
        write_all(fd, ",\n", 2);
      }
      first = false;
      write_all(fd, json.data(), json.size());
    }
    write_all(fd, "\n]\n", 3);
  } catch (...) {
    ::close(fd);
    throw;
  }
  if (::close(fd) != 0) {
    throw std::runtime_error("JSON export close failed");
  }
}
} // namespace tracer_repl

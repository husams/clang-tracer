#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include "events.hpp"
#include "../src/bounded_proto.hpp"
#include "../src/object_snapshot.hpp"
#include "fixtures/repl_complex_types.hpp"
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <unistd.h>

namespace tracer_repl {
class ConstructedRecord {
  std::string label_;

public:
  explicit ConstructedRecord(std::string label) : label_(std::move(label)) {}
  const std::string &label() const { return label_; }
};
template <> struct EventCodec<ConstructedRecord> {
  static ConstructedRecord decode(const EventValue &value) {
    return ConstructedRecord(value.field("label").as<std::string>());
  }
};
template <> struct EventRecord<repl_fixture::Location> {
  static constexpr const char *type_name = "repl_fixture::Location";
  static auto fields() {
    return std::make_tuple(event_member("code", &repl_fixture::Location::code),
                           event_member("name", &repl_fixture::Location::name));
  }
};
template <> struct EventRecord<repl_fixture::Record> {
  static constexpr const char *type_name = "repl_fixture::Record";
  static auto fields() {
    return std::make_tuple(
        event_member("id", &repl_fixture::Record::id),
        event_member("label", &repl_fixture::Record::label),
        event_member("location", &repl_fixture::Record::location),
        event_member("samples", &repl_fixture::Record::samples));
  }
};
} // namespace tracer_repl

using namespace tracer_repl;
namespace api = clangtracer::v1;

namespace {
void require(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}

template <class Error, class Call>
void rejects(Call call, const char *message) {
  try {
    call();
  } catch (const Error &) {
    return;
  }
  throw std::runtime_error(message);
}

Event snapshot(const api::TraceEvent &message) {
  EventBuffer buffer(1, 16384);
  buffer.push(message);
  return buffer.snapshot().at(0);
}

void native_values() {
  api::TraceEvent message;
  auto *argument = message.add_arguments();
  auto *result = message.mutable_result();
  argument->set_type(api::BOOL);
  result->set_type(api::BOOL);
  for (unsigned truth : {0u, 1u}) {
    argument->set_unsigned_value(truth);
    result->set_signed_value(truth);
    auto event = snapshot(message);
    static_assert(std::is_same_v<decltype(event.result_as<bool>()), bool>);
    static_assert(std::is_same_v<decltype(event.argument_as<int>(0)), int>);
    require(event.has_result() && event.result_as<bool>() == bool(truth),
            "boolean result is a bool");
    require(event.argument_as<bool>(0) == bool(truth),
            "boolean argument is a bool");
    require(std::get<bool>(event.result_value()) == bool(truth),
            "variant retains boolean type");
    require(event.result() == (truth ? "true" : "false"),
            "text accessors stay compatible");
    rejects<std::invalid_argument>([&] { event.result_as<int>(); },
                                   "boolean cannot silently become a number");
  }
  argument->set_type(api::INT32);
  argument->set_signed_value(-21);
  result->set_type(api::INT64);
  result->set_signed_value(42);
  auto event = snapshot(message);
  require(event.argument_as<int>(0) + event.result_as<int>() == 21,
          "integers participate directly in arithmetic");
  require(event.result_as<unsigned>() == 42,
          "checked positive integer conversion");
  rejects<std::out_of_range>([&] { event.argument_as<unsigned>(0); },
                             "negative cannot become unsigned");
  rejects<std::invalid_argument>([&] { event.result_as<bool>(); },
                                 "integer cannot silently become a bool");
  rejects<std::invalid_argument>([&] { event.result_as<double>(); },
                                 "integer is not a floating value");
  rejects<std::out_of_range>([&] { event.argument_as<int>(1); },
                             "argument bounds are checked");
  for (auto edge : {std::numeric_limits<std::int64_t>::min(),
                    std::numeric_limits<std::int64_t>::max()}) {
    result->set_signed_value(edge);
    event = snapshot(message);
    require(event.result_as<std::int64_t>() == edge,
            "signed 64-bit endpoints stay exact");
    rejects<std::out_of_range>([&] { event.result_as<std::int32_t>(); },
                               "narrowing overflow is rejected");
  }
  result->set_type(api::UINT64);
  result->set_unsigned_value(std::numeric_limits<std::uint64_t>::max());
  event = snapshot(message);
  require(event.result_as<std::uint64_t>() ==
              std::numeric_limits<std::uint64_t>::max(),
          "unsigned 64-bit maximum stays exact");
  rejects<std::out_of_range>([&] { event.result_as<std::int64_t>(); },
                             "unsigned overflow cannot become signed");
  rejects<std::out_of_range>([&] { event.result_as<std::uint32_t>(); },
                             "unsigned narrowing is checked");
  result->set_type(api::FLOAT64);
  result->set_real_value(1.25);
  argument->set_type(api::FLOAT32);
  argument->set_real_value(2.5);
  event = snapshot(message);
  require(event.result_as<double>() + event.argument_as<float>(0) == 3.75,
          "floating arithmetic uses native numbers");
  rejects<std::invalid_argument>([&] { event.result_as<int>(); },
                                 "floating values cannot silently truncate");
  result->set_real_value(std::numeric_limits<double>::max());
  event = snapshot(message);
  rejects<std::out_of_range>([&] { event.result_as<float>(); },
                             "floating overflow is rejected");
  result->set_real_value(std::numeric_limits<double>::infinity());
  require(std::isinf(snapshot(message).result_as<float>()),
          "floating infinity is preserved");
  result->set_real_value(std::numeric_limits<double>::quiet_NaN());
  require(std::isnan(snapshot(message).result_as<double>()),
          "floating NaN is preserved");
  result->set_type(api::POINTER);
  result->set_unsigned_value(42);
  event = snapshot(message);
  rejects<std::invalid_argument>([&] { event.result_as<std::uint64_t>(); },
                                 "addresses are not scalar integers");
  result->set_type(api::VOID);
  result->clear_data();
  event = snapshot(message);
  rejects<std::invalid_argument>([&] { event.result_as<int>(); },
                                 "void has no numeric value");
  result->set_type(api::INT32);
  result->set_real_value(42);
  event = snapshot(message);
  rejects<std::invalid_argument>([&] { event.result_as<int>(); },
                                 "payload/type mismatch is rejected");
  result->clear_data();
  event = snapshot(message);
  rejects<std::invalid_argument>([&] { event.result_as<int>(); },
                                 "missing numeric payload is rejected");
  result->set_type(api::BOOL);
  result->set_unsigned_value(2);
  event = snapshot(message);
  rejects<std::invalid_argument>([&] { event.result_as<bool>(); },
                                 "invalid boolean payload is rejected");
  message.clear_result();
  event = snapshot(message);
  require(!event.has_result(), "missing result is discoverable");
  rejects<std::runtime_error>([&] { event.result_as<int>(); },
                              "missing result does not become zero");
}

void callback_error_events() {
  api::TraceEvent message;
  message.set_phase(api::PRE);
  message.set_function_name("Widget::update()");
  message.set_callback_error(true);
  message.set_callback_error_message("interceptor threw");
  const auto event = snapshot(message);
  require(event.callback_error(), "typed callback error accessor is true");
  require(event.callback_error_message() == "interceptor threw",
          "typed callback error message is retained");
  require(callback_error_matches(message, CallbackErrorFilter::include) &&
              callback_error_matches(message, CallbackErrorFilter::only) &&
              !callback_error_matches(message, CallbackErrorFilter::exclude),
          "callback error filters select error events");
  const auto summary = event_summary(message);
  require(summary.find("callback_error_message") != std::string::npos &&
              summary.find("interceptor threw") != std::string::npos,
          "callback failure is visible in event summaries");
  message.set_callback_error(false);
  require(callback_error_matches(message, CallbackErrorFilter::include) &&
              callback_error_matches(message, CallbackErrorFilter::exclude) &&
              !callback_error_matches(message, CallbackErrorFilter::only),
          "success events follow callback error filter mode");

  char path[] = "/tmp/clang-tracer-callback-errors-XXXXXX";
  const int fd = ::mkstemp(path);
  if (fd < 0)
    throw std::runtime_error("cannot create callback error recording fixture");
  api::TraceEvent success;
  success.set_function_name("Widget::update()");
  api::TraceEvent failure = message;
  failure.set_callback_error(true);
  failure.set_callback_error_message("interceptor threw");
  write_record(fd, success);
  write_record(fd, failure);
  write_record(fd, success);
  ::close(fd);
  EventQuery query;
  query.callback_errors = CallbackErrorFilter::only;
  query.count = 1;
  const auto selected = open_recording(path, 8, 1024 * 1024, query).snapshot();
  ::unlink(path);
  require(selected.size() == 1 && selected.at(0).callback_error(),
          "recording query counts matching callback failures");
}

void context_values() {
  api::TraceEvent message;
  auto *context = message.mutable_context()->mutable_event();
  auto *argument = context->add_arguments();
  argument->set_index(3);
  argument->mutable_value()->set_value_type(api::BOOL);
  argument->mutable_value()->set_unsigned_value(1);
  context->mutable_result()->set_value_type(api::FLOAT64);
  context->mutable_result()->set_real_value(3.5);
  message.mutable_result()->set_type(api::INT32);
  message.mutable_result()->set_signed_value(99);
  auto event = snapshot(message);
  require(
      event.has_result() && event.argument_as<bool>(3) &&
          event.result_as<double>() == 3.5,
      "shared context uses recorded argument indices and its own typed result");
  rejects<std::out_of_range>([&] { event.argument_as<bool>(0); },
                             "sparse context indices are not vector positions");
  argument->mutable_value()->set_value_type(api::INT64);
  argument->mutable_value()->set_signed_value(-17);
  context->mutable_result()->set_value_type(api::UINT64);
  context->mutable_result()->set_unsigned_value(42);
  event = snapshot(message);
  require(event.argument_as<int>(3) == -17 && event.result_as<unsigned>() == 42,
          "context integer values stay numeric");
  context->mutable_result()->set_value_type(api::BOOL);
  context->mutable_result()->set_signed_value(0);
  require(!snapshot(message).result_as<bool>(), "context false is a bool");
  context->mutable_result()->set_value_type(api::FLOAT32);
  context->mutable_result()->set_real_value(1.5);
  require(snapshot(message).result_as<float>() == 1.5f,
          "context float32 is supported");
  context->clear_result();
  message.clear_result();
  event = snapshot(message);
  require(!event.has_result(), "context missing result is discoverable");
  rejects<std::runtime_error>([&] { event.result_as<bool>(); },
                              "missing context result does not become false");
}

void complex_values() {
  using FD = google::protobuf::FieldDescriptorProto;
  google::protobuf::FileDescriptorSet files;
  auto *file = files.add_file();
  file->set_name("typed_snapshot.proto");
  file->set_package("repl.test");
  file->set_syntax("proto3");
  const auto field = [](auto *message, const char *name, int number,
                        FD::Type type, bool repeated = false,
                        const char *target = "") {
    auto *f = message->add_field();
    f->set_name(name);
    f->set_number(number);
    f->set_type(type);
    f->set_label(repeated ? FD::LABEL_REPEATED : FD::LABEL_OPTIONAL);
    if (*target)
      f->set_type_name(target);
  };
  auto *location = file->add_message_type();
  location->set_name("Location");
  field(location, "code", 1, FD::TYPE_SINT32);
  field(location, "name", 2, FD::TYPE_BYTES);
  auto *record = file->add_message_type();
  record->set_name("Record");
  field(record, "id", 1, FD::TYPE_SINT32);
  field(record, "label", 2, FD::TYPE_BYTES);
  field(record, "location", 3, FD::TYPE_MESSAGE, false, ".repl.test.Location");
  field(record, "samples", 4, FD::TYPE_SINT32, true);
  auto *root = file->add_message_type();
  root->set_name("Root");
  field(root, "value", 1, FD::TYPE_MESSAGE, false, ".repl.test.Record");
  auto *records = file->add_message_type();
  records->set_name("Records");
  field(records, "items", 1, FD::TYPE_MESSAGE, true, ".repl.test.Record");
  auto *records_root = file->add_message_type();
  records_root->set_name("RecordsRoot");
  field(records_root, "value", 1, FD::TYPE_MESSAGE, false, ".repl.test.Records");
  auto *text_root = file->add_message_type();
  text_root->set_name("Text");
  field(text_root, "value", 1, FD::TYPE_BYTES);
  auto *truth_root = file->add_message_type();
  truth_root->set_name("Truth");
  field(truth_root, "value", 1, FD::TYPE_BOOL);
  auto *truth_list = file->add_message_type();
  truth_list->set_name("TruthList");
  field(truth_list, "value", 1, FD::TYPE_BOOL, true);
  auto *integer_root = file->add_message_type();
  integer_root->set_name("Integer");
  field(integer_root, "value", 1, FD::TYPE_SINT32);
  auto *optional_record = file->add_message_type();
  optional_record->set_name("OptionalRecord");
  field(optional_record, "value", 1, FD::TYPE_MESSAGE, false,
        ".repl.test.Record");
  auto *optional_root = file->add_message_type();
  optional_root->set_name("OptionalRoot");
  field(optional_root, "value", 1, FD::TYPE_MESSAGE, false,
        ".repl.test.OptionalRecord");
  google::protobuf::DescriptorPool pool;
  require(pool.BuildFile(*file), "build owned snapshot test schema");
  google::protobuf::DynamicMessageFactory factory(&pool);
  std::unique_ptr<google::protobuf::Message> body(
      factory.GetPrototype(pool.FindMessageTypeByName("repl.test.Root"))
          ->New());
  auto *value = body->GetReflection()->MutableMessage(
      body.get(), body->GetDescriptor()->field(0));
  const auto *r = value->GetReflection();
  const auto *d = value->GetDescriptor();
  r->SetInt32(value, d->FindFieldByName("id"), 37);
  const std::string bytes("a\0\xffz", 4);
  r->SetString(value, d->FindFieldByName("label"), bytes);
  r->AddInt32(value, d->FindFieldByName("samples"), 2);
  r->AddInt32(value, d->FindFieldByName("samples"), 4);
  auto *nested = r->MutableMessage(value, d->FindFieldByName("location"));
  nested->GetReflection()->SetInt32(nested, nested->GetDescriptor()->field(0),
                                    12);
  nested->GetReflection()->SetString(nested, nested->GetDescriptor()->field(1),
                                     "owned-nested");
  api::TraceEvent message;
  auto *argument = message.add_arguments();
  argument->set_type(api::OBJECT);
  argument->set_unsigned_value(1);
  auto *captured = argument->mutable_snapshot();
  captured->set_cpp_type("repl_fixture::Record");
  captured->set_root_type("repl.test.Root");
  captured->set_descriptor_set(files.SerializeAsString());
  captured->set_payload(body->SerializeAsString());
  captured->set_status(clangtracer::context::v1::ValueSnapshot::COMPLETE);
  captured->add_container_roles("1.4=vector");
  captured->add_record_types("repl.test.Location=repl_fixture::Location");
  captured->add_record_types("repl.test.Record=repl_fixture::Record");
  *message.mutable_result() = *argument;
  auto event = snapshot(message);
  auto decoded = event.argument_as<repl_fixture::Record>(0);
  auto *observed = message.mutable_object_observation()->mutable_snapshot_after();
  tracer_repl::encode_observation_value(event.argument_snapshot(0), *observed);
  api::TraceEvent restored;
  require(restored.ParseFromString(message.SerializeAsString()),
          "round-trip an observation with nested and non-UTF-8 values");
  const auto &observation = restored.object_observation().snapshot_after();
  require(observation.composite_kind() == api::ObjectObservation::SnapshotValue::RECORD &&
              observation.cpp_type() == "repl_fixture::Record",
          "observation retains its captured record type");
  const auto child = [](const auto &parent, std::string_view name) -> const auto & {
    const auto found = std::find_if(parent.children().begin(), parent.children().end(),
        [&](const auto &item) { return item.field_name() == name; });
    if (found == parent.children().end())
      throw std::runtime_error("missing observed field");
    return *found;
  };
  const auto &samples = child(observation, "samples");
  require(child(observation, "label").bytes_value() == bytes &&
              child(child(observation, "location"), "name").string_value() == "owned-nested" &&
              samples.composite_kind() == api::ObjectObservation::SnapshotValue::SEQUENCE &&
              samples.children_size() == 2 && samples.children(1).signed_value() == 4,
          "observation retains nested records, sequences, and binary strings");
  message.clear_object_observation();
  const auto keyed_snapshot = [&](int id) {
    std::unique_ptr<google::protobuf::Message> wrapper(
        factory.GetPrototype(pool.FindMessageTypeByName("repl.test.RecordsRoot"))->New());
    auto *items = wrapper->GetReflection()->MutableMessage(wrapper.get(), wrapper->GetDescriptor()->field(0));
    auto *item = items->GetReflection()->AddMessage(items, items->GetDescriptor()->field(0));
    item->CopyFrom(*value);
    r->SetInt32(item, d->FindFieldByName("id"), id);
    tracer_repl::Snapshot sample = *captured;
    sample.set_cpp_type("repl_fixture::Owner");
    sample.set_root_type("repl.test.RecordsRoot");
    sample.set_payload(wrapper->SerializeAsString());
    sample.clear_container_roles();
    sample.add_container_roles("1.1=vector");
    return sample;
  };
  const auto old_items = keyed_snapshot(37);
  auto new_items = keyed_snapshot(46);
  const auto changes = tracer_repl::compare_snapshots(old_items, new_items, {"items=id"});
  require(changes.size() == 2 && changes[0].detail == "removed" &&
              changes[0].before && changes[0].before->field("label").as<std::string>() == bytes &&
              changes[1].detail == "added" && changes[1].after &&
              changes[1].after->field("location").field("code").as<int>() == 12,
          "keyed changes retain the complete captured element");
  const auto unmatched = tracer_repl::compare_snapshots(old_items, new_items, {"items=unknown"});
  require(unmatched.size() == 1 && unmatched.front().gap &&
              unmatched.front().before && unmatched.front().after &&
              unmatched.front().before->at(0).field("id").as<int>() == 37 &&
              unmatched.front().after->at(0).field("id").as<int>() == 46,
          "an unknown identity retains samples without claiming adds or removals");
  new_items.set_status(tracer_repl::Snapshot::LIMIT_EXCEEDED);
  const auto unavailable = tracer_repl::compare_snapshots(old_items, new_items, {"items=id"});
  require(unavailable.size() == 1 && unavailable.front().gap && !unavailable.front().before &&
              !unavailable.front().after,
          "a limited capture cannot report element removal");
  require(decoded.id == 37 && decoded.label == bytes &&
              decoded.location.code == 12 &&
              decoded.location.name == "owned-nested" &&
              decoded.samples == std::vector<int>({2, 4}),
          "nested class/struct values decode into owning C++ objects");
  require(event.result_as<repl_fixture::Record>().samples.at(1) == 4,
          "record result supports the same typed API");
  require(event.argument_as<ConstructedRecord>(0).label() == bytes,
          "factory codecs reconstruct classes without a default constructor");
  require(event.argument_snapshot(0)
                  .field("location")
                  .field("name")
                  .as<std::string>() == "owned-nested",
          "dynamic named fields need no mapping");
  rejects<std::out_of_range>(
      [&] { event.argument_snapshot(0).field("missing"); },
      "missing fields are explicit");

  std::unique_ptr<google::protobuf::Message> optional_body(
      factory.GetPrototype(pool.FindMessageTypeByName("repl.test.OptionalRoot"))
          ->New());
  auto *wrapper = optional_body->GetReflection()->MutableMessage(
      optional_body.get(), optional_body->GetDescriptor()->field(0));
  wrapper->GetReflection()
      ->MutableMessage(wrapper, wrapper->GetDescriptor()->field(0))
      ->CopyFrom(*value);
  api::TraceEvent optional_message;
  auto *optional_snapshot = optional_message.add_arguments()->mutable_snapshot();
  *optional_snapshot = *captured;
  optional_snapshot->set_cpp_type("std::optional<repl_fixture::Record>");
  optional_snapshot->set_root_type("repl.test.OptionalRoot");
  optional_snapshot->set_payload(optional_body->SerializeAsString());
  optional_snapshot->clear_container_roles();
  optional_snapshot->add_container_roles("1=optional");
  optional_snapshot->add_container_roles("1/value.4=vector");
  for (const auto phase : {api::PRE, api::POST}) {
    optional_message.set_phase(phase);
    const auto optional_event = snapshot(optional_message);
    const auto record = optional_event.argument_as<std::optional<repl_fixture::Record>>(0);
    require(record && record->id == 37 && record->label == bytes &&
                record->location.code == 12 &&
                record->location.name == "owned-nested" &&
                record->samples == std::vector<int>({2, 4}),
            "engaged optional PRE/POST arguments retain nested fields");
    const auto raw = optional_event.argument_snapshot(0);
    require(raw.cpp_type() == "std::optional<repl_fixture::Record>" &&
                !raw.matches_record_type("repl_fixture::Record") &&
                raw.field("location").field("name").as<std::string>() == "owned-nested",
            "optional conversion leaves the snapshot API intact");
    rejects<std::invalid_argument>(
        [&] { optional_event.argument_as<std::optional<repl_fixture::Location>>(0); },
        "an optional cannot decode as a different record type");
    rejects<std::invalid_argument>(
        [&] { optional_event.argument_as<repl_fixture::Record>(0); },
        "a raw optional retains its wrapper identity");
  }
  optional_snapshot->set_cpp_type("std::optional<WrongType>");
  rejects<std::invalid_argument>(
      [&] { snapshot(optional_message).argument_as<std::optional<repl_fixture::Record>>(0); },
      "incorrect optional payload identity is rejected");
  optional_snapshot->set_cpp_type("::std::optional< repl_fixture::Record >");
  require(snapshot(optional_message)
                  .argument_as<std::optional<repl_fixture::Record>>(0)->id == 37,
          "qualified optional spellings retain contained record identity");
  optional_snapshot->set_cpp_type("std::optional<repl_fixture::Record>");
  optional_body->Clear();
  optional_snapshot->set_payload(optional_body->SerializeAsString());
  for (const auto phase : {api::PRE, api::POST}) {
    optional_message.set_phase(phase);
    require(!snapshot(optional_message).argument_as<std::optional<repl_fixture::Record>>(0),
            "disengaged optional PRE/POST arguments remain empty");
  }
  optional_snapshot->set_payload(std::string("\x0a\x01\x0a", 3));
  rejects<std::runtime_error>(
      [&] { snapshot(optional_message).argument_as<std::optional<repl_fixture::Record>>(0); },
      "malformed optional payloads cannot fabricate empty values");
  captured->set_cpp_type("WrongType");
  rejects<std::invalid_argument>(
      [&] { snapshot(message).argument_as<repl_fixture::Record>(0); },
      "typed record identity is checked");
  captured->set_cpp_type("repl_fixture::Record");
  const auto saved_payload = captured->payload();
  captured->clear_payload();
  rejects<std::runtime_error>(
      [&] { snapshot(message).argument_as<repl_fixture::Record>(0); },
      "a missing record payload cannot fabricate a default object");
  captured->set_payload(saved_payload);
  char path[] = "/tmp/tracer-complex-recording.XXXXXX";
  const int temporary = ::mkstemp(path);
  require(temporary >= 0, "create recording test path");
  ::close(temporary);
  ::unlink(path);
  const int fd = create_recording(path);
  write_record(fd, message);
  ::close(fd);
  const auto replay = open_recording(path, 10, 1024 * 1024).snapshot();
  ::unlink(path);
  require(replay.at(0).argument_as<repl_fixture::Record>(0).label == bytes,
          "owned complex values survive recording replay");
  std::unique_ptr<google::protobuf::Message> text(
      factory.GetPrototype(pool.FindMessageTypeByName("repl.test.Text"))
          ->New());
  for (const auto &input :
       {std::string(), std::string("SSO"), std::string(128, 'h'), bytes}) {
    text->GetReflection()->SetString(text.get(),
                                     text->GetDescriptor()->field(0), input);
    captured->set_cpp_type("std::string");
    captured->set_root_type("repl.test.Text");
    captured->set_payload(input.empty() ? std::string("\x0a\x00", 2)
                                        : text->SerializeAsString());
    captured->clear_container_roles();
    *message.mutable_result() = *argument;
    require(snapshot(message).argument_snapshot(0).cpp_type() == "std::string",
            "root string snapshots retain their C++ type metadata");
    require(snapshot(message).argument_as<std::string>(0) == input &&
                snapshot(message).result_as<std::string>() == input,
            "strings preserve empty, SSO, heap, NUL and non-UTF8 bytes");
  }
  auto *context_argument =
      message.mutable_context()->mutable_event()->add_arguments();
  context_argument->set_index(3);
  context_argument->mutable_value()->set_value_type(api::POINTER);
  context_argument->mutable_value()->set_unsigned_value(1);
  *context_argument->mutable_value()->mutable_snapshot() = *captured;
  *message.mutable_context()->mutable_event()->mutable_result() =
      context_argument->value();
  require(snapshot(message).argument_as<std::string>(3) == bytes &&
              snapshot(message).result_as<std::string>() == bytes,
          "shared-context snapshots use the same typed API");
  captured->set_status(clangtracer::context::v1::ValueSnapshot::LIMIT_EXCEEDED);
  message.clear_context();
  rejects<std::runtime_error>(
      [&] { snapshot(message).argument_as<std::string>(0); },
      "partial capture is never returned as a complete value");
  captured->set_status(clangtracer::context::v1::ValueSnapshot::COMPLETE);
  captured->set_payload("corrupt");
  rejects<std::runtime_error>(
      [&] { snapshot(message).argument_as<std::string>(0); },
      "malformed capture is rejected");
  captured->set_root_type("repl.test.Truth");
  captured->set_cpp_type("bool");
  captured->set_payload(std::string("\x08\x02", 2));
  rejects<std::runtime_error>([&] { snapshot(message).argument_as<bool>(0); },
                              "invalid boolean snapshot payload is rejected");
  captured->set_payload(std::string("\x08\x00", 2));
  require(!snapshot(message).argument_as<bool>(0),
          "explicit false snapshot is retained");
  captured->set_payload(std::string("\x08", 1) + std::string(9, '\x80') +
                        std::string("\x02", 1));
  rejects<std::runtime_error>([&] { snapshot(message).argument_as<bool>(0); },
                              "overflowing 64-bit varint cannot become false");
  captured->set_root_type("repl.test.TruthList");
  captured->set_cpp_type("std::vector<bool>");
  captured->add_container_roles("1=vector");
  captured->set_payload(std::string("\x0a\x0a", 2) + std::string(9, '\x80') +
                        std::string("\x02", 1));
  rejects<std::runtime_error>(
      [&] { snapshot(message).argument_as<std::vector<bool>>(0); },
      "overflowing packed varint is rejected");
  captured->clear_container_roles();
  captured->set_root_type("repl.test.Integer");
  captured->set_cpp_type("int");
  captured->set_payload(std::string("\x08\x80\x80\x80\x80\x80\x20", 7));
  rejects<std::runtime_error>(
      [&] { snapshot(message).argument_as<int>(0); },
      "schema integer overflow cannot silently truncate");
}
} // namespace

// Values captured in a form of their own (issue #181): 128-bit integers as two halves, a
// long double as the bytes of its format, std::nullptr_t as a marker, a member pointer as
// the parts its ABI gives it, and a wide enumeration as an integer that names its type.
void abi_values() {
  const auto record = [](EventValue::Object fields, std::string type = {}) {
    return EventValue{std::move(fields), std::move(type)};
  };
  const auto halves = [&](std::uint64_t low, EventValue high) {
    return record({{"low", EventValue{low}}, {"high", std::move(high)}});
  };
  const __int128 big = (static_cast<__int128>(1) << 100) + 5;
  const auto signed_value = [&](__int128 value) {
    return halves(static_cast<std::uint64_t>(value),
                  EventValue{static_cast<std::int64_t>(value >> 64)});
  };
  using Unsigned = unsigned __int128;
  const Unsigned top = ~static_cast<Unsigned>(0) - 1;
  const auto unsigned_top = halves(~0ULL - 1, EventValue{~std::uint64_t(0)});
  require(signed_value(big).as<__int128>() == big && signed_value(-big).as<__int128>() == -big &&
              signed_value(5).as<__int128>() == 5 && signed_value(-1).as<__int128>() == -1,
          "a signed 128-bit integer keeps every bit");
  require(unsigned_top.as<Unsigned>() == top && signed_value(big).as<Unsigned>() == big,
          "an unsigned 128-bit integer keeps its top bit");
  rejects<std::out_of_range>([&] { (void)unsigned_top.as<__int128>(); },
                             "an unsigned value above the signed range is refused");
  rejects<std::out_of_range>([&] { (void)signed_value(-big).as<Unsigned>(); },
                             "a negative value is refused as unsigned");
  require(EventValue{std::int64_t(-3)}.as<__int128>() == -3 &&
              EventValue{std::uint64_t(7)}.as<Unsigned>() == 7,
          "a 64-bit event integer widens");
  rejects<std::out_of_range>([] { (void)EventValue{std::int64_t(-3)}.as<Unsigned>(); },
                             "a negative 64-bit value is refused as unsigned");
  rejects<std::invalid_argument>([&] { (void)record({}).as<__int128>(); },
                                 "a record without halves is not an integer");

  const auto extended = [&](const char *format, std::string bytes) {
    return record({{"format", EventValue{std::string(format)}},
                   {"bytes", EventValue{std::move(bytes)}}},
                  "long double");
  };
  const auto x87 = [&](std::string bytes) { return extended("x87-extended-80", std::move(bytes)); };
  const std::string zeros7(7, '\0');
  // 1.5, 1 + 2^-60, -0, +infinity and a quiet NaN in the x87 extended format.
  require(x87(zeros7 + std::string("\xc0\xff\x3f", 3)).as<long double>() == 1.5L &&
              x87(zeros7 + std::string("\xc0\xff\x3f", 3)).as<double>() == 1.5,
          "an x87 long double decodes");
  const auto precise = x87(std::string("\x08", 1) + std::string(6, '\0') +
                           std::string("\x80\xff\x3f", 3));
  if (std::numeric_limits<long double>::digits >= 64)
    require(precise.as<long double>() == 1.0L + 0x1p-60L && precise.as<long double>() != 1.0L,
            "an x87 long double keeps the bits a double has no room for");
  require(precise.as<double>() == 1.0, "a double is the nearest value on request");
  const auto negative_zero = x87(zeros7 + std::string("\x00\x00\x80", 3)).as<long double>();
  require(negative_zero == 0 && std::signbit(negative_zero), "a negative zero keeps its sign");
  require(std::isinf(x87(zeros7 + std::string("\x80\xff\x7f", 3)).as<long double>()) &&
              std::isnan(x87(zeros7 + std::string("\xc0\xff\x7f", 3)).as<long double>()),
          "infinity and NaN decode");
  // The same values in IEEE binary128 and binary64.
  const auto quad = [&](std::uint64_t low, std::uint64_t high) {
    std::string bytes(16, '\0');
    for (int i = 0; i < 8; ++i) {
      bytes[i] = static_cast<char>(low >> (8 * i));
      bytes[8 + i] = static_cast<char>(high >> (8 * i));
    }
    return extended("ieee-binary128", std::move(bytes));
  };
  require(quad(0, 0x3fff800000000000ULL).as<long double>() == 1.5L &&
              quad(0, 0xbfff800000000000ULL).as<long double>() == -1.5L,
          "a binary128 long double decodes");
  if (std::numeric_limits<long double>::digits >= 64)
    require(quad(1ULL << 52, 0x3fff000000000000ULL).as<long double>() == 1.0L + 0x1p-60L,
            "a binary128 long double keeps its low bits");
  require(std::isinf(quad(0, 0x7fff000000000000ULL).as<long double>()) &&
              std::isnan(quad(1, 0x7fff000000000000ULL).as<long double>()),
          "binary128 infinity and NaN decode");
  require(extended("ieee-binary64", std::string("\0\0\0\0\0\0\xf8\x3f", 8)).as<long double>() ==
              1.5L,
          "a binary64 long double decodes");
  rejects<std::invalid_argument>(
      [&] { (void)extended("ibm-double-double", std::string(16, '\0')).as<long double>(); },
      "an unknown long double format is refused");
  rejects<std::invalid_argument>([&] { (void)record({}).as<double>(); },
                                 "a record is not a floating-point value");

  const auto marker = [&](bool null) {
    return record({{"null", EventValue{null}}}, "std::nullptr_t");
  };
  require(marker(true).as<std::nullptr_t>() == nullptr, "a null marker decodes");
  rejects<std::invalid_argument>([&] { (void)marker(false).as<std::nullptr_t>(); },
                                 "a cleared null marker is refused");

  struct Pair {
    int a, b;
    int sum(int n) { return a + b + n; }
  };
  const auto data_member = [&](const char *abi, bool null, std::int64_t offset) {
    return record({{"abi", EventValue{std::string(abi)}},
                   {"null", EventValue{null}},
                   {"offset", EventValue{offset}}});
  };
  require(data_member("itanium", false, sizeof(int)).as<int Pair::*>() == &Pair::b &&
              data_member("itanium", false, 0).as<int Pair::*>() == &Pair::a &&
              data_member("itanium", true, -1).as<int Pair::*>() == nullptr,
          "a data member pointer is rebuilt from its offset");
  rejects<std::invalid_argument>(
      [&] { (void)data_member("microsoft", false, 4).as<int Pair::*>(); },
      "a data member pointer of another ABI is refused");
  const auto function_member = [&](bool null) {
    return record({{"abi", EventValue{std::string("itanium")}},
                   {"null", EventValue{null}},
                   {"is_virtual", EventValue{false}},
                   {"address", EventValue{std::uint64_t(null ? 0 : 0x401000)}},
                   {"vtable_offset", EventValue{std::uint64_t(0)}},
                   {"this_adjustment", EventValue{std::int64_t(0)}}});
  };
  require(function_member(true).as<int (Pair::*)(int)>() == nullptr,
          "a null member function pointer decodes");
  rejects<std::invalid_argument>([&] { (void)function_member(false).as<int (Pair::*)(int)>(); },
                                 "a member function pointer of the target has no local value");

  // A wide enumeration is an integer field whose options name the enumeration.
  using FD = google::protobuf::FieldDescriptorProto;
  google::protobuf::FileDescriptorSet files;
  auto *file = files.add_file();
  file->set_name("wide_enum.proto");
  file->set_package("repl.test");
  file->set_syntax("proto3");
  auto *holder = file->add_message_type();
  holder->set_name("Holder");
  const auto wide = [&](const char *name, int number, FD::Type type, const char *enum_type) {
    auto *field = holder->add_field();
    field->set_name(name);
    field->set_number(number);
    field->set_type(type);
    field->set_label(FD::LABEL_OPTIONAL);
    if (*enum_type) {
      auto *options = field->mutable_options()->mutable_unknown_fields();
      options->AddLengthDelimited(tracer::context_engine::kEnumTypeOption, enum_type);
      options->AddLengthDelimited(tracer::context_engine::kEnumeratorOption,
                                  "top=18446744073709551615");
    }
  };
  wide("big", 1, FD::TYPE_UINT64, "wide::Big");
  wide("low", 2, FD::TYPE_INT64, "wide::Low");
  wide("top", 3, FD::TYPE_UINT32, "wide::Top32");
  wide("plain", 4, FD::TYPE_UINT64, "");
  auto *root = file->add_message_type();
  root->set_name("HolderRoot");
  auto *value_field = root->add_field();
  value_field->set_name("value");
  value_field->set_number(1);
  value_field->set_type(FD::TYPE_MESSAGE);
  value_field->set_label(FD::LABEL_OPTIONAL);
  value_field->set_type_name(".repl.test.Holder");
  google::protobuf::DescriptorPool pool;
  require(pool.BuildFile(*file), "a pool keeps the options it has no definition for");
  google::protobuf::DynamicMessageFactory factory(&pool);
  std::unique_ptr<google::protobuf::Message> body(
      factory.GetPrototype(pool.FindMessageTypeByName("repl.test.HolderRoot"))->New());
  auto *held = body->GetReflection()->MutableMessage(body.get(), body->GetDescriptor()->field(0));
  const auto *r = held->GetReflection();
  const auto *d = held->GetDescriptor();
  r->SetUInt64(held, d->FindFieldByName("big"), ~0ULL);
  r->SetInt64(held, d->FindFieldByName("low"), std::numeric_limits<std::int64_t>::min());
  r->SetUInt32(held, d->FindFieldByName("top"), 0xffffffffu);
  r->SetUInt64(held, d->FindFieldByName("plain"), 9);
  api::TraceEvent message;
  auto *argument = message.add_arguments();
  argument->set_type(api::OBJECT);
  argument->set_unsigned_value(1);
  auto *captured = argument->mutable_snapshot();
  captured->set_cpp_type("wide::Holder");
  captured->set_root_type("repl.test.HolderRoot");
  captured->set_descriptor_set(files.SerializeAsString());
  captured->set_payload(body->SerializeAsString());
  captured->set_status(clangtracer::context::v1::ValueSnapshot::COMPLETE);
  captured->add_record_types("repl.test.Holder=wide::Holder");
  const auto event = snapshot(message);
  const auto decoded = event.argument_snapshot(0);
  enum class Big : unsigned long long { top = ~0ULL };
  enum class Low : long long { low = std::numeric_limits<long long>::min() };
  require(decoded.field("big").cpp_type() == "wide::Big" &&
              decoded.field("big").as<unsigned long long>() == ~0ULL &&
              decoded.field("big").as<Big>() == Big::top,
          "a wide unsigned enumeration keeps a value above INT64_MAX and names its type");
  require(decoded.field("low").cpp_type() == "wide::Low" &&
              decoded.field("low").as<Low>() == Low::low,
          "a wide signed enumeration keeps INT64_MIN");
  require(decoded.field("top").cpp_type() == "wide::Top32" &&
              decoded.field("top").as<unsigned>() == 0xffffffffu,
          "a 32-bit unsigned enumeration keeps UINT32_MAX");
  require(decoded.field("plain").cpp_type().empty() &&
              decoded.field("plain").as<unsigned long long>() == 9,
          "an integer without the options is not an enumeration");
}

int main() {
  try {
    native_values();
    callback_error_events();
    context_values();
    complex_values();
    abi_values();
    std::cout << "typed event value checks passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

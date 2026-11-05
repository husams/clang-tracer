#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include "events.hpp"
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

int main() {
  try {
    native_values();
    context_values();
    complex_values();
    std::cout << "typed event value checks passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

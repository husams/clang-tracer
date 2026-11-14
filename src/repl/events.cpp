#include "events.hpp"
#include "../prepared_contract.hpp"
#include "../object_snapshot.hpp"
#include "tracer/protocol.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>

#include <fcntl.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/stubs/strutil.h>
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

std::vector<StackFrame> Event::stack() const {
  const auto &captured = data_->message.stack().frames();
  std::vector<StackFrame> frames;
  frames.reserve(captured.size());
  for (const auto &frame : captured) {
    frames.push_back({frame.pc(), frame.stack_pointer(),
                      api::StackFrame::Kind_Name(frame.kind()), frame.module(),
                      frame.module_offset(), frame.function(), frame.file(),
                      frame.line(), frame.application_frame()});
  }
  return frames;
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

void print_receiver_delta(const api::TraceEvent &before, const api::TraceEvent &after,
                          const std::vector<std::string> &keys, std::ostream &out) {
  if (before.phase() != api::PRE || after.phase() != api::POST ||
      !after.receiver_sampled_at_return() || !before.call_id() ||
      before.call_id() != after.call_id() || before.session_id() != after.session_id() ||
      before.server_instance_id() != after.server_instance_id() ||
      before.probe_id() != after.probe_id() || before.pid() != after.pid() ||
      before.tid() != after.tid() || !before.object_address() ||
      before.object_address() != after.object_address() || before.sequence() >= after.sequence())
    throw std::invalid_argument("diff requires independently sampled entry and return for one invocation");
  validate_snapshot_keys(keys);
  out << "snapshot_window call=" << before.call_id()
      << " function=" << std::quoted(before.function_name()) << '\n';
  if (!before.has_receiver_snapshot() || !after.has_receiver_snapshot()) {
    out << "coverage_gap receiver missing sample\n";
    return;
  }
  const auto changes = compare_snapshots(before.receiver_snapshot(), after.receiver_snapshot(), keys);
  std::size_t gaps = 0;
  for (const auto &change : changes) {
    gaps += change.gap;
    out << (change.gap ? "coverage_gap " : "snapshot_delta ")
        << change.path << ' ' << change.detail << '\n';
  }
  out << "snapshot_changes=" << changes.size() - gaps << " gaps=" << gaps << '\n';
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
  if (event.has_object_observation()) {
    const auto &object = event.object_observation();
    out << "cursor=" << event.sequence() << " watch=" << event.probe_id()
        << " tid=" << event.tid() << " " << api::ObjectObservation::Kind_Name(object.kind());
    if (object.lifetime_id())
      out << " " << object.concrete_type() << "#" << object.lifetime_id()
          << " generation=" << object.generation() << " address=0x" << std::hex
          << event.object_address() << std::dec;
    if (object.borrow_id())
      out << " borrow=" << object.borrow_id();
    if (object.runtime_module_generation())
      out << " module_generation=" << object.runtime_module_generation();
    if (object.has_shared_storage()) {
      const auto &storage = object.shared_storage();
      out << " shared_storage=" << storage.device() << ':' << storage.inode()
          << ':' << storage.file_offset();
    }
    if (object.external_write_order_unknown())
      out << " external_order=unknown";
    if (object.owner_address())
      out << " owner=0x" << std::hex << object.owner_address() << std::dec;
    if (object.has_owner_index())
      out << " slot=" << object.owner_index();
    if (!object.field().empty())
      out << " " << object.field();
    if (object.has_old_integer())
      out << " " << object.old_integer() << " ->";
    else if (object.kind() == api::ObjectObservation::FIELD_CHANGED && object.has_new_integer())
      out << " old=unavailable ->";
    if (object.has_new_integer())
      out << " " << object.new_integer();
    if (object.has_old_string())
      out << " \"" << google::protobuf::CEscape(object.old_string()) << "\" ->";
    if (object.has_new_string())
      out << " \"" << google::protobuf::CEscape(object.new_string()) << '"';
    const auto union_sample = [&](std::uint32_t tag, const std::string &member,
                                  const api::ObjectObservation::SnapshotValue &value) {
      out << " tag=" << tag;
      if (member.empty())
        return;
      out << ' ' << member << '=';
      if (value.has_signed_value())
        out << value.signed_value();
      else if (value.has_unsigned_value())
        out << value.unsigned_value();
      else if (value.has_real_value())
        out << value.real_value();
    };
    if (object.has_old_union_tag()) {
      union_sample(object.old_union_tag(), object.old_union_member(), object.snapshot_before());
      out << " ->";
    }
    if (object.has_new_union_tag())
      union_sample(object.new_union_tag(), object.new_union_member(), object.snapshot_after());
    const auto raw_bytes = [&](const std::string &bytes) {
      std::ostringstream encoded;
      encoded << std::hex << std::setfill('0');
      for (unsigned char byte : bytes)
        encoded << std::setw(2) << unsigned(byte);
      return encoded.str();
    };
    if (object.has_old_bytes())
      out << " bytes=" << raw_bytes(object.old_bytes()) << " ->";
    if (object.has_new_bytes())
      out << " bytes=" << raw_bytes(object.new_bytes());
    if (object.destination())
      out << " destination=0x" << std::hex << object.destination() << std::dec;
    if (object.pc())
      out << " pc=0x" << std::hex << object.pc() << std::dec;
    if (!object.function().empty())
      out << " function=" << std::quoted(object.function())
          << " module=" << std::quoted(object.module());
    if (!object.file().empty())
      out << " " << object.file() << ":" << object.line();
    if (!object.diagnostic().empty())
      out << " | " << object.diagnostic();
    return out.str();
  }
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
  ++appended;
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

namespace {
using ProbeKey = std::tuple<std::string_view, std::string_view, std::uint64_t>;
using TargetKey = std::pair<ProbeKey, std::uint32_t>;
using CallKey = std::tuple<TargetKey, std::uint32_t, std::uint64_t>;

struct CallPair {
  const api::TraceEvent *pre = nullptr, *post = nullptr;
  bool ambiguous = false;

  void add(const api::TraceEvent &event) {
    auto &half = event.phase() == api::PRE ? pre : post;
    if (!half)
      half = &event;
    else if (!event.sequence() || event.sequence() != half->sequence())
      ambiguous = true;
  }

  std::optional<std::uint64_t> duration() const {
    if (ambiguous || !pre || !post || !pre->monotonic_time_ns() ||
        post->monotonic_time_ns() < pre->monotonic_time_ns())
      return std::nullopt;
    return post->monotonic_time_ns() - pre->monotonic_time_ns();
  }
};

struct CallCounts {
  std::size_t paired = 0, missing_pre = 0, missing_post = 0, timed = 0, exceptional = 0;
  std::size_t ambiguous = 0;

  void add(const CallPair &pair, bool has_duration) {
    paired += pair.pre && pair.post;
    exceptional += pair.post && pair.post->phase() == api::EXCEPTION;
    missing_pre += !pair.pre;
    missing_post += !pair.post;
    timed += has_duration;
    ambiguous += pair.ambiguous;
  }

  std::size_t incomplete() const { return missing_pre + missing_post; }
};

// Upper-exclusive duration buckets, in nanoseconds.
constexpr std::array<std::uint64_t, 8> duration_bounds{
    100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};

using CallerKey = std::tuple<std::string_view, std::string_view, std::uint64_t,
                             std::uint64_t>;

struct FunctionStats {
  std::string_view name;
  CallCounts counts;
  std::array<std::size_t, duration_bounds.size() + 1> histogram{};
  std::map<CallerKey, std::size_t> callers;
  std::size_t caller_unknown = 0;
};

std::string_view target_name(const api::TraceEvent *event) {
  if (!event)
    return {};
  if (!event->function_name().empty())
    return event->function_name();
  if (event->stack().frames_size() &&
      event->stack().frames(0).kind() == api::StackFrame::TARGET_ENTRY)
    return event->stack().frames(0).function();
  return {};
}

std::optional<CallerKey> direct_caller(const api::TraceEvent *event) {
  if (!event)
    return std::nullopt;
  const auto &stack = event->stack();
  if (stack.frames_size() < 2 ||
      (stack.status() != api::StackTrace::COMPLETE &&
       stack.status() != api::StackTrace::TRUNCATED &&
       stack.status() != api::StackTrace::PARTIAL))
    return std::nullopt;
  const auto &entry = stack.frames(0);
  const auto &caller = stack.frames(1);
  if (entry.kind() != api::StackFrame::TARGET_ENTRY || !entry.pc() ||
      caller.kind() != api::StackFrame::NATIVE || !caller.pc())
    return std::nullopt;
  // A filtered stack can hide the real caller. The entry frame and its
  // immediate caller are one x86-64 return-address slot apart on the stack.
  constexpr std::uint64_t return_address_size = 8;
  if (stack.application_only() && stack.hidden_frames() &&
      (!entry.stack_pointer() || caller.stack_pointer() <= entry.stack_pointer() ||
       caller.stack_pointer() - entry.stack_pointer() != return_address_size))
    return std::nullopt;
  if (!caller.function().empty())
    return CallerKey{caller.module(), caller.function(), 0, 0};
  if (!caller.module().empty())
    return CallerKey{caller.module(), {}, caller.module_offset(), 0};
  return CallerKey{{}, {}, 0, caller.pc()};
}

std::string printable_stat_text(std::string_view text) {
  std::string result(text);
  for (char &ch : result) {
    const auto byte = static_cast<unsigned char>(ch);
    if (byte < 32 || byte == 127)
      ch = '?';
  }
  return result;
}

void print_target(std::ostream &out, const TargetKey &key, std::string_view name) {
  const auto &[probe_key, pid] = key;
  const auto &[server, session, probe] = probe_key;
  out << "function=" << std::quoted(printable_stat_text(name))
      << " server=" << std::quoted(printable_stat_text(server))
      << " session=" << std::quoted(printable_stat_text(session))
      << " pid=" << pid << " probe=" << probe;
}

void print_function_stats(std::ostream &out, const TargetKey &key,
                          const FunctionStats &stats) {
  print_target(out, key, stats.name);
  const auto &counts = stats.counts;
  out << " calls=" << counts.paired + counts.incomplete()
      << " paired=" << counts.paired << " timed=" << counts.timed
      << " missing_pre=" << counts.missing_pre
      << " missing_post=" << counts.missing_post
      << " untimed_pairs=" << counts.paired - counts.timed
      << " ambiguous=" << counts.ambiguous
      << " caller_unknown=" << stats.caller_unknown << " histogram_ns=[";
  bool first = true;
  for (std::size_t i = 0; i < stats.histogram.size(); ++i) {
    if (!stats.histogram[i])
      continue;
    if (!first)
      out << ',';
    first = false;
    out << (i ? duration_bounds[i - 1] : 0);
    if (i < duration_bounds.size())
      out << ".." << duration_bounds[i] - 1;
    else
      out << '+';
    out << ':' << stats.histogram[i];
  }
  out << "]\n";
  for (const auto &[caller, count] : stats.callers) {
    const auto &[module, function, offset, pc] = caller;
    out << "edge ";
    print_target(out, key, stats.name);
    if (!module.empty())
      out << " caller_module=" << std::quoted(printable_stat_text(module));
    if (!function.empty())
      out << " caller=" << std::quoted(printable_stat_text(function));
    else if (!module.empty())
      out << " caller_offset=0x" << std::hex << offset << std::dec;
    else
      out << " caller_pc=0x" << std::hex << pc << std::dec;
    out << " calls=" << count << '\n';
  }
}
} // namespace

void print_event_stack(const api::TraceEvent &event, std::ostream &out) {
  if (!event.has_stack()) {
    out << "no stack captured\n";
    return;
  }
  const auto flags = out.flags();
  const auto &stack = event.stack();
  out << std::dec << "stack " << api::StackTrace::Status_Name(stack.status())
      << " frames=" << stack.frames_size();
  if (stack.hidden_frames())
    out << " hidden=" << stack.hidden_frames();
  if (!stack.detail().empty())
    out << " detail=" << std::quoted(printable_stat_text(stack.detail()));
  out << '\n';
  int index = 0;
  for (const auto &frame : stack.frames()) {
    out << '#' << index++ << " 0x" << std::hex << frame.pc() << std::dec
        << ' ' << api::StackFrame::Kind_Name(frame.kind());
    if (!frame.function().empty())
      out << " function=" << std::quoted(printable_stat_text(frame.function()));
    if (!frame.module().empty()) {
      out << " module=" << std::quoted(printable_stat_text(frame.module()))
          << "+0x" << std::hex << frame.module_offset() << std::dec;
    }
    if (!frame.file().empty()) {
      out << " file=" << std::quoted(printable_stat_text(frame.file()));
      if (frame.line())
        out << ':' << frame.line();
    }
    out << '\n';
  }
  out.flags(flags);
}

void event_statistics(const EventBuffer &buffer, std::ostream &out,
                      bool calls) {
  std::map<CallKey, CallPair> pairs;
  std::map<ProbeKey, std::uint64_t> agent_losses;
  std::map<std::string_view, std::uint64_t> server_losses;
  std::uint64_t pre = 0, post = 0, exceptions = 0, other = 0, unidentified = 0;
  for (const auto &item : buffer.events()) {
    const auto &e = item->message;
    const ProbeKey probe{e.server_instance_id(), e.session_id(), e.probe_id()};
    auto &probe_loss = agent_losses[probe];
    probe_loss = std::max(probe_loss, e.agent_dropped_events());
    auto &server_loss = server_losses[e.server_instance_id()];
    server_loss = std::max(server_loss, e.server_dropped_events());
    pre += e.phase() == api::PRE;
    post += e.phase() == api::POST;
    exceptions += e.phase() == api::EXCEPTION;
    const bool paired_phase = e.phase() == api::PRE || e.phase() == api::POST || e.phase() == api::EXCEPTION;
    other += !paired_phase;
    if (e.has_context() || !paired_phase)
      continue;
    if (e.session_id().empty() || !e.probe_id() || !e.tid() || !e.call_id()) {
      ++unidentified;
      continue;
    }
    pairs[{{probe, e.pid()}, e.tid(), e.call_id()}].add(e);
  }
  const auto sum_losses = [](const auto &losses) {
    std::uint64_t total = 0;
    for (const auto &entry : losses) {
      const auto remaining = std::numeric_limits<std::uint64_t>::max() - total;
      total += std::min(remaining, entry.second);
    }
    return total;
  };

  CallCounts counts;
  std::map<TargetKey, FunctionStats> functions;
  for (const auto &[key, pair] : pairs) {
    const auto duration = pair.duration();
    counts.add(pair, duration.has_value());
    if (calls) {
      const auto *e = pair.pre ? pair.pre : pair.post;
      out << "session=" << printable_stat_text(e->session_id())
          << " probe=" << e->probe_id()
          << " tid=" << e->tid() << " call=" << e->call_id()
          << " PRE=" << (pair.pre ? "present" : "missing")
          << (pair.post && pair.post->phase() == api::EXCEPTION
                  ? " EXCEPTION=present" : pair.post ? " POST=present" : " POST=missing");
      if (duration)
        out << " duration_ns=" << *duration;
      if (pair.ambiguous)
        out << " duplicate_phase=true";
      out << '\n';
      continue;
    }
    auto &function = functions[std::get<0>(key)];
    if (function.name.empty()) {
      function.name = target_name(pair.pre);
      if (function.name.empty())
        function.name = target_name(pair.post);
    }
    function.counts.add(pair, duration.has_value());
    if (duration) {
      const auto bucket = std::upper_bound(duration_bounds.begin(),
                                           duration_bounds.end(), *duration);
      ++function.histogram[bucket - duration_bounds.begin()];
    }
    // Both phases carry entry-stack evidence. A single phase can prove an edge.
    auto caller = pair.ambiguous ? std::nullopt : direct_caller(pair.pre);
    if (!caller && !pair.ambiguous)
      caller = direct_caller(pair.post);
    if (caller)
      ++function.callers[*caller];
    else
      ++function.caller_unknown;
  }
  out << "received=" << buffer.received
      << " retained=" << buffer.events().size() << " bytes=" << buffer.bytes()
      << " evicted=" << buffer.evicted << " PRE=" << pre << " POST=" << post
      << " EXCEPTION=" << exceptions << " other_phases=" << other << " paired=" << counts.paired
      << " incomplete=" << counts.incomplete()
      << " lost(agent=" << sum_losses(agent_losses)
      << ",server=" << sum_losses(server_losses) << ')'
      << " calls=" << counts.paired + counts.incomplete()
      << " timed=" << counts.timed << " exceptional=" << counts.exceptional
      << " missing_pre=" << counts.missing_pre
      << " missing_post=" << counts.missing_post
      << " untimed_pairs=" << counts.paired - counts.timed
      << " ambiguous=" << counts.ambiguous
      << " unidentified_events=" << unidentified << '\n';
  if (calls) {
    out << "shared-context events retain invocation/task identities in event "
           "details; they are not "
           "paired by native thread/call fields\n";
  }
  for (const auto &[key, stats] : functions)
    print_function_stats(out, key, stats);
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

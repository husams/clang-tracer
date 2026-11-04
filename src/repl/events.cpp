#include "events.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include <fcntl.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/util/json_util.h>
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

EventBuffer open_recording(const std::string &path, std::size_t max_events,
                           std::size_t max_bytes) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("cannot open recording: " + path);
  }
  EventBuffer buffer(max_events, max_bytes);
  unsigned char prefix[4];
  while (input.read(reinterpret_cast<char *>(prefix), sizeof(prefix)) ||
         input.gcount()) {
    if (input.gcount() != 4) {
      throw std::runtime_error("truncated recording header");
    }
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

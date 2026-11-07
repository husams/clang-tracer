#include "context_agent_delivery.hpp"

#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <grpcpp/grpcpp.h>
#include <unistd.h>

#include "context_journal.hpp"
#include "tracer/compat.hpp"

namespace tracer::context_delivery {
namespace {

template <std::size_t N>
std::string request_string(const char (&value)[N], const char *field,
                           bool required = false) {
  const auto *end = static_cast<const char *>(std::memchr(value, '\0', N));
  if (!end)
    throw std::invalid_argument(std::string("unterminated ") + field);
  std::string result(value, end);
  if (required && result.empty())
    throw std::invalid_argument(std::string("missing ") + field);
  return result;
}

std::mutex registry_mutex;
std::unordered_map<std::string, std::weak_ptr<ContextAgentDelivery>> registry;

} // namespace

std::shared_ptr<ContextAgentDelivery>
ContextAgentDelivery::acquire(const Request &request) {
  const auto session = request_string(request.session_id, "session id", true);
  std::lock_guard lock(registry_mutex);
  for (auto current = registry.begin(); current != registry.end();) {
    if (current->second.expired())
      current = registry.erase(current);
    else
      ++current;
  }
  if (auto found = registry.find(session); found != registry.end()) {
    if (auto existing = found->second.lock())
      return existing;
    registry.erase(found);
  }
  if (registry.size() >= 64)
    throw std::runtime_error("shared-context producer session limit reached");
  auto delivery = std::shared_ptr<ContextAgentDelivery>(
      new ContextAgentDelivery(request));
  registry.emplace(session, delivery);
  return delivery;
}

ContextAgentDelivery::ContextAgentDelivery(const Request &request) {
  const auto endpoint = request_string(request.context_endpoint,
                                       "shared-context endpoint", true);
  if (request.context_delivery_profile != 1 &&
      request.context_delivery_profile != 2)
    throw std::invalid_argument("invalid shared-context delivery profile");
  if (request.context_native_pid == 0 ||
      request.context_native_pid != static_cast<std::uint32_t>(::getpid()) ||
      request.context_process_start_time == 0)
    throw std::invalid_argument("invalid native process identity for context delivery");
  const bool durable = request.context_delivery_profile == 2;
  auto open = clangtracer::context::v1::ExportOpen{};
  open.set_producer_session_id(
      request_string(request.session_id, "session id", true));
  open.set_epoch_id(1);
  open.set_protocol_version(1);
  open.set_native_pid(request.context_native_pid);
  open.set_process_start_time(request.context_process_start_time);
  open.set_profile(durable ? clangtracer::context::v1::DURABLE
                           : clangtracer::context::v1::BEST_EFFORT);
  if (durable) {
    const auto directory = request_string(request.context_journal_dir,
                                          "shared-context journal directory", true);
    const auto recording = request_string(request.context_recording_id,
                                          "shared-context recording id", true);
    const auto sink = request_string(request.context_sink_id,
                                     "shared-context sink id", true);
    journal_ = std::make_shared<ContextJournal>(directory);
    if (!journal_->healthy())
      throw std::runtime_error(journal_->error());
    const auto state = journal_->state();
    if (state.creation_confirmed) {
      if (state.recording_id != recording || state.sink_id != sink)
        throw std::runtime_error("durable journal identity does not match request");
      open.set_operation(clangtracer::context::v1::RESUME);
      open.set_saved_ack_position(state.acknowledged);
    } else {
      open.set_operation(clangtracer::context::v1::CREATE);
    }
    open.set_recording_id(recording);
    open.set_sink_id(sink);
  }

  const auto limits = stream_limits_for_event_bytes(request.context_event_bytes);
  grpc::ChannelArguments arguments;
  arguments.SetInt(GRPC_ARG_ENABLE_RETRIES, 0);
  arguments.SetMaxSendMessageSize(static_cast<int>(limits.max_packet_bytes));
  arguments.SetMaxReceiveMessageSize(4 * 1024);
  auto channel = grpc::CreateCustomChannel(
      endpoint, grpc::InsecureChannelCredentials(), arguments);
  auto confirmation = [](const clangtracer::context::v1::DurableControl &) {
    return true;
  };
  client_ = std::make_unique<ContextExportClient>(
      std::move(channel), std::move(open), limits,
      durable ? ContextExportClient::DurableConfirmation(confirmation)
              : ContextExportClient::DurableConfirmation{},
      journal_);
  if (!client_->start())
    throw std::runtime_error("shared-context exporter could not start");
}

ContextAgentDelivery::~ContextAgentDelivery() {
  if (client_)
    client_->stop();
}

bool ContextAgentDelivery::try_publish_capture(
    ::tracer::compat::span<const std::byte> event_record,
    const std::shared_ptr<const void>& schema_lease) noexcept {
  return client_ && client_->try_publish_capture(event_record, schema_lease);
}

ContextHookRing &ContextAgentDelivery::hook_ring() noexcept {
  return client_->hook_ring();
}

bool ContextAgentDelivery::ready() const noexcept {
  return client_ && client_->ready();
}

bool ContextAgentDelivery::paused() const noexcept {
  return !client_ || client_->paused();
}

ExportCounters ContextAgentDelivery::counters() const noexcept {
  return client_ ? client_->counters() : ExportCounters{};
}

} // namespace tracer::context_delivery

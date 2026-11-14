#include "background_events.hpp"

#include <chrono>
#include <utility>

namespace tracer_repl {
namespace api = clangtracer::v1;

BackgroundEvents::BackgroundEvents(const std::string& endpoint, SessionHandle handle,
                                   std::uint64_t count, unsigned timeout, std::size_t events,
                                   std::size_t bytes)
    : buffer_(events, bytes)
    , session(std::move(handle))
    , max_events(events)
    , max_bytes(bytes) {
  if (timeout) {
    context_.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(timeout));
  }
  worker_ = std::thread([this, endpoint, count] { run(endpoint, count); });
}

BackgroundEvents::~BackgroundEvents() {
  stop();
}

void BackgroundEvents::run(const std::string& endpoint, std::uint64_t count) noexcept {
  try {
    grpc::ChannelArguments arguments;
    arguments.SetMaxReceiveMessageSize(32 * 1024 * 1024);
    auto stub = api::Tracer::NewStub(
        grpc::CreateCustomChannel(endpoint, grpc::InsecureChannelCredentials(), arguments));
    api::WatchEventsRequest request;
    request.add_session_ids(session.id);
    auto reader = stub->WatchEvents(&context_, request);
    api::TraceEvent event;
    std::uint64_t received = 0;
    while ((!count || received < count) && reader->Read(&event)) {
      std::lock_guard<std::mutex> lock(mutex_);
      buffer_.push(event);
      ++received;
    }
    const bool complete = count && received == count;
    if (complete) {
      context_.TryCancel();
    }
    const auto status = reader->Finish();
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.complete = complete;
    buffer_.timed_out = status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED;
    buffer_.cancelled =
        !complete && !buffer_.timed_out && status.error_code() == grpc::StatusCode::CANCELLED;
    if (!status.ok() && !complete && !buffer_.timed_out && !buffer_.cancelled) {
      error_ = "WatchEvents " + std::to_string(status.error_code()) + ": " + status.error_message();
    } else if (status.ok() && !complete) {
      error_ = "event stream ended early; partial results retained";
    }
    running_ = false;
  } catch (const std::exception& error) {
    context_.TryCancel();
    std::lock_guard<std::mutex> lock(mutex_);
    error_ = error.what();
    running_ = false;
  }
}

Collection BackgroundEvents::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return buffer_.snapshot();
}

std::pair<std::size_t, std::size_t> BackgroundEvents::reserved() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (running_) {
    return {max_events, max_bytes};
  }
  return {buffer_.events().size(), buffer_.bytes()};
}

bool BackgroundEvents::running() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return running_;
}

std::string BackgroundEvents::error() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return error_;
}

void BackgroundEvents::stop() {
  std::lock_guard<std::mutex> lock(stop_mutex_);
  context_.TryCancel();
  if (worker_.joinable()) {
    worker_.join();
  }
}

EventRange::EventRange(std::shared_ptr<BackgroundEvents> state)
    : state_(std::move(state)) {}

Collection EventRange::snapshot() const {
  return state_->snapshot();
}

bool EventRange::running() const {
  return state_->running();
}

std::string EventRange::error() const {
  return state_->error();
}

void EventRange::stop() {
  state_->stop();
}
} // namespace tracer_repl

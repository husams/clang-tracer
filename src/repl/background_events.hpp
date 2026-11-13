#pragma once
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "events.hpp"
#include "tracer.grpc.pb.h"

namespace tracer_repl {
// The worker owns its RPC and buffer, never the interpreter or controller.
class BackgroundEvents {
  mutable std::mutex mutex_;
  std::mutex stop_mutex_;
  std::condition_variable changed_;
  EventBuffer buffer_;
  bool running_ = true;
  std::string error_;
  grpc::ClientContext context_;
  std::thread worker_;
  void run(const std::string& endpoint, std::uint64_t count) noexcept;

public:
  const SessionHandle session;
  const std::size_t max_events, max_bytes;
  BackgroundEvents(const std::string& endpoint, SessionHandle session, std::uint64_t count,
                   unsigned timeout, std::size_t max_events, std::size_t max_bytes);
  ~BackgroundEvents();
  Collection snapshot() const;
  // Blocks until an event at or after cursor is retained, then returns it and
  // advances cursor past it. Positions already evicted are skipped. Returns
  // null once the stream has ended, or on Ctrl+C, with nothing left to read.
  std::shared_ptr<const EventData> next(std::uint64_t& cursor);
  bool running() const;
  std::string error() const;
  void stop();
};
} // namespace tracer_repl

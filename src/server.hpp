#pragma once

#include <chrono>
#include <cstddef>
#include <string>

#include "tracer/protocol.hpp"

namespace tracer {

struct EventHistoryLimits {
  std::size_t events = 4096;
  std::size_t context_events = 128;
  std::size_t context_bytes = 16 * 1024 * 1024;
};

// Starts the synchronous gRPC service and returns after orderly shutdown.
int RunServer(const std::string &agent_path, const std::string &listen_uri,
              std::chrono::seconds operation_timeout,
              const std::string &context_durable_directory = {},
              EventHistoryLimits history_limits = {},
              std::uint32_t context_event_bytes = kDefaultContextEventBytes);

} // namespace tracer

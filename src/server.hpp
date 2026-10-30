#pragma once

#include <chrono>
#include <string>

namespace tracer {

// Starts the synchronous gRPC service and returns after orderly shutdown.
int RunServer(const std::string &agent_path, const std::string &listen_uri,
              std::chrono::seconds operation_timeout);

} // namespace tracer

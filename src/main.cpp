#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "runtime_paths.hpp"
#include "server.hpp"

namespace {

unsigned parse_positive(const std::string &text, const char *option) {
  std::size_t used = 0;
  unsigned long value = 0;
  try {
    value = std::stoul(text, &used);
  } catch (...) {
    throw std::invalid_argument(std::string("invalid value for ") + option);
  }
  if (used != text.size() || value == 0 || value > 300) {
    throw std::invalid_argument(std::string("invalid value for ") + option);
  }
  return static_cast<unsigned>(value);
}

int usage() {
  std::cout
      << "Usage: clang-tracer-server [--agent LIBRARY] [--listen "
         "unix:///PATH|127.0.0.1:PORT] "
         "[--timeout SECONDS] [--context-durable-dir DIRECTORY]\n"
         "  --event-history-limit N (default 4096)\n"
         "  --context-history-limit N (default 128)\n"
         "  --context-history-bytes N (default 16777216; serialized bytes)\n"
         "  --context-event-bytes N (default 262144; bytes per shared event)\n"
         "  --context-transport-memory-bytes N (default 67108864)\n"
         "Starts the native multi-process tracing gRPC server. Default "
         "endpoint is a "
         "private Unix socket; Linux x86_64 only.\n"
         "Shared-context streaming is enabled by default; "
         "--context-durable-dir enables resumable durable recording.\n"
         "Agent lookup: --agent, CLANG_TRACER_AGENT, installed library, "
         "then adjacent build library.\n";
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  try {
    std::string agent;
    std::string listen;
    std::string context_durable_directory;
    unsigned timeout = 10;
    tracer::EventHistoryLimits history_limits;
    tracer::ContextTransportBudget context_budget;
    for (int i = 1; i < argc; ++i) {
      const std::string option = argv[i];
      if (option == "--help")
        return usage();
      if (option != "--agent" && option != "--listen" &&
          option != "--timeout" && option != "--context-durable-dir" &&
          option != "--event-history-limit" &&
          option != "--context-history-limit" &&
          option != "--context-history-bytes" && option != "--context-event-bytes" &&
          option != "--context-transport-memory-bytes") {
        throw std::invalid_argument("unknown option: " + option);
      }
      if (++i == argc)
        throw std::invalid_argument("missing value for " + option);
      const std::string value = argv[i];
      if (option == "--agent")
        agent = value;
      else if (option == "--listen")
        listen = value;
      else if (option == "--context-durable-dir")
        context_durable_directory = value;
      else if (option == "--timeout") {
        timeout = parse_positive(value, "--timeout");
      } else {
        if (value.empty() ||
            value.find_first_not_of("0123456789") != std::string::npos) {
          throw std::invalid_argument("invalid value for " + option);
        }
        const auto limit = std::stoull(value);
        if (limit == 0 || limit > std::numeric_limits<std::size_t>::max() / 2) {
          throw std::invalid_argument("invalid value for " + option);
        }
        if (option == "--event-history-limit") {
          history_limits.events = static_cast<std::size_t>(limit);
        } else if (option == "--context-history-limit") {
          history_limits.context_events = static_cast<std::size_t>(limit);
        } else if (option == "--context-event-bytes") {
          if (limit > std::numeric_limits<std::uint32_t>::max())
            throw std::invalid_argument("invalid value for " + option);
          context_budget.event_bytes = static_cast<std::uint32_t>(limit);
        } else if (option == "--context-transport-memory-bytes") {
          context_budget.memory_bytes = limit;
        } else {
          history_limits.context_bytes = static_cast<std::size_t>(limit);
        }
      }
    }
    if (agent.empty())
      agent = tracer::runtime::default_agent_path();
    return tracer::RunServer(agent, listen, std::chrono::seconds(timeout),
                             context_durable_directory, history_limits, context_budget);
  } catch (const std::exception &error) {
    std::cerr << "clang-tracer: " << error.what() << '\n';
    return 1;
  }
}

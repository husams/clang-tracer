#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
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
  std::cout << "Usage: clang-tracer-server [--agent LIBRARY] [--listen "
               "unix:///PATH|127.0.0.1:PORT] "
               "[--timeout SECONDS]\n"
               "Starts the native multi-process tracing gRPC server. Default "
               "endpoint is a "
               "private Unix socket; Linux x86_64 only.\n"
               "Agent lookup: --agent, CLANG_TRACER_AGENT, installed library, "
               "then adjacent build library.\n";
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  try {
    std::string agent;
    std::string listen;
    unsigned timeout = 10;
    for (int i = 1; i < argc; ++i) {
      const std::string option = argv[i];
      if (option == "--help")
        return usage();
      if (option != "--agent" && option != "--listen" &&
          option != "--timeout") {
        throw std::invalid_argument("unknown option: " + option);
      }
      if (++i == argc)
        throw std::invalid_argument("missing value for " + option);
      const std::string value = argv[i];
      if (option == "--agent")
        agent = value;
      else if (option == "--listen")
        listen = value;
      else
        timeout = parse_positive(value, "--timeout");
    }
    if (agent.empty())
      agent = tracer::runtime::default_agent_path();
    return tracer::RunServer(agent, listen, std::chrono::seconds(timeout));
  } catch (const std::exception &error) {
    std::cerr << "clang-tracer: " << error.what() << '\n';
    return 1;
  }
}

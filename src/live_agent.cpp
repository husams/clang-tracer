#include "live_agent_protocol.hpp"
#include "live_update.hpp"
#include "tracer.pb.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>
#include <mutex>
#include <stdexcept>

namespace tracer::live {
namespace {
struct Images {
  std::mutex mutex;
  std::uint64_t last_token = 0;
  std::map<std::uint64_t, std::shared_ptr<JitImage>> retained;
};

Images &images() {
  // Intentionally process-lifetime: target pointers or frames may outlive the
  // controller and must remain callable during normal DSO teardown.
  static auto *state = new Images;
  return *state;
}

std::pair<std::uintptr_t, std::size_t> executable_mapping(std::uintptr_t address) {
  std::ifstream input("/proc/self/maps");
  std::string line;
  while (std::getline(input, line)) {
    unsigned long begin = 0, end = 0;
    char flags[5]{};
    if (std::sscanf(line.c_str(), "%lx-%lx %4s", &begin, &end, flags) == 3 &&
        flags[2] == 'x' && address >= begin && address < end)
      return {begin, end - begin};
  }
  throw std::runtime_error("ORC address lacks an executable target mapping");
}

JitRequest jit_input(const AgentRequest &request) {
  clangtracer::v1::LiveJitInput input;
  if (!request.input || !request.input_size || request.input_size > max_payload_bytes ||
      !input.ParseFromArray(reinterpret_cast<const void *>(request.input), request.input_size))
    throw std::runtime_error("invalid live JIT source payload");
  JitRequest result;
  for (const auto &source : input.sources())
    result.files.push_back({source.path(), source.contents()});
  for (const auto &unit : input.units())
    result.translation_units.push_back({unit.path(), unit.directory(),
        {unit.compiler_arguments().begin(), unit.compiler_arguments().end()}});
  for (const auto &[name, address] : input.imports())
    result.imports.emplace(name, address);
  result.entry_symbols.assign(input.entry_symbols().begin(), input.entry_symbols().end());
  if (result.entry_symbols.empty() || result.entry_symbols.size() > max_entries)
    throw std::runtime_error("invalid live JIT entry count");
  return result;
}
} // namespace
} // namespace tracer::live

extern "C" int tracer_agent_live_v1(tracer::live::AgentRequest *request) noexcept {
  using namespace tracer::live;
  if (!request || request->version != agent_magic)
    return -1;
  try {
    auto &state = images();
    std::lock_guard lock(state.mutex);
    request->error[0] = '\0';
    request->retained = state.retained.size();
    switch (request->action) {
    case AgentRequest::Action::status:
      return 0;
    case AgentRequest::Action::discard:
      state.retained.erase(request->token);
      request->retained = state.retained.size();
      return 0;
    case AgentRequest::Action::stage: {
      if (!request->limit || state.retained.size() >= request->limit)
        throw std::runtime_error("live JIT retained generation limit reached");
      auto input = jit_input(*request);
      auto image = compile_in_target(input);
      request->count = input.entry_symbols.size();
      for (std::size_t i = 0; i < input.entry_symbols.size(); ++i) {
        request->addresses[i] = image->lookup(input.entry_symbols[i]);
        const auto [begin, size] = executable_mapping(request->addresses[i]);
        request->mapping_begin[i] = begin;
        request->mapping_size[i] = size;
      }
      const auto token = state.last_token + 1;
      if (!token)
        throw std::runtime_error("live JIT generation identifiers exhausted");
      state.retained.emplace(token, std::move(image));
      state.last_token = request->token = token;
      request->retained = state.retained.size();
      return 0;
    }
    }
    throw std::runtime_error("unknown live JIT agent action");
  } catch (const std::exception &error) {
    std::snprintf(request->error, sizeof(request->error), "%s", error.what());
    return 1;
  } catch (...) {
    std::snprintf(request->error, sizeof(request->error), "unexpected live JIT failure");
    return 1;
  }
}

#include "live_agent_extension.hpp"

#include "live_agent_protocol.hpp"
#include "live_data.hpp"
#include "live_migration.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <thread>

namespace tracer::live {
namespace {
namespace api = clangtracer::v1;
using Status = api::LiveUpdateStatus;
using Extension = api::LiveAgentExtension;
using Phase = AgentRequest::Publication;
using Outcome = AgentRequest::CodeOutcome;

struct LiveWorker {
  std::mutex mutex;
  Phase phase = Phase::none;
  managed::CodeResult outcome{managed::CodeState::unchanged, {}};
  std::vector<EntryImage> entries;
  std::uint64_t last = 0;
  std::uint64_t token = 0;
  std::future<Status> pending;
  std::optional<Status> response;
  std::map<std::string, LiveDataSession> data;
  std::map<std::uint64_t, std::unique_ptr<managed::Revision>> managed;
};

LiveWorker &worker() {
  // Invoked code and object ownership can outlive the control connection.
  static auto *state = new LiveWorker;
  return *state;
}

void migration_result(Status &output, const managed::Result &result) {
  switch (result.status) {
  case managed::Status::applied: output.set_state(Status::APPLIED); break;
  case managed::Status::stale: output.set_state(Status::STALE); break;
  case managed::Status::restoration_required:
    output.set_state(Status::RESTORATION_REQUIRED);
    break;
  default: output.set_state(Status::BLOCKED); break;
  }
  output.set_diagnostic(result.detail + " instances=" + std::to_string(result.instances));
}

managed::CodeResult request_publication(Phase phase, std::vector<EntryImage> entries = {}) {
  auto &state = worker();
  std::unique_lock lock(state.mutex);
  state.entries = std::move(entries);
  state.phase = phase;
  // Once the server may have started writing, keep managed access gated
  // until it reports the verified outcome (even after the hook deadline).
  while (state.phase != Phase::none) {
    lock.unlock();
    // Supply a rendezvous even when every application thread is gated.
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    lock.lock();
  }
  return std::move(state.outcome);
}

Status execute(const Extension &input, std::uint64_t token) {
  Status output;
  try {
    auto &state = worker();
    const auto deadline = std::chrono::steady_clock::time_point(
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::nanoseconds(input.deadline_nanoseconds())));
    if (std::chrono::steady_clock::now() >= deadline)
      throw std::runtime_error("live operation expired before starting");
    if (input.action() == Extension::MANAGED_UPDATE) {
      using GetRegistry = managed::Registry *(*)() noexcept;
      auto registry = reinterpret_cast<GetRegistry>(input.registry());
      auto *storage = registry ? registry() : nullptr;
      if (!storage)
        throw managed::RebuildRequired("rebuild required: missing application-owned registry");
      auto jit = decode_jit(input.update());
      if (jit.entry_symbols.size() > max_entries ||
          input.publish_native() != !jit.entry_symbols.empty())
        throw std::runtime_error("invalid managed native entry count");
      auto revision = managed::stage(*storage, jit,
          {input.hook_file(), input.hook_function(), input.hook_type()});
      managed::Publication publication;
      if (input.publish_native()) {
        std::vector<EntryImage> entries;
        for (const auto &symbol : jit.entry_symbols)
          entries.push_back(describe_entry(*revision->image(), symbol));
        // A hook can throw after a native entry has been visible. Keep its
        // image resident even when the transaction restores the old bytes.
        retain_image(revision->image(), input.retention_limit());
        publication.install = [entries = std::move(entries)] {
          return request_publication(Phase::install, entries);
        };
        publication.restore = [] { return request_publication(Phase::restore); };
      }
      auto item = state.managed.emplace(token, std::move(revision)).first;
      const auto result = item->second->apply(deadline, publication);
      if (result.status != managed::Status::applied &&
          result.status != managed::Status::restoration_required)
        state.managed.erase(item);
      migration_result(output, result);
      return output;
    }
    if (input.action() == Extension::MANAGED_REVERT) {
      auto item = state.managed.find(input.revision());
      if (item == state.managed.end())
        throw std::runtime_error("unknown managed layout revision");
      managed::Publication publication;
      if (input.publish_native()) {
        publication.install = [] { return request_publication(Phase::install); };
        publication.restore = [] { return request_publication(Phase::restore); };
      }
      const auto result = item->second->revert(deadline, publication);
      if (result.status == managed::Status::applied)
        state.managed.erase(item);
      migration_result(output, result);
      return output;
    }

    const LiveDataTarget target{input.process_start(), input.module(), input.build_id()};
    if (input.session().empty())
      throw std::runtime_error("live data operation lacks a session identity");
    auto &session = state.data[input.session()];
    std::optional<LiveDataDiscovery> discovery;
    if (input.has_discovery())
      discovery = LiveDataDiscovery{decode_jit(input.discovery()), input.discovery_function()};
    if (input.action() == Extension::HEAP_SCAN) {
      const std::vector<std::string> types(input.types().begin(), input.types().end());
      const auto report = session.scan_heap(target, types, discovery);
      output.set_state(Status::CURRENT);
      for (const auto &item : report.candidates) {
        auto *candidate = output.add_candidates();
        candidate->set_state(Status::CURRENT);
        candidate->set_symbol(item.id);
        candidate->set_signature(item.type);
        candidate->set_module(item.module);
        candidate->set_source(item.root);
        candidate->set_address(item.address);
        candidate->set_diagnostic(item.evidence);
      }
      for (const auto &limit : report.limitations)
        output.add_notes(limit);
      return output;
    }
    if (input.action() != Extension::DATA_UPDATE)
      throw std::runtime_error("unknown live extension action");

    LiveDataSelection selection;
    selection.module = input.module();
    if (input.address())
      selection.address = input.address();
    selection.scan_ids.assign(input.scan_ids().begin(), input.scan_ids().end());
    const auto report = session.update_data(target, decode_jit(input.update()), discovery, selection);
    std::size_t updated = 0, failed = 0;
    for (const auto &item : report.objects) {
      auto *candidate = output.add_candidates();
      switch (item.outcome) {
      case LiveObjectOutcome::updated:
        candidate->set_state(Status::APPLIED);
        ++updated;
        break;
      case LiveObjectOutcome::skipped:
        candidate->set_state(Status::UNCHANGED);
        break;
      case LiveObjectOutcome::failed:
        candidate->set_state(Status::BLOCKED);
        ++failed;
        break;
      }
      candidate->set_symbol(item.id);
      candidate->set_signature(item.type);
      candidate->set_module(target.module);
      candidate->set_source(item.root);
      candidate->set_address(item.address);
      candidate->set_diagnostic(item.detail);
    }
    for (const auto &root : report.roots)
      output.add_notes("Discovery root " + root);
    for (const auto &limit : report.limitations)
      output.add_notes(limit);
    output.set_state(updated ? Status::APPLIED : failed ? Status::BLOCKED : Status::UNCHANGED);
    output.set_diagnostic("updated=" + std::to_string(updated) +
        " skipped=" + std::to_string(report.objects.size() - updated - failed) +
        " failed=" + std::to_string(failed) + " duplicates=" + std::to_string(report.duplicates));
  } catch (const std::exception &error) {
    output.set_state(Status::BLOCKED);
    output.set_diagnostic(error.what());
  } catch (...) {
    output.set_state(Status::BLOCKED);
    output.set_diagnostic("unexpected live extension failure");
  }
  return output;
}
} // namespace

void execute_extension(AgentRequest &request) {
  auto &state = worker();
  std::lock_guard lock(state.mutex);
  if (request.action == AgentRequest::Action::launch) {
    if (state.pending.valid() || state.response)
      throw std::runtime_error("a live extension is still active or awaiting its result");
    Extension input;
    if (!request.input || !request.input_size || request.input_size > max_payload_bytes ||
        !input.ParseFromArray(reinterpret_cast<const void *>(request.input), request.input_size))
      throw std::runtime_error("invalid live extension payload");
    if (state.last == UINT64_MAX)
      throw std::runtime_error("live extension identifiers exhausted");
    const auto token = state.last + 1;
    std::packaged_task<Status()> task([input = std::move(input), token] {
      auto status = execute(input, token);
      status.set_operation_token(token);
      return status;
    });
    auto pending = task.get_future();
    std::thread(std::move(task)).detach();
    state.pending = std::move(pending);
    state.last = state.token = request.token = token;
    return;
  }
  if ((!state.pending.valid() && !state.response) || request.token != state.token)
    throw std::runtime_error("unknown live extension operation");
  if (request.action == AgentRequest::Action::publication_result) {
    if (state.phase == Phase::none || request.publication != state.phase ||
        !std::memchr(request.error, 0, sizeof(request.error)))
      throw std::runtime_error("invalid managed publication acknowledgement");
    auto code = managed::CodeState::restoration_required;
    switch (request.code_outcome) {
    case Outcome::installed: code = managed::CodeState::installed; break;
    case Outcome::unchanged: code = managed::CodeState::unchanged; break;
    case Outcome::stale: code = managed::CodeState::stale; break;
    case Outcome::uncertain: break;
    }
    state.outcome = {code, request.error};
    state.phase = Phase::none;
    state.entries.clear();
    return;
  }
  if (state.phase != Phase::none) {
    request.publication = state.phase;
    request.count = state.entries.size();
    for (std::size_t i = 0; i < state.entries.size(); ++i) {
      request.addresses[i] = state.entries[i].address;
      request.mapping_begin[i] = state.entries[i].mapping_begin;
      request.mapping_size[i] = state.entries[i].mapping_size;
    }
    return;
  }
  if (!state.response && state.pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    return;
  request.retained = retained_image_count();
  if (!state.response)
    state.response = state.pending.get();
  auto size = state.response->ByteSizeLong();
  if (size > max_payload_bytes) {
    state.response->clear_candidates();
    state.response->clear_notes();
    state.response->set_diagnostic(state.response->diagnostic() +
        " (individual results exceed the transport budget)");
    size = state.response->ByteSizeLong();
  }
  request.output_size = size;
  if (!request.output || request.output_capacity < size)
    return;
  if (!state.response->SerializeToArray(reinterpret_cast<void *>(request.output), size))
    throw std::runtime_error("cannot serialize live extension result");
  state.response.reset();
  request.ready = 1;
}

} // namespace tracer::live

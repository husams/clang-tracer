#include "session.hpp"

#include "injector.hpp"
#include "live_agent_protocol.hpp"
#include "live_extension_plan.hpp"
#include "live_mock_plan.hpp"
#include "live_plan.hpp"
#include "native_detour.hpp"
#include "proc_identity.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <set>
#include <thread>

namespace tracer {
namespace {
namespace api = clangtracer::v1;
using Status = api::LiveUpdateStatus;
using Action = api::LiveUpdateRequest;
using live::AgentRequest;

std::string payload(const live::JitRequest &request) {
  api::LiveJitInput wire;
  for (const auto &file : request.files) {
    auto *output = wire.add_sources();
    output->set_path(file.path);
    output->set_contents(file.contents);
  }
  auto units = request.translation_units;
  if (units.empty()) {
    for (const auto &file : request.files) {
      const auto source = std::filesystem::path(file.path);
      const auto suffix = source.extension();
      if (suffix == ".cpp" || suffix == ".cc" || suffix == ".cxx" || suffix == ".C")
        units.push_back({file.path, source.parent_path().string(), request.compiler_arguments});
    }
  }
  for (const auto &unit : units) {
    auto *output = wire.add_units();
    output->set_path(unit.path);
    output->set_directory(unit.directory);
    for (const auto &argument : unit.compiler_arguments)
      output->add_compiler_arguments(argument);
  }
  for (const auto &[name, address] : request.imports) {
    auto *item = wire.add_imports();
    item->set_name(name);
    item->set_address(address);
  }
  for (const auto &name : request.entry_symbols)
    wire.add_entry_symbols(name);
  if (wire.ByteSizeLong() > live::max_payload_bytes)
    throw std::runtime_error("live JIT snapshot exceeds the transport budget");
  return wire.SerializeAsString();
}

AgentRequest invoke(injector::Process &process, std::uintptr_t entry, AgentRequest request,
                    const std::string &input = {}, std::string *output = nullptr) {
  if (!entry)
    throw std::runtime_error("attached agent has no live JIT service");
  const auto capacity = output ? request.output_capacity : 0;
  if (capacity > live::max_payload_bytes)
    throw std::runtime_error("live extension response exceeds the transport budget");
  const auto size = sizeof(request) + input.size() + capacity;
  const auto remote = process.remote_allocate(size);
  try {
    if (!input.empty()) {
      request.input = remote + sizeof(request);
      request.input_size = input.size();
      process.remote_write(request.input, input.data(), input.size());
    }
    if (output)
      request.output = remote + sizeof(request) + input.size();
    process.remote_write(remote, &request, sizeof(request));
    const auto returned = process.remote_call(entry, {remote}, 1);
    process.remote_read(remote, &request, sizeof(request));
    if (request.version != live::agent_magic || !std::memchr(request.error, 0, sizeof(request.error)))
      throw std::runtime_error("invalid response from the live JIT service");
    if (returned)
      throw std::runtime_error(request.error[0] ? request.error : "live JIT service failed");
    if (output && request.ready) {
      if (!request.output_size || request.output_size > capacity)
        throw std::runtime_error("agent returned an invalid live report size");
      output->resize(request.output_size);
      process.remote_read(remote + sizeof(request) + input.size(), output->data(), output->size());
    }
  } catch (...) {
    if (!process.unsafe()) {
      try { process.remote_free(remote, size); } catch (...) {}
    }
    throw;
  }
  process.remote_free(remote, size);
  return request;
}

void report_revision(Status &output, const live::Transaction &state) {
  output.set_active_revision(state.active);
  output.set_retained_generations(state.retained);
  output.set_retention_limit(state.limit);
}

std::string jump_to(std::size_t size, std::uintptr_t address) {
  if (size < native_detour::kJumpBytes || !address)
    throw std::runtime_error("invalid verified entry overwrite or replacement address");
  std::string bytes(size, static_cast<char>(0x90));
  constexpr unsigned char jump[]{0xff, 0x25, 0, 0, 0, 0};
  std::memcpy(bytes.data(), jump, sizeof(jump));
  std::memcpy(bytes.data() + sizeof(jump), &address, sizeof(address));
  return bytes;
}

bool publish(std::uint32_t pid, std::uint64_t process_start,
             injector::Process &process, const live::Revision &from,
             const live::Revision &to, const std::function<void()> &before_write,
             const std::function<void()> &committed, bool registry_gated = false) {
  std::set<std::uintptr_t> addresses;
  for (const auto &[address, value] : from.entries) {
    (void)value;
    addresses.insert(address);
  }
  for (const auto &[address, value] : to.entries) {
    (void)value;
    addresses.insert(address);
  }
  std::vector<injector::CodePatch> patches, drained;
  std::vector<const live::Entry *> bindings;
  for (const auto address : addresses) {
    const auto old = from.entries.find(address);
    const auto next = to.entries.find(address);
    const auto &entry = old == from.entries.end() ? next->second.binding : old->second.binding;
    const auto &expected = old == from.entries.end() ? entry.original_bytes : old->second.installed_bytes;
    const auto &installed = next == to.entries.end() ? entry.original_bytes : next->second.installed_bytes;
    if (expected == installed)
      continue;
    if (expected.size() != installed.size())
      throw live::Stale("overwrite window changed for " + entry.symbol);
    patches.push_back({address, expected.data(), installed.data(), expected.size()});
    bindings.push_back(&entry);
    if (!registry_gated && (from.safe_point == live::SafePoint::drained_stack ||
                            to.safe_point == live::SafePoint::drained_stack)) {
      drained.push_back({entry.address, nullptr, nullptr, entry.function_size});
      if (old != from.entries.end())
        drained.push_back({old->second.mapping_begin, nullptr, nullptr, old->second.mapping_size});
    }
  }
  if (before_write)
    before_write();
  const auto validate = [&] {
    if (proc::start_time(proc::directory(pid) / "stat") != process_start)
      throw live::Stale("target process identity changed before publication");
    for (const auto *entry : bindings)
      live::validate_entry(pid, *entry);
  };
  if (patches.empty()) {
    validate();
    committed();
    return true;
  }
  process.attach_for_patch(static_cast<pid_t>(pid));
  if (!process.patch_code_batch(patches, validate, committed, drained))
    return false;
  process.detach();
  return true;
}
void read_entries(live::Revision &next, const std::vector<live::Entry> &entries,
                  const AgentRequest &response) {
  if (response.count != entries.size() || entries.size() > live::max_entries)
    throw std::runtime_error("live JIT service returned incomplete entry addresses");
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const auto &entry = entries[i];
    if (!response.mapping_begin[i] || !response.mapping_size[i] ||
        response.addresses[i] < response.mapping_begin[i] ||
        response.addresses[i] - response.mapping_begin[i] >= response.mapping_size[i])
      throw std::runtime_error("agent returned an unverified JIT executable mapping");
    next.entries[entry.address] = {
        entry, jump_to(entry.original_bytes.size(), response.addresses[i]),
        response.addresses[i], response.mapping_begin[i], response.mapping_size[i]};
  }
}

void publish_managed(AgentRequest &request, std::uint32_t pid, std::uint64_t start,
                     const live::Revision &before, const live::Revision &after,
                     const std::function<void()> &validate) {
  using Outcome = AgentRequest::CodeOutcome;
  injector::Process process;
  bool written = false;
  try {
    if (!publish(pid, start, process, before, after, validate, [&] { written = true; }, true))
      throw std::runtime_error("managed native entry or affected frame is still active");
    request.code_outcome = Outcome::installed;
  } catch (const live::Stale &error) {
    request.code_outcome = Outcome::stale;
    std::snprintf(request.error, sizeof(request.error), "%s", error.what());
  } catch (const std::exception &error) {
    request.code_outcome = written || process.unsafe() ? Outcome::uncertain : Outcome::unchanged;
    std::snprintf(request.error, sizeof(request.error), "%s", error.what());
  }
}
} // namespace

Status Session::run_live_extension_locked(api::LiveAgentExtension input,
    const std::function<void(AgentRequest &)> &publication) {
  injector::Process process;
  const auto deadline = std::chrono::steady_clock::now() + operation_timeout_;
  input.set_deadline_nanoseconds(std::chrono::duration_cast<std::chrono::nanoseconds>(
      deadline.time_since_epoch()).count());
  const auto bytes = input.SerializeAsString();
  if (bytes.size() > live::max_payload_bytes)
    throw std::runtime_error("live extension snapshots exceed the transport budget");
  try {
    attach_process(process, deadline);
    AgentRequest start;
    start.action = AgentRequest::Action::launch;
    const auto launch = invoke(process, live_address_, start, bytes);
    if (!launch.token)
      throw std::runtime_error("agent did not start the live operation");
    live_.pending_extension = launch.token;
    process.detach();

    AgentRequest poll;
    poll.action = AgentRequest::Action::poll;
    poll.token = launch.token;
    const auto report_deadline = deadline + std::chrono::seconds(5);
    for (;;) {
      // Reattach briefly to check progress. Application code runs between
      // polls, including on the thread used to invoke the agent entry.
      attach_process(process, report_deadline);
      const auto response = invoke(process, live_address_, poll);
      if (response.publication != AgentRequest::Publication::none) {
        process.detach();
        auto acknowledgement = response;
        acknowledgement.action = AgentRequest::Action::publication_result;
        try {
          if (!publication)
            throw std::runtime_error("unexpected managed code publication request");
          publication(acknowledgement);
        } catch (const std::exception &error) {
          acknowledgement.code_outcome = AgentRequest::CodeOutcome::unchanged;
          std::snprintf(acknowledgement.error, sizeof(acknowledgement.error), "%s", error.what());
        }
        attach_process(process, report_deadline);
        invoke(process, live_address_, acknowledgement);
        process.detach();
        continue;
      }
      if (response.output_size) {
        std::string serialized;
        poll.output_capacity = response.output_size;
        const auto filled = invoke(process, live_address_, poll, {}, &serialized);
        if (!filled.ready)
          throw std::runtime_error("live agent response changed during retrieval");
        Status output;
        if (!output.ParseFromString(serialized))
          throw std::runtime_error("cannot decode live agent response");
        process.detach();
        live_.pending_extension = 0;
        live_.retained = filled.retained;
        report_revision(output, live_);
        if (output.state() == Status::RESTORATION_REQUIRED) {
          unsafe_target_ = true;
          set_failed_locked(output.diagnostic());
        }
        output.set_operation_token(launch.token);
        return output;
      }
      process.detach();
      if (std::chrono::steady_clock::now() >= report_deadline)
        throw std::runtime_error("live operation is still running in the target");
      std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
  } catch (...) {
    if (process.unsafe()) {
      unsafe_target_ = true;
      set_failed_locked("live extension rendezvous could not safely restore the target");
    }
    throw;
  }
}

Status Session::apply_live_extension_locked(const Action &request, Status result,
                                             const std::function<bool()> &cancelled) {
  const auto previous = live_.active;
  const auto &before = live_.revisions.at(previous);
  auto planned = live::plan_extension(pid_, process_start_, id_, executable_, request, before, cancelled);
  for (const auto &candidate : planned.native.candidates) {
    *result.add_candidates() = candidate;
    if (candidate.state() == Status::BLOCKED)
      result.set_state(Status::BLOCKED);
  }
  if (result.state() == Status::BLOCKED) {
    result.set_diagnostic("managed update has an incompatible native entry");
    return result;
  }
  if (planned.validate)
    planned.validate();
  if (cancelled && cancelled())
    throw live::Stale("live extension cancelled before launch");
  const auto managed = planned.input.action() == api::LiveAgentExtension::MANAGED_UPDATE;
  if (managed && request.action() == Action::PREVIEW) {
    result.set_state(Status::PLANNED);
    result.set_diagnostic("managed layout, instances and hook will be validated in the target");
    return result;
  }
  if (!managed)
    return run_live_extension_locked(planned.input);

  const auto next_id = live_.last_committed + 1;
  if (!next_id)
    throw std::runtime_error("live revision identifiers exhausted");
  const auto [record, inserted] = live_.revisions.emplace(next_id, live_.revisions.at(previous));
  if (!inserted)
    throw std::runtime_error("duplicate managed live revision");
  record->second.parent = previous;
  record->second.managed = 0;
  auto &next = record->second;
  next.managed_native = !planned.native.entries.empty();
  if (!planned.native.entries.empty())
    next.safe_point = planned.native.safe_point;
  for (const auto &source : planned.native.accepted_sources)
    next.sources[source.path] = source;
  const auto limit = request.retention_limit() ? request.retention_limit() : live_.limit;
  planned.input.set_retention_limit(limit);
  try {
    result = run_live_extension_locked(planned.input, [&](AgentRequest &phase) {
      if (phase.publication == AgentRequest::Publication::install) {
        read_entries(next, planned.native.entries, phase);
        publish_managed(phase, pid_, process_start_, before, next, [&] {
          if (cancelled && cancelled())
            throw live::Stale("managed update cancelled before publication");
          planned.validate();
        });
      } else {
        publish_managed(phase, pid_, process_start_, next, before, {});
      }
    });
  } catch (...) {
    live_.revisions.erase(record);
    throw;
  }
  if (result.state() == Status::APPLIED) {
    record->second.managed = result.operation_token();
    live_.active = live_.last_committed = next_id;
    if (!planned.native.entries.empty())
      live_.limit = limit;
    report_revision(result, live_);
  } else if (result.state() != Status::RESTORATION_REQUIRED) {
    live_.revisions.erase(record);
  }
  for (auto candidate : planned.native.candidates) {
    candidate.set_state(result.state());
    *result.add_candidates() = std::move(candidate);
  }
  return result;
}

clangtracer::v1::LiveUpdateStatus Session::live_update(
    const clangtracer::v1::LiveUpdateRequest &request, const std::function<bool()> &cancelled) {
  std::lock_guard lock(mutex_);
  Status result;
  report_revision(result, live_);
  if (unsafe_target_) {
    result.set_state(Status::RESTORATION_REQUIRED);
    result.set_diagnostic(error_);
    return result;
  }
  try {
    check_identity_locked();
    if (state_ != api::Session::ATTACHED)
      throw std::runtime_error("select an attached target for live update");
    if (live_.pending_extension)
      throw std::runtime_error("a live extension outcome is unresolved; inspect the target before further updates");
    if (request.action() == Action::STATUS) {
      result.set_state(Status::CURRENT);
      for (const auto &[address, active] : live_.revisions.at(live_.active).entries) {
        auto *candidate = result.add_candidates();
        candidate->set_state(Status::APPLIED);
        candidate->set_symbol(active.binding.symbol);
        candidate->set_signature(active.binding.signature);
        candidate->set_source(active.binding.source);
        candidate->set_module(active.binding.module);
        candidate->set_address(address);
      }
      return result;
    }
    require_probe_control_locked();
    if (std::any_of(probes_.begin(), probes_.end(), [](const auto &item) {
          return item.second.patch_committed || item.second.value.draining();
        }))
      throw std::runtime_error("remove active or draining trace probes before a live code transition");
    if (request.action() == Action::REVERT)
      return revert_live_locked(request.has_exact_revision() ? request.revision() :
                                    live_.revisions.at(live_.active).parent);
    const auto expected_revision = live_.active;
    if (request.action() == Action::MOCK) {
      const auto plan = live::plan_live_mock(pid_, executable_, request,
                                             live_.revisions.at(expected_revision), cancelled);
      for (const auto &candidate : plan.candidates)
        result.add_candidates()->CopyFrom(candidate);
      return activate_live_locked(plan, expected_revision, request.retention_limit(),
                                  cancelled, std::move(result));
    }
    if (request.has_preview_revision() && request.preview_revision() != expected_revision)
      throw live::Stale("active revision changed after preview; rediscover the update");
    if (request.action() == Action::SCAN_HEAP || request.action() == Action::UPDATE_DATA ||
        ((request.action() == Action::UPDATE || request.action() == Action::PREVIEW) &&
         !request.options().empty()))
      return apply_live_extension_locked(request, std::move(result), cancelled);
    if (request.action() != Action::UPDATE && request.action() != Action::PREVIEW)
      throw std::runtime_error("unknown live update action");
    auto plan = live::plan_sources(pid_, executable_, request, live_.revisions.at(expected_revision), cancelled);
    bool blocked = false;
    for (const auto &candidate : plan.candidates) {
      result.add_candidates()->CopyFrom(candidate);
      blocked |= candidate.state() == Status::BLOCKED;
    }
    if (blocked) {
      result.set_state(Status::BLOCKED);
      result.set_diagnostic("submitted group lacks independent coverage evidence");
      return result;
    }
    if (plan.entries.empty()) {
      result.set_state(Status::UNCHANGED);
      result.set_diagnostic("no changed native application entry");
      return result;
    }
    if (request.action() == Action::PREVIEW) {
      result.set_state(Status::PLANNED);
      return result;
    }
    return activate_live_locked(plan, expected_revision, request.retention_limit(),
                                cancelled, std::move(result));
  } catch (const std::exception &error) {
    result.set_state(unsafe_target_ || live_.pending_extension ? Status::RESTORATION_REQUIRED :
                     dynamic_cast<const live::Stale *>(&error) ? Status::STALE : Status::BLOCKED);
    result.set_diagnostic(error.what());
  }
  return result;
}

clangtracer::v1::LiveUpdateStatus Session::activate_live_locked(
    const live::Plan &plan, std::uint64_t expected_revision, std::uint64_t retention_limit,
    const std::function<bool()> &cancelled, Status result) {
  injector::Process process;
  std::uint64_t staged_token = 0;
  bool did_commit = false;
  try {
    if (plan.entries.size() > live::max_entries)
      throw std::runtime_error("submitted group exceeds the live JIT entry budget");

    if (plan.validate_sources)
      plan.validate_sources();
    if (plan.jit.entry_symbols.size() != plan.entries.size())
      throw std::runtime_error("each native entry needs one exact JIT definition");
    const auto source_bytes = payload(plan.jit);
    const auto deadline = std::chrono::steady_clock::now() + operation_timeout_;
    attach_process(process, deadline);
    AgentRequest stage;
    stage.action = AgentRequest::Action::stage;
    stage.limit = retention_limit ? retention_limit : live_.limit;
    const auto response = invoke(process, live_address_, stage, source_bytes);
    staged_token = response.token;
    live_.retained = response.retained;
    process.detach();
    if (!staged_token)
      throw std::runtime_error("live JIT service did not retain the native image");

    auto next = live_.revisions.at(expected_revision);
    next.parent = expected_revision;
    next.managed = 0;
    next.managed_native = false;
    next.safe_point = plan.safe_point;
    for (const auto &source : plan.accepted_sources)
      next.sources[source.path] = source;
    read_entries(next, plan.entries, response);

    const auto next_id = live_.last_committed + 1;
    if (!next_id)
      throw std::runtime_error("live revision identifiers exhausted");
    const auto [revision, inserted] = live_.revisions.emplace(next_id, std::move(next));
    if (!inserted)
      throw std::runtime_error("duplicate live revision");
    try {
      if (!publish(pid_, process_start_, process, live_.revisions.at(expected_revision),
                   revision->second, [&] {
                     if ((cancelled && cancelled()) || live_.active != expected_revision)
                       throw live::Stale("staged live operation was superseded or cancelled");
                     if (plan.validate_sources)
                       plan.validate_sources();
                   }, [&] {
                     live_.active = live_.last_committed = next_id;
                     live_.limit = stage.limit;
                     did_commit = true;
                   }))
        throw std::runtime_error("affected call chain or overwrite window is active; retry at a safe point");
    } catch (...) {
      if (!did_commit)
        live_.revisions.erase(revision);
      throw;
    }
    for (auto &candidate : *result.mutable_candidates())
      candidate.set_state(Status::APPLIED);
    result.set_state(Status::APPLIED);
  } catch (const std::exception &error) {
    const auto stale = dynamic_cast<const live::Stale *>(&error) != nullptr;
    result.set_diagnostic(error.what());
    if (staged_token && !did_commit && !process.unsafe()) {
      try {
        if (process.attached())
          process.detach();
        attach_process(process, std::chrono::steady_clock::now() + operation_timeout_);
        AgentRequest discard;
        discard.action = AgentRequest::Action::discard;
        discard.token = staged_token;
        live_.retained = invoke(process, live_address_, discard).retained;
        process.detach();
      } catch (const std::exception &discard_error) {
        result.set_diagnostic(result.diagnostic() + "; staged image cleanup: " + discard_error.what());
      }
    }
    if (process.unsafe()) {
      unsafe_target_ = true;
      set_failed_locked(result.diagnostic());
      result.set_state(Status::RESTORATION_REQUIRED);
    } else {
      result.set_state(did_commit ? Status::APPLIED : stale ? Status::STALE : Status::BLOCKED);
    }
    for (auto &candidate : *result.mutable_candidates())
      if (candidate.state() == Status::PLANNED)
        candidate.set_state(result.state());
  }
  report_revision(result, live_);
  return result;
}

clangtracer::v1::LiveUpdateStatus Session::revert_live_locked(std::uint64_t revision) {
  Status result;
  injector::Process process;
  bool did_commit = false;
  try {
    if (!live_.revisions.count(revision))
      throw std::runtime_error("unknown live revision");
    if (revision == live_.active) {
      result.set_state(Status::UNCHANGED);
    } else {
      const auto previous = live_.active;
      const auto &current = live_.revisions.at(previous);
      if (current.managed) {
        if (revision != current.parent)
          throw std::runtime_error("revert managed layout revisions in reverse application order");
        api::LiveAgentExtension input;
        input.set_action(api::LiveAgentExtension::MANAGED_REVERT);
        input.set_revision(current.managed);
        const auto &parent = live_.revisions.at(revision);
        input.set_publish_native(current.managed_native);
        result = run_live_extension_locked(std::move(input), [&](AgentRequest &phase) {
          if (phase.publication == AgentRequest::Publication::install)
            publish_managed(phase, pid_, process_start_, current, parent, {});
          else
            publish_managed(phase, pid_, process_start_, parent, current, {});
        });
        if (result.state() == Status::APPLIED) {
          live_.active = revision;
          result.set_state(Status::REVERTED);
          report_revision(result, live_);
        }
        return result;
      }
      for (auto cursor = previous; cursor && cursor != revision;
           cursor = live_.revisions.at(cursor).parent) {
        if (live_.revisions.at(cursor).managed)
          throw std::runtime_error("revert managed layout revisions in reverse application order");
      }
      if (live_.revisions.at(revision).managed && revision != current.parent)
        throw std::runtime_error("managed layout revisions cannot be reapplied by revision number");
      if (!publish(pid_, process_start_, process, live_.revisions.at(previous),
                   live_.revisions.at(revision), {}, [&] {
                     live_.active = revision;
                     did_commit = true;
                   }))
        throw std::runtime_error("affected call chain or overwrite window is active; retry at a safe point");
      result.set_state(Status::REVERTED);
    }
  } catch (const std::exception &error) {
    result.set_diagnostic(error.what());
    if (process.unsafe() || live_.pending_extension) {
      unsafe_target_ = true;
      set_failed_locked(result.diagnostic());
      result.set_state(Status::RESTORATION_REQUIRED);
    } else {
      result.set_state(did_commit ? Status::REVERTED : Status::BLOCKED);
    }
  }
  report_revision(result, live_);
  return result;
}
} // namespace tracer

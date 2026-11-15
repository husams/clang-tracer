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

std::size_t payload_limit(const Action &request) {
  const auto limit = request.has_payload_limit() ? request.payload_limit() : live::default_payload_bytes;
  if (limit == 0 || limit > live::max_input_bytes)
    throw std::invalid_argument("--payload-limit must be between 1 and " +
                                std::to_string(live::max_input_bytes) + " bytes");
  return static_cast<std::size_t>(limit);
}

api::LiveJitInput jit_wire(const live::JitRequest &request) {
  api::LiveJitInput wire;
  wire.set_resolve_standard_runtime(request.resolve_standard_runtime);
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
    output->set_reuse_stream_initializer(unit.reuse_stream_initializer);
    output->set_reuse_global_lifecycle(unit.reuse_global_lifecycle);
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
  return wire;
}

void check_payload_size(const google::protobuf::MessageLite &wire, std::size_t limit,
                        std::string_view description) {
  if (wire.ByteSizeLong() > limit)
    throw std::runtime_error(std::string(description) + " exceeds --payload-limit (" +
                             std::to_string(limit) + " bytes)");
}

AgentRequest invoke(injector::Process &process, std::uintptr_t entry, AgentRequest request,
                    const std::string &input = {}, std::string *output = nullptr) {
  if (!entry)
    throw std::runtime_error("attached agent has no live JIT service");
  const auto capacity = output ? request.output_capacity : 0;
  if (capacity > live::max_report_bytes)
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
             const std::function<void()> &committed, bool registry_gated = false,
             bool reconcile = false) {
  std::set<std::uintptr_t> addresses;
  for (const auto &[address, value] : from.entries) {
    (void)value;
    addresses.insert(address);
  }
  for (const auto &[address, value] : to.entries) {
    (void)value;
    addresses.insert(address);
  }
  std::vector<injector::CodePatch> patches;
  std::vector<injector::CodeRange> affected;
  bool requires_drain = false;
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
    requires_drain |= (old != from.entries.end() && old->second.requires_drain) ||
                      (next != to.entries.end() && next->second.requires_drain);
    affected.push_back({entry.address, entry.function_size, entry.return_at_end});
    if (old != from.entries.end())
      affected.push_back({old->second.mapping_begin, old->second.mapping_size, false, true});
  }
  if (registry_gated || !requires_drain)
    affected.clear();
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
  if (!process.patch_code_batch(patches, validate, committed, affected, reconcile))
    return false;
  process.detach();
  return true;
}
std::size_t staged_entry_count(const live::Plan &plan) {
  return plan.entries.size() + plan.has_native_mock_alternate;
}

live::JitEntryAddress read_jit_address(const AgentRequest &response, std::size_t index) {
  const auto address = response.addresses[index];
  const auto begin = response.mapping_begin[index];
  const auto size = response.mapping_size[index];
  if (!begin || !size || address < begin || address - begin >= size)
    throw std::runtime_error("agent returned an unverified JIT executable mapping");
  return {address, begin, size};
}

void read_entries(live::Revision &next, const live::Plan &plan, const AgentRequest &response) {
  const auto &entries = plan.entries;
  if (response.count != staged_entry_count(plan) || response.count > live::max_entries)
    throw std::runtime_error("live JIT service returned incomplete entry addresses");
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const auto &entry = entries[i];
    const auto output = read_jit_address(response, i);
    auto &slot = next.entries[entry.address];
    const bool requires_drain = slot.requires_drain || plan.safe_point == live::SafePoint::drained_stack;
    slot = {entry, jump_to(entry.original_bytes.size(), output.destination),
            output.destination, output.mapping_begin, output.mapping_size,
            requires_drain, plan.mock_original, {}};
  }
  if (plan.has_native_mock_alternate) {
    if (entries.size() != 1 || !plan.mock_original)
      throw std::runtime_error("invalid native mock alternate entry");
    next.entries.at(entries.front().address).native_mock_alternate = read_jit_address(response, 1);
  }
  for (const auto address : plan.restore_mock_originals) {
    auto &slot = next.entries.at(address);
    if (!slot.mock_original || !slot.native_mock_alternate)
      throw live::Stale("restoring a mock requires its retained native entry");
    const auto output = *slot.native_mock_alternate;
    slot.installed_bytes = jump_to(slot.binding.original_bytes.size(), output.destination);
    slot.destination = output.destination;
    slot.mapping_begin = output.mapping_begin;
    slot.mapping_size = output.mapping_size;
    slot.requires_drain |= plan.safe_point == live::SafePoint::drained_stack;
    slot.mock_original = slot.binding.original();
    slot.native_mock_alternate.reset();
  }
}

void publish_managed(AgentRequest &request, std::uint32_t pid, std::uint64_t start,
                     const live::Revision &before, const live::Revision &after,
                     const std::function<void()> &validate, bool reconcile = false) {
  using Outcome = AgentRequest::CodeOutcome;
  injector::Process process;
  bool written = false;
  try {
    if (!publish(pid, start, process, before, after, validate, [&] { written = true; },
                 true, reconcile))
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

std::optional<Status> Session::read_live_extension_report_locked(
    injector::Process &process, const AgentRequest &response) {
  if (!response.output_size)
    return std::nullopt;
  AgentRequest poll;
  poll.action = AgentRequest::Action::poll;
  poll.token = live_.pending_extension;
  poll.output_capacity = response.output_size;
  std::string serialized;
  const auto filled = invoke(process, live_address_, poll, {}, &serialized);
  if (!filled.ready)
    throw std::runtime_error("live agent response changed during retrieval");
  Status output;
  if (!output.ParseFromString(serialized))
    throw std::runtime_error("cannot decode live agent response");
  output.set_operation_token(live_.pending_extension);
  live_.pending_extension = 0;
  live_.retained = filled.retained;
  report_revision(output, live_);
  return output;
}

std::optional<Status> Session::poll_pending_live_data_locked() {
  if (!live_.pending_extension || live_.managed_transition)
    return std::nullopt;
  injector::Process process;
  try {
    attach_process(process, std::chrono::steady_clock::now() + operation_timeout_);
    AgentRequest poll;
    poll.action = AgentRequest::Action::poll;
    poll.token = live_.pending_extension;
    const auto response = invoke(process, live_address_, poll);
    if (response.publication != AgentRequest::Publication::none)
      throw std::runtime_error("unexpected code publication during a live data operation");
    auto status = read_live_extension_report_locked(process, response);
    process.detach();
    return status;
  } catch (...) {
    if (process.unsafe()) {
      unsafe_target_ = true;
      set_failed_locked("live data rendezvous could not safely restore the target");
    }
    throw;
  }
}

Status Session::run_live_extension_locked(api::LiveAgentExtension input,
    const std::function<void(AgentRequest &)> &publication, std::size_t input_limit) {
  injector::Process process;
  const auto deadline = std::chrono::steady_clock::now() + operation_timeout_;
  input.set_deadline_nanoseconds(std::chrono::duration_cast<std::chrono::nanoseconds>(
      deadline.time_since_epoch()).count());
  check_payload_size(input, input_limit, "live extension snapshot");
  const auto bytes = input.SerializeAsString();
  try {
    attach_process(process, deadline);
    AgentRequest start;
    start.action = AgentRequest::Action::launch;
    start.input_limit = input_limit;
    const auto launch = invoke(process, live_address_, start, bytes);
    if (!launch.token)
      throw std::runtime_error("agent did not start the live operation");
    live_.pending_extension = launch.token;
    process.detach();
  } catch (...) {
    if (process.unsafe()) {
      unsafe_target_ = true;
      set_failed_locked("live extension rendezvous could not safely restore the target");
    }
    throw;
  }
  return collect_live_extension_locked(deadline + std::chrono::seconds(live::extension_grace_seconds),
                                       publication);
}

Status Session::collect_live_extension_locked(std::chrono::steady_clock::time_point deadline,
    const std::function<void(AgentRequest &)> &publication) {
  injector::Process process;
  AgentRequest poll;
  poll.action = AgentRequest::Action::poll;
  poll.token = live_.pending_extension;
  try {
    for (;;) {
      attach_process(process, deadline);
      const auto response = invoke(process, live_address_, poll);
      auto *transition = live_.managed_transition ? &*live_.managed_transition : nullptr;
      auto *saved = transition ? &transition->acknowledgement : nullptr;
      if (response.publication != AgentRequest::Publication::none) {
        process.detach();
        if (!saved)
          throw std::runtime_error("unexpected managed code publication request");
        if (!*saved || (*saved)->publication != response.publication) {
          auto acknowledgement = response;
          acknowledgement.action = AgentRequest::Action::publication_result;
          try {
            publication(acknowledgement);
          } catch (const std::exception &error) {
            acknowledgement.code_outcome = AgentRequest::CodeOutcome::unchanged;
            std::snprintf(acknowledgement.error, sizeof(acknowledgement.error), "%s", error.what());
          }
          *saved = acknowledgement;
        }
        // Keep the acknowledgement until a later poll proves the phase
        // advanced. A failed rendezvous must never repeat a native write.
        attach_process(process, deadline);
        invoke(process, live_address_, **saved);
        process.detach();
      } else {
        if (auto status = read_live_extension_report_locked(process, response)) {
          if (saved)
            saved->reset();
          process.detach();
          return std::move(*status);
        }
        process.detach();
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
      }
      if (std::chrono::steady_clock::now() >= deadline)
        throw std::runtime_error("live operation is still running in the target");
    }
  } catch (...) {
    if (process.unsafe()) {
      unsafe_target_ = true;
      set_failed_locked("live extension rendezvous could not safely restore the target");
    }
    throw;
  }
}

Status Session::finish_managed_locked(Status result) {
  const auto &transition = *live_.managed_transition;
  const auto restore = transition.recovery_revision != 0;
  if (restore && result.state() != Status::APPLIED)
    result.set_state(Status::RESTORATION_REQUIRED);
  if (result.state() == Status::RESTORATION_REQUIRED) {
    if (!restore)
      live_.managed_transition->recovery_revision = transition.reverting
          ? live_.revisions.at(transition.before).managed : result.operation_token();
    result.set_diagnostic(result.diagnostic() + " (use %live recover)");
    report_revision(result, live_);
    return result;
  }
  if (restore) {
    result.set_state(Status::REVERTED);
    result.set_diagnostic("managed publication restored");
  } else if (result.state() == Status::APPLIED) {
    live_.active = transition.after;
    if (transition.reverting) {
      result.set_state(Status::REVERTED);
    } else {
      auto &next = live_.revisions.at(transition.after);
      next.managed = result.operation_token();
      live_.last_committed = transition.after;
      if (next.managed_native)
        live_.limit = transition.limit;
    }
  }
  if (!transition.reverting && live_.active != transition.after)
    live_.revisions.erase(transition.after);
  live_.managed_transition.reset();
  report_revision(result, live_);
  return result;
}

Status Session::recover_managed_locked() {
  if (!live_.managed_transition) {
    Status result;
    result.set_state(Status::UNCHANGED);
    result.set_diagnostic("no managed publication requires recovery");
    report_revision(result, live_);
    return result;
  }
  const auto &transition = *live_.managed_transition;
  const auto &before = live_.revisions.at(transition.before);
  const auto &after = live_.revisions.at(transition.after);
  const auto restore = [&](AgentRequest &phase) {
    if (phase.publication == AgentRequest::Publication::install) {
      // An interrupted request cannot start a new publication. The poll
      // loop will resend a saved acknowledgement if the write ran already.
      phase.code_outcome = AgentRequest::CodeOutcome::unchanged;
      std::snprintf(phase.error, sizeof(phase.error), "%s", "managed publication was interrupted");
      return;
    }
    publish_managed(phase, pid_, process_start_, after, before, {}, true);
  };
  if (live_.pending_extension) {
    const bool already_recovering = transition.recovery_revision != 0;
    auto result = finish_managed_locked(collect_live_extension_locked(
        std::chrono::steady_clock::now() + operation_timeout_, restore));
    if (already_recovering || !live_.managed_transition ||
        !live_.managed_transition->recovery_revision)
      return result;
  }
  api::LiveAgentExtension input;
  input.set_action(api::LiveAgentExtension::MANAGED_RECOVER);
  input.set_revision(live_.managed_transition->recovery_revision);
  return finish_managed_locked(run_live_extension_locked(std::move(input), restore));
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
  const auto input_limit = payload_limit(request);
  const auto limit = request.retention_limit() ? request.retention_limit() : live_.limit;
  if (managed)
    planned.input.set_retention_limit(limit);
  if (managed && request.action() == Action::PREVIEW) {
    const auto deadline = std::chrono::steady_clock::now() + operation_timeout_;
    planned.input.set_deadline_nanoseconds(std::chrono::duration_cast<std::chrono::nanoseconds>(
        deadline.time_since_epoch()).count());
    check_payload_size(planned.input, input_limit, "live extension snapshot");
    result.set_state(Status::PLANNED);
    result.set_diagnostic("managed layout, instances and hook will be validated in the target");
    return result;
  }
  if (!managed)
    return run_live_extension_locked(planned.input, {}, input_limit);

  const auto next_id = live_.last_committed + 1;
  if (!next_id)
    throw std::runtime_error("live revision identifiers exhausted");
  const auto [record, inserted] = live_.revisions.emplace(next_id, live_.revisions.at(previous));
  if (!inserted)
    throw std::runtime_error("duplicate managed live revision");
  record->second.parent = previous;
  record->second.managed = 0;
  auto &next = record->second;
  next.managed_native = planned.native.publishes_code();
  for (const auto &source : planned.native.accepted_sources)
    next.sources[source.path] = source;
  for (const auto &[unit, closures] : planned.native.private_closures)
    next.private_closures.insert_or_assign(unit, closures);
  live_.managed_transition.emplace(live::ManagedTransition{previous, next_id, limit, false});
  try {
    result = finish_managed_locked(run_live_extension_locked(planned.input, [&](AgentRequest &phase) {
      if (phase.publication == AgentRequest::Publication::install) {
        read_entries(next, planned.native, phase);
        publish_managed(phase, pid_, process_start_, before, next, [&] {
          if (cancelled && cancelled())
            throw live::Stale("managed update cancelled before publication");
          planned.validate();
        });
      } else {
        publish_managed(phase, pid_, process_start_, next, before, {});
      }
    }, input_limit));
  } catch (...) {
    if (!live_.pending_extension) {
      live_.revisions.erase(record);
      live_.managed_transition.reset();
    }
    throw;
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
    const bool recovering = request.action() == Action::RECOVER;
    if (!recovering && live_.managed_transition)
      throw std::runtime_error("managed publication requires %live recover");
    if (!recovering && live_.pending_extension) {
      if (request.action() == Action::STATUS)
        if (auto status = poll_pending_live_data_locked())
          return std::move(*status);
      result.set_operation_token(live_.pending_extension);
      throw std::runtime_error("a live data operation is running; use live status to collect its result");
    }
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
    if (recovering)
      return recover_managed_locked();
    if (request.action() == Action::REVERT)
      return revert_live_locked(request.has_exact_revision() ? request.revision() :
                                    live_.revisions.at(live_.active).parent);
    const auto expected_revision = live_.active;
    const auto input_limit = payload_limit(request);
    if (request.action() == Action::MOCK) {
      const auto plan = live::plan_live_mock(pid_, executable_, request,
                                             live_.revisions.at(expected_revision), cancelled);
      for (const auto &candidate : plan.candidates)
        result.add_candidates()->CopyFrom(candidate);
      return activate_live_locked(plan, expected_revision, request.retention_limit(),
                                  cancelled, std::move(result), input_limit);
    }
    if (request.has_preview_revision() && request.preview_revision() != expected_revision)
      throw live::Stale("active revision changed after preview; rediscover the update");
    if (request.action() == Action::SCAN_HEAP || request.action() == Action::UPDATE_DATA ||
        ((request.action() == Action::UPDATE || request.action() == Action::PREVIEW) &&
         !request.options().empty()))
      return apply_live_extension_locked(request, std::move(result), cancelled);
    if (request.action() != Action::UPDATE && request.action() != Action::PREVIEW)
      throw std::runtime_error("unknown live update action");
    auto preview_key = request;
    preview_key.set_action(Action::PREVIEW);
    preview_key.clear_preview_revision();
    auto key = preview_key.SerializeAsString();
    std::shared_ptr<live::Plan> saved;
    if (request.action() == Action::UPDATE && request.has_preview_revision() &&
        live_preview_revision_ == expected_revision && live_preview_process_start_ == process_start_ &&
        live_preview_key_ == key)
      saved = std::move(live_preview_);
    live_preview_.reset();
    live_preview_key_.clear();
    if (!saved)
      saved = std::make_shared<live::Plan>(live::plan_sources(
          pid_, executable_, request, live_.revisions.at(expected_revision), cancelled));
    auto &plan = *saved;
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
    if (!plan.publishes_code()) {
      result.set_state(Status::UNCHANGED);
      result.set_diagnostic("no changed native application entry");
      return result;
    }
    if (request.action() == Action::PREVIEW) {
      check_payload_size(jit_wire(plan.jit), input_limit, "live JIT snapshot");
      live_preview_ = std::move(saved);
      live_preview_key_ = std::move(key);
      live_preview_revision_ = expected_revision;
      live_preview_process_start_ = process_start_;
      result.set_state(Status::PLANNED);
      return result;
    }
    return activate_live_locked(plan, expected_revision, request.retention_limit(),
                                cancelled, std::move(result), input_limit);
  } catch (const std::exception &error) {
    result.set_state(unsafe_target_ || live_.managed_transition ? Status::RESTORATION_REQUIRED :
                     dynamic_cast<const live::Stale *>(&error) ? Status::STALE : Status::BLOCKED);
    result.set_diagnostic(error.what());
  }
  return result;
}

clangtracer::v1::LiveUpdateStatus Session::activate_live_locked(
    const live::Plan &plan, std::uint64_t expected_revision, std::uint64_t retention_limit,
    const std::function<bool()> &cancelled, Status result, std::size_t input_limit) {
  injector::Process process;
  std::uint64_t staged_token = 0;
  std::uint64_t stage_operation = 0;
  bool did_commit = false;
  try {
    if (staged_entry_count(plan) > live::max_entries ||
        plan.entries.size() + plan.restore_mock_originals.size() > live::max_entries)
      throw std::runtime_error("submitted group exceeds the live JIT entry budget");

    if (plan.validate_sources)
      plan.validate_sources();
    const auto limit = retention_limit ? retention_limit : live_.limit;
    AgentRequest response;
    if (!plan.entries.empty()) {
      if (plan.jit.entry_symbols.size() != staged_entry_count(plan))
        throw std::runtime_error("each native entry needs one exact JIT definition");
      const auto wire = jit_wire(plan.jit);
      check_payload_size(wire, input_limit, "live JIT snapshot");
      const auto source_bytes = wire.SerializeAsString();
      const auto deadline = std::chrono::steady_clock::now() + operation_timeout_;
      attach_process(process, deadline);
      AgentRequest stage;
      stage.action = AgentRequest::Action::stage;
      stage.limit = limit;
      stage.input_limit = input_limit;
      stage_operation = invoke(process, live_address_, stage, source_bytes).token;
      process.detach();
      if (!stage_operation)
        throw std::runtime_error("agent did not launch the native live compile");

      AgentRequest poll;
      poll.action = AgentRequest::Action::stage_poll;
      poll.token = stage_operation;
      for (;;) {
        if (cancelled && cancelled())
          throw live::Stale("native live compile cancelled");
        if (std::chrono::steady_clock::now() >= deadline)
          throw std::runtime_error("native live compile exceeded the operation timeout");
        attach_process(process, deadline);
        response = invoke(process, live_address_, poll);
        if (response.ready) {
          stage_operation = 0;
          staged_token = response.token;
          live_.retained = response.retained;
        }
        process.detach();
        if (response.ready)
          break;
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
      }
      if (!staged_token)
        throw std::runtime_error("live JIT service did not retain the native image");
    }

    auto next = live_.revisions.at(expected_revision);
    next.parent = expected_revision;
    next.managed = 0;
    next.managed_native = false;
    for (const auto &source : plan.accepted_sources)
      next.sources[source.path] = source;
    for (const auto &[unit, closures] : plan.private_closures)
      next.private_closures.insert_or_assign(unit, closures);
    read_entries(next, plan, response);

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
                     live_.limit = limit;
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
    if ((stage_operation || staged_token) && !did_commit && !process.unsafe()) {
      try {
        if (process.attached())
          process.detach();
        attach_process(process, std::chrono::steady_clock::now() + operation_timeout_);
        AgentRequest discard;
        discard.action = stage_operation ? AgentRequest::Action::stage_abandon
                                         : AgentRequest::Action::discard;
        discard.token = stage_operation ? stage_operation : staged_token;
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
        live_.managed_transition.emplace(live::ManagedTransition{previous, revision, live_.limit, true});
        return finish_managed_locked(run_live_extension_locked(std::move(input), [&](AgentRequest &phase) {
          if (phase.publication == AgentRequest::Publication::install)
            publish_managed(phase, pid_, process_start_, current, parent, {});
          else
            publish_managed(phase, pid_, process_start_, parent, current, {});
        }));
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
    if (process.unsafe()) {
      unsafe_target_ = true;
      set_failed_locked(result.diagnostic());
    }
    if (live_.managed_transition && !live_.pending_extension &&
        !live_.managed_transition->recovery_revision)
      live_.managed_transition.reset();
    result.set_state(unsafe_target_ || live_.managed_transition ? Status::RESTORATION_REQUIRED :
                     did_commit ? Status::REVERTED : Status::BLOCKED);
  }
  report_revision(result, live_);
  return result;
}
} // namespace tracer

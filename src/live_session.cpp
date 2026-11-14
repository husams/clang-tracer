#include "session.hpp"

#include "injector.hpp"
#include "live_agent_protocol.hpp"
#include "live_plan.hpp"
#include "native_detour.hpp"
#include "proc_identity.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <memory>
#include <set>

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
  for (const auto &[name, address] : request.imports)
    (*wire.mutable_imports())[name] = address;
  for (const auto &name : request.entry_symbols)
    wire.add_entry_symbols(name);
  if (wire.ByteSizeLong() > live::max_payload_bytes)
    throw std::runtime_error("live JIT snapshot exceeds the transport budget");
  return wire.SerializeAsString();
}

AgentRequest invoke(injector::Process &process, std::uintptr_t entry, AgentRequest request,
                    const std::string &input = {}) {
  if (!entry)
    throw std::runtime_error("attached agent has no live JIT service");
  const auto size = sizeof(request) + input.size();
  const auto remote = process.remote_allocate(size);
  try {
    if (!input.empty()) {
      request.input = remote + sizeof(request);
      request.input_size = input.size();
      process.remote_write(request.input, input.data(), input.size());
    }
    process.remote_write(remote, &request, sizeof(request));
    const auto returned = process.remote_call(entry, {remote}, 1);
    process.remote_read(remote, &request, sizeof(request));
    if (request.version != live::agent_magic || !std::memchr(request.error, 0, sizeof(request.error)))
      throw std::runtime_error("invalid response from the live JIT service");
    if (returned)
      throw std::runtime_error(request.error[0] ? request.error : "live JIT service failed");
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
             const std::function<void()> &committed) {
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
    if (from.safe_point == live::SafePoint::drained_stack || to.safe_point == live::SafePoint::drained_stack) {
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
} // namespace

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
    if (request.action() != Action::UPDATE && request.action() != Action::PREVIEW)
      throw std::runtime_error("this server has no handler for the requested live extension");
    if (!request.options().empty())
      throw std::runtime_error("managed migration options require the live migration handler");
    const auto expected_revision = live_.active;
    if (request.has_preview_revision() && request.preview_revision() != expected_revision)
      throw live::Stale("active revision changed after preview; rediscover the update");
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
    result.set_state(dynamic_cast<const live::Stale *>(&error) ? Status::STALE : Status::BLOCKED);
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
    if (!staged_token || response.count != plan.entries.size())
      throw std::runtime_error("live JIT service returned incomplete entry addresses");

    auto next = live_.revisions.at(expected_revision);
    next.parent = expected_revision;
    next.safe_point = plan.safe_point;
    for (const auto &source : plan.accepted_sources)
      next.sources[source.path] = source;
    for (std::size_t i = 0; i < plan.entries.size(); ++i) {
      const auto &entry = plan.entries[i];
      if (!response.mapping_begin[i] || !response.mapping_size[i] ||
          response.addresses[i] < response.mapping_begin[i] ||
          response.addresses[i] - response.mapping_begin[i] >= response.mapping_size[i])
        throw std::runtime_error("agent returned an unverified JIT executable mapping");
      next.entries[entry.address] = {
          entry, jump_to(entry.original_bytes.size(), response.addresses[i]),
          response.addresses[i], response.mapping_begin[i], response.mapping_size[i]};
    }

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
      result.set_state(Status::RESTORATION_REQUIRED);
    } else {
      result.set_state(did_commit ? Status::REVERTED : Status::BLOCKED);
    }
  }
  report_revision(result, live_);
  return result;
}
} // namespace tracer

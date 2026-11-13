#include "arm_object_watch.hpp"

#include "debug_resolver.hpp"
#include "injector.hpp"
#include "object_code_cache.hpp"
#include "object_snapshot.hpp"
#include "object_watch_plan.hpp"
#include "prepared_contract.hpp"
#include "proc_identity.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <set>
#include <stdexcept>
#include <thread>

#include <signal.h>

namespace tracer::arm_object_watch {
namespace api = clangtracer::v1;
namespace {
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
}

api::ObjectWatchStatus preview_snapshots(const api::ObjectWatchRequest &request, std::uint32_t pid,
                                        const std::string &executable) {
  using Request = api::ObjectWatchRequest;
  if (request.methods().empty() || request.methods_size() > 16 || request.concrete_type().empty() ||
      request.no_values() || request.stack_depth() > 64 || request.snapshot_fields_size() > 128 ||
      request.snapshot_keys_size() > 128 || request.snapshot_max_depth() > 32 ||
      request.snapshot_max_bytes() > 4096 || request.max_instances() > 65536 ||
      request.max_elements() > 65536 || request.max_string_bytes() > 65536 ||
      request.writer_tid() || request.writer_entry() || request.first_seen() || request.follow_calls() ||
      request.runtime_limit() || request.event_buffer() || request.syscall_output() ||
      request.cfi_stack() || request.serialize_wait_us() || !request.shared_file().empty() ||
      request.backend() != Request::PTRACE || request.overflow_policy() != Request::DROP_NEW ||
      !request.application_rendezvous().empty() ||
      !request.base_type().empty() || !request.owner_type().empty() || !request.code_symbol().empty() ||
      !request.collection().empty() || !request.vector_field().empty() || !request.map_field().empty() ||
      !request.pointee_field().empty() || !request.async_signals().empty() ||
      request.snapshot_module().size() > 4096 || request.snapshot_module().find('\0') != std::string::npos ||
      (pid && !request.has_prepared_bindings()) ||
      (request.synchronization() != Request::THREAD_CONFINED &&
       request.synchronization() != Request::CALLER_LOCK &&
       request.synchronization() != Request::STABLE_AT_BOUNDARIES))
    throw std::invalid_argument("method snapshots require prepared methods and declared synchronization");
  std::set<std::string> fields;
  for (const auto &field : request.snapshot_fields())
    if (field.empty() || field.size() > 512 || field.find('\0') != std::string::npos || !fields.insert(field).second)
      throw std::invalid_argument("snapshot fields must be nonempty and unique");
  for (const auto &key : request.snapshot_keys())
    if (key.size() > 512 || key.find('\0') != std::string::npos)
      throw std::invalid_argument("invalid snapshot identity rule");
  tracer_repl::validate_snapshot_keys({request.snapshot_keys().begin(), request.snapshot_keys().end()});
  api::ObjectWatchStatus result;
  result.set_mode(Request::SNAPSHOTS);
  auto &plan = *result.mutable_plan();
  plan.set_field_encoding("typed receiver");
  const auto modules = pid ? client::list_debug_modules(pid) : std::vector<std::string>{};
  std::set<std::uint64_t> entries;
  for (const auto &method : request.methods()) {
    auto receiver = std::string_view(method.header_types().record_name());
    if (receiver.starts_with("::"))
      receiver.remove_prefix(2);
    auto selected = std::string_view(request.concrete_type());
    if (selected.starts_with("::"))
      selected.remove_prefix(2);
    if (!prepared::is_typed_version(method.format_version()) || method.ByteSizeLong() > 65536 ||
        method.module_sha256().size() != 32 || method.signature().kind() != api::METHOD ||
        receiver != selected || method.header_types().receiver_volatile())
      throw std::invalid_argument("method snapshot requires a pinned typed receiver adapter");
    std::string module = executable;
    if (pid) {
      const auto &wanted = request.snapshot_module().empty() ? method.module_name() : request.snapshot_module();
      std::vector<std::string> candidates;
      for (const auto &path : modules)
        if (path == wanted || std::filesystem::path(path).filename() == wanted)
          candidates.push_back(path);
      if (candidates.size() != 1)
        throw std::runtime_error("snapshot method requires one matching mapped ELF");
      module = std::move(candidates.front());
    }
    auto boundary = object_watch::Plan::method_boundary(module, method, nullptr, pid != 0, pid);
    if (!entries.insert(boundary.binding.elf_pc()).second)
      throw std::invalid_argument("duplicate or aliased method entry in snapshot plan");
    if (plan.binary_sha256().empty()) {
      plan.set_binary_sha256(boundary.origin.module_sha256());
      plan.set_writer_build_id(boundary.binding.module_build_id());
      plan.set_executable_build_id(boundary.binding.module_build_id());
    } else if (plan.binary_sha256() != boundary.origin.module_sha256()) {
      throw std::invalid_argument("snapshot methods must belong to the same ELF");
    }
    *plan.add_bindings() = std::move(boundary.binding);
  }
  result.set_binding_status(pid ? object_watch::compare_prepared(request.prepared_bindings(), plan, false) :
                                  "offline");
  result.set_diagnostic("verified AArch64 method boundaries and typed receiver layout");
  return result;
}

void validate(api::ObjectWatchRequest &request) {
  using Request = api::ObjectWatchRequest;
  const bool cache = request.backend() == Request::CODE_CACHE;
  if (request.mode() != Request::WRITES ||
      (!cache && request.backend() != Request::PTRACE) ||
      request.writer_entry() || !request.borrow_caller().empty() ||
      ((request.follow_calls() || request.runtime_limit()) && !cache) ||
      request.concrete_type().empty() || request.field().empty() || request.code_symbol().empty() ||
      request.stack_depth() > 64 || request.max_elements() > 65536 ||
      request.max_string_bytes() > 65536 || request.max_instances() > 65536)
    throw std::invalid_argument("AArch64 capture requires a verified writer, field and valid limits");
  for (const auto *value : {&request.concrete_type(), &request.base_type(), &request.owner_type(),
                           &request.field(), &request.collection(), &request.snapshot_field(),
                           &request.code_symbol(), &request.code_module(),
                           &request.vector_field(), &request.element_key_field(),
                           &request.mutation_symbol(), &request.map_field(), &request.map_key(),
                           &request.pointee_field(),
                           &request.union_tag(), &request.application_rendezvous()})
    if (value->size() > 512 || value->find('\0') != std::string::npos)
      throw std::invalid_argument("invalid AArch64 object watch selector");
  if (!cache && !request.application_rendezvous().empty())
    throw std::invalid_argument("an application rendezvous requires code-cache capture");
  std::set<unsigned> signals;
  for (auto sig : request.async_signals())
    if (!cache || sig < 1 || sig > 64 || sig == SIGKILL || sig == SIGSTOP || sig == SIGSEGV ||
        sig == SIGBUS || !signals.insert(sig).second)
      throw std::invalid_argument("invalid or duplicate AArch64 asynchronous code-cache signal");
  const bool tagged = !request.union_tag().empty();
  const bool ownership = !request.owner_type().empty();
  const bool vector = !request.vector_field().empty();
  const bool map = !request.map_field().empty();
  const bool pointee = !request.pointee_field().empty();
  const bool logical_range = vector || map || pointee;
  const bool synchronized = request.synchronization() == Request::THREAD_CONFINED ||
                            request.synchronization() == Request::CALLER_LOCK;
  if (tagged != !request.union_cases().empty() || request.union_cases_size() > 128 ||
      (tagged && (!synchronized || ownership || logical_range)))
    throw std::invalid_argument("tagged union capture requires declared synchronization and tag cases");
  if (ownership != !request.base_type().empty() || ownership != !request.collection().empty() ||
      (ownership && request.first_seen()) ||
      (cache && (!request.snapshot_field().empty() || (ownership && !synchronized))) ||
      (!cache && ownership != !request.snapshot_field().empty()))
    throw std::invalid_argument("ownership capture requires a verified base, owner and collection");
  if (vector != !request.element_key_field().empty() || vector != request.has_element_key() ||
      logical_range != !request.mutation_symbol().empty() ||
      (logical_range && (!request.first_seen() || (cache && !synchronized))) ||
      (map && vector) || (pointee && (map || vector)) || (!map && !request.map_key().empty()))
    throw std::invalid_argument("collection capture requires a verified mutation and a supported key");
  if (request.event_buffer() &&
      (!cache || request.event_buffer() < 2 || request.event_buffer() > 65536))
    throw std::invalid_argument("--event-buffer requires code-cache and 2 to 65536 slots");
  if (request.overflow_policy() != Request::DROP_NEW &&
      (!cache || request.overflow_policy() != Request::STOP_CAPTURE))
    throw std::invalid_argument("invalid AArch64 code-cache overflow policy");
  if (request.cfi_stack() && (!cache || !request.stack_depth()))
    throw std::invalid_argument("CFI stack capture requires code-cache and a positive stack depth");
  if (!request.max_elements())
    request.set_max_elements(4096);
  if (!request.max_string_bytes())
    request.set_max_string_bytes(4096);
}

bool same_process(std::uint32_t pid, std::uint64_t start, const std::string &executable) {
  try {
    const auto directory = proc::directory(pid);
    return proc::start_time(directory / "stat") == start &&
        std::filesystem::read_symlink(directory / "exe") == executable;
  } catch (const std::exception &) {
    return false;
  }
}

struct NativeWatch::Impl {
  std::uint32_t pid;
  std::uint64_t process_start;
  std::string executable;
  std::chrono::seconds timeout;
  object_watch::Plan plan;
  object_cache::Watch watch;
  bool unsafe = false;

  Impl(const api::ObjectWatchRequest &request, std::uint64_t id,
       std::uint32_t target, std::uint64_t start, const std::string &binary,
       const std::string &event_path, std::chrono::seconds operation_timeout,
       object_watch::Watch::Publish publish)
      : pid(target), process_start(start), executable(binary), timeout(operation_timeout),
        plan(pid, binary, request), watch(request, id, pid, binary, event_path, std::move(publish)) {}

  bool alive() const { return same_process(pid, process_start, executable); }
};

NativeWatch::NativeWatch(const api::ObjectWatchRequest &request, std::uint64_t id,
    std::uint32_t pid, std::uint64_t start, const std::string &executable,
    const std::string &event_path, std::chrono::seconds timeout,
    object_watch::Watch::Publish publish)
    : impl_(std::make_unique<Impl>(request, id, pid, start, executable, event_path,
                                  timeout, std::move(publish))) {}

NativeWatch::~NativeWatch() = default;

void NativeWatch::start(const std::string &agent, const std::string &rendezvous) {
  auto &p = *impl_;
  const auto canonical = std::filesystem::canonical(agent).string();
  const auto entry = rendezvous.empty() ? std::nullopt :
      std::optional(p.plan.application_range(p.pid, rendezvous));
  const auto deadline = Clock::now() + p.timeout;
  injector::Process process;
  try {
    for (;;) {
      if (!p.alive())
        throw std::runtime_error("target exited before AArch64 code-cache installation");
      try {
        if (entry)
          process.attach_at_application_point(p.pid, entry->first, entry->second);
        else
          process.attach(p.pid);
        break;
      } catch (const injector::IdlePointUnavailable &) {
        if (Clock::now() >= deadline)
          throw;
        std::this_thread::sleep_for(1ms);
      }
    }
    if (!p.alive())
      throw std::runtime_error("target identity changed before AArch64 agent loading");
    if (entry) {
      const auto [pc, size] = *entry;
      const auto original = p.plan.code(pc).substr(0, size);
      if (original.size() != size)
        throw std::runtime_error("application rendezvous exceeds its verified ELF text");
      std::array<char, 4096> live{};
      for (std::size_t offset = 0; offset < size;) {
        const auto width = std::min(live.size(), size - offset);
        process.remote_read(pc + offset, live.data(), width);
        if (std::string_view(live.data(), width) != original.substr(offset, width))
          throw std::runtime_error("application rendezvous bytes disagree with the verified ELF");
        offset += width;
      }
    }
    p.watch.install(process, object_cache::load_agent(process, canonical));
    process.detach();
  } catch (...) {
    p.unsafe = process.unsafe();
    throw;
  }
}

api::ObjectWatchStatus NativeWatch::status() const {
  auto &p = *impl_;
  if (p.alive())
    p.watch.retire_if_mapping_changed(Clock::now() + p.timeout);
  return p.watch.status();
}

api::ObjectWatchStatus NativeWatch::stop(api::ObjectObservation::ClosureReason reason) {
  auto &p = *impl_;
  const auto initial = status();
  if (!initial.active() && !initial.restoration_pending()) {
    if (p.unsafe && p.alive())
      throw std::runtime_error("target state is uncertain after AArch64 injection");
    return initial;
  }
  const auto deadline = Clock::now() + p.timeout;
  p.watch.stop_runtime_admissions();
  injector::Process process;
  try {
    while (initial.active() && p.alive()) {
      if (p.unsafe)
        throw std::runtime_error("target state is uncertain after AArch64 injection");
      process.attach_for_patch(p.pid);
      if (!p.alive())
        break;
      if (p.watch.retire_if_mapping_changed(deadline)) {
        process.detach();
        return p.watch.status();
      }
      const auto restored = p.watch.restore(process);
      process.detach();
      if (restored)
        break;
      if (Clock::now() >= deadline)
        throw std::runtime_error("timed out restoring AArch64 code-cache redirects");
      std::this_thread::sleep_for(1ms);
    }
  } catch (...) {
    p.unsafe |= process.unsafe();
    if (p.alive())
      throw;
  }
  p.watch.finish(deadline, p.alive() ? reason : api::ObjectObservation::TARGET_EXITED);
  return p.watch.status();
}
} // namespace tracer::arm_object_watch

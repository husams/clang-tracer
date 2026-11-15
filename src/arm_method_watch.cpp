#include "arm_method_watch.hpp"

#include "arm_object_watch.hpp"
#include "arm_snapshot_protocol.hpp"
#include "injector.hpp"
#include "linux_syscalls.hpp"
#include "native_entry.hpp"
#include "object_snapshot.hpp"
#include "object_watch_plan.hpp"
#include "tracer/protocol.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <new>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <thread>

#include <dlfcn.h>
#include <fcntl.h>
#include <linux/futex.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace tracer::arm_object_watch {
namespace api = clangtracer::v1;
namespace fs = std::filesystem;
using namespace object_cache;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

namespace {
struct MappedFile {
  std::string path;
  std::size_t size;
  void *data = nullptr;
  struct stat identity{};
  MappedFile(const std::string &name, std::size_t width) : path(name + "-XXXXXX"), size(width) {
    const int fd = ::mkstemp(path.data());
    if (fd < 0)
      throw std::runtime_error("cannot create private method snapshot channel");
    data = ::fstat(fd, &identity) || ::ftruncate(fd, size) ? MAP_FAILED :
        ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);
    if (data == MAP_FAILED) {
      ::unlink(path.c_str());
      throw std::runtime_error("cannot map private method snapshot channel");
    }
  }
  ~MappedFile() {
    ::munmap(data, size);
    ::unlink(path.c_str());
  }
};

struct Redirect {
  Instruction instruction;
  std::uint32_t original = 0, replacement = 0;
  Address gateway = 0;
};

}

struct MethodWatch::Impl {
  api::ObjectWatchRequest request;
  api::Session session;
  api::ObjectWatchStatus report;
  std::string agent;
  std::chrono::seconds timeout;
  object_watch::Watch::Publish publish;
  MappedFile control, payload;
  Channel &channel;
  arm_snapshot::Queue &queue;
  std::vector<Redirect> redirects;
  std::map<Address, api::ObjectObservation> origins;
  std::map<Address, Address> method_probes;
  std::vector<std::string> keys, fields;
  mutable std::mutex mutex;
  std::jthread reader;
  std::uint64_t cursor = 0, next_call = 1;
  std::uint64_t seen_drops = 0, drop_barrier = 0;
  Clock::time_point deadline = Clock::time_point::max();
  bool installed = false, stopped = false;
  bool pending_restore = false, draining = false;
  struct Pending {
    std::uint64_t id;
    Address receiver;
    tracer_repl::Snapshot snapshot;
    api::StackTrace stack;
  };
  std::map<std::tuple<unsigned, Address, Address>, Pending> pending;
  struct Receiver { unsigned active = 0, depth = 0; bool violated = false; };
  std::map<Address, Receiver> receivers;

  bool release_receiver(Address address) {
    const auto receiver = receivers.find(address);
    const bool violated = receiver->second.violated;
    if (!--receiver->second.depth)
      receivers.erase(receiver);
    report.set_admitted_receivers(receivers.size());
    return violated;
  }

  Impl(const api::ObjectWatchRequest &spec, std::uint64_t id, const api::Session &target,
       const api::ObjectWatchStatus &preview, const std::string &directory, const std::string &library,
       std::chrono::seconds operation_timeout, object_watch::Watch::Publish sink)
      : request(spec), session(target), report(preview), agent(fs::canonical(library).string()),
        timeout(operation_timeout), publish(std::move(sink)),
        control(directory + "/method-control", Channel::mapping_size(2, 0)),
        payload(directory + "/method-payload", sizeof(arm_snapshot::Queue)),
        channel(*new (control.data) Channel), queue(*new (payload.data) arm_snapshot::Queue),
        keys(spec.snapshot_keys().begin(), spec.snapshot_keys().end()),
        fields(spec.snapshot_fields().begin(), spec.snapshot_fields().end()) {
    for (unsigned index = 0; index < arm_snapshot::slots; ++index)
      queue.records[index].turn.store(index, std::memory_order_relaxed);
    report.set_watch_id(id);
    report.set_diagnostic("prepared AArch64 receiver samples at verified method entry and return");
  }

  bool alive() const {
    return same_process(session.pid(), session.process_start_time(), session.executable());
  }

  bool has_control_mapping() const {
    std::ifstream maps("/proc/" + std::to_string(session.pid()) + "/maps");
    for (std::string line; std::getline(maps, line);) {
      unsigned long long inode = 0;
      unsigned device_major = 0, device_minor = 0;
      if (std::sscanf(line.c_str(), "%*x-%*x %*s %*x %x:%x %llu", &device_major, &device_minor, &inode) == 3 &&
          inode == control.identity.st_ino && makedev(device_major, device_minor) == control.identity.st_dev)
        return true;
    }
    return false;
  }

  bool mapping_replaced() const {
    std::ifstream maps("/proc/" + std::to_string(session.pid()) + "/maps");
    if (!maps)
      return false;
    std::vector<std::pair<Address, Address>> ranges;
    for (std::string line; std::getline(maps, line);) {
      unsigned long long begin = 0, end = 0, offset = 0, inode = 0;
      unsigned device_major = 0, device_minor = 0;
      char permissions[5]{};
      if (std::sscanf(line.c_str(), "%llx-%llx %4s %llx %x:%x %llu", &begin, &end, permissions,
                      &offset, &device_major, &device_minor, &inode) != 7)
        return false;
      if (inode == control.identity.st_ino && makedev(device_major, device_minor) == control.identity.st_dev)
        return false;
      if (std::ranges::any_of(redirects, [&](const auto &patch) {
            return patch.gateway >= begin && patch.gateway < end;
          }))
        return false;
      ranges.emplace_back(begin, end);
    }
    if (ranges.empty() || maps.bad())
      return false;
    for (const auto &patch : redirects) {
      std::uint32_t code = 0;
      iovec local{&code, sizeof(code)};
      iovec remote{reinterpret_cast<void *>(patch.instruction.pc), sizeof(code)};
      const auto length = linux_api::process_vm_readv(session.pid(), &local, 1, &remote, 1, 0);
      if (length == sizeof(code) ? code == patch.replacement :
          length != -1 || errno != EFAULT || std::ranges::any_of(ranges, [&](const auto &range) {
            return patch.instruction.pc >= range.first && patch.instruction.pc < range.second;
          }))
        return false;
    }
    return true;
  }

  void emit(unsigned tid, std::uint64_t time, std::uint64_t id, Address method, Address receiver,
            const tracer_repl::SnapshotChange &change, const api::StackTrace *captured = nullptr,
            api::ObjectObservation::ClosureReason reason = api::ObjectObservation::CLOSURE_UNSPECIFIED) {
    if (Clock::now() >= deadline || (request.max_events() && report.observations() >= request.max_events()))
      return;
    api::TraceEvent event;
    event.set_protocol_version(kProtocolVersion);
    event.set_session_id(session.id());
    event.set_probe_id(report.watch_id());
    event.set_probe_sequence(report.observations() + 1);
    event.set_agent_dropped_events(seen_drops);
    event.set_pid(session.pid());
    event.set_tid(tid);
    event.set_call_id(id);
    event.set_object_address(receiver);
    event.set_monotonic_time_ns(time ? time : std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now().time_since_epoch()).count());
    event.set_function_name("object.trace");
    auto &observation = *event.mutable_object_observation();
    if (method) {
      observation = origins.at(method);
      observation.set_boundary_probe_id(method_probes.at(method));
    } else {
      observation.set_source_unavailable(true);
    }
    observation.set_kind(change.gap ? api::ObjectObservation::COVERAGE_GAP : api::ObjectObservation::SNAPSHOT_DELTA);
    observation.set_concrete_type(request.concrete_type());
    observation.set_field(change.path);
    observation.set_diagnostic(change.detail);
    observation.set_closure_reason(reason);
    observation.set_synchronization(request.synchronization());
    if (change.before)
      tracer_repl::encode_observation_value(*change.before, *observation.mutable_snapshot_before());
    if (change.after)
      tracer_repl::encode_observation_value(*change.after, *observation.mutable_snapshot_after());
    if (captured && captured->frames_size())
      *event.mutable_stack() = *captured;
    report.set_observations(report.observations() + 1);
    report.set_coverage_gaps(report.coverage_gaps() + change.gap);
    if (request.max_events() && report.observations() == request.max_events())
      channel.capture_limit.store(Channel::event_limit, std::memory_order_relaxed);
    publish(std::move(event));
  }

  void observe(const arm_snapshot::Sample &sample) {
    const auto key = std::tuple{sample.tid, sample.method, sample.stack};
    tracer_repl::Snapshot decoded;
    const auto id = next_call;
    const auto gap = [&](std::string diagnostic) {
      emit(sample.tid, sample.time, id, sample.method, sample.receiver, {"receiver", std::move(diagnostic), true});
    };
    if (sample.entry) {
      ++next_call;
      for (auto it = pending.begin(); it != pending.end();) {
        const auto &[tid, method, stack] = it->first;
        if (tid != sample.tid || stack < sample.stack) {
          ++it;
          continue;
        }
        emit(tid, sample.time, it->second.id, method, it->second.receiver,
             {"receiver", "method return unavailable before next invocation", true}, &it->second.stack);
        release_receiver(it->second.receiver);
        it = pending.erase(it);
      }
      if (!sample.receiver || pending.size() >= 1024) {
        gap("receiver unavailable or pending method sample limit reached");
        return;
      }
      auto receiver = receivers.find(sample.receiver);
      if (receiver == receivers.end()) {
        const auto maximum = request.max_instances() ? request.max_instances() : 1024;
        if (receivers.size() >= maximum) {
          gap("receiver admission limit reached");
          return;
        }
        receiver = receivers.try_emplace(sample.receiver).first;
      }
      auto &state = receiver->second;
      state.violated |= state.active && state.active != sample.tid;
      if (!state.active)
        state.active = sample.tid;
      ++state.depth;
      if (sample.size && (sample.size > sample.bytes.size() || !decoded.ParseFromArray(sample.bytes.data(), sample.size)))
        decoded.Clear();
      api::StackTrace stack;
      if (sample.depth) {
        stack.set_requested_depth(request.stack_depth());
        stack.set_status(static_cast<api::StackTrace::Status>(sample.stack_status));
        stack.set_detail("native CFI from original application method entry");
        for (unsigned i = 0; i < std::min<unsigned>(sample.depth, stack_limit); ++i) {
          auto &frame = *stack.add_frames();
          frame.set_pc(sample.frames[i]);
          frame.set_stack_pointer(sample.stack_pointers[i]);
          if (!i) {
            const auto &origin = origins.at(sample.method);
            frame.set_kind(api::StackFrame::TARGET_ENTRY);
            frame.set_application_frame(true);
            frame.set_module(origin.module());
            frame.set_module_offset(origin.elf_pc());
            frame.set_function(origin.function());
            frame.set_file(origin.file());
            frame.set_line(origin.line());
          }
        }
        stack.set_captured_depth(stack.frames_size());
      }
      pending.emplace(key, Pending{id, sample.receiver, std::move(decoded), std::move(stack)});
      report.set_admitted_receivers(receivers.size());
      return;
    }
    const auto found = pending.find(key);
    if (found == pending.end()) {
      gap("matching method entry unavailable");
      return;
    }
    auto previous = std::move(found->second);
    pending.erase(found);
    if (release_receiver(previous.receiver) || sample.receiver != previous.receiver || !sample.size || sample.size > sample.bytes.size() ||
        !decoded.ParseFromArray(sample.bytes.data(), sample.size) || !previous.snapshot.status()) {
      emit(sample.tid, sample.time, previous.id, sample.method, previous.receiver,
           {"receiver", "matching synchronized receiver samples unavailable", true}, &previous.stack);
      return;
    }
    for (const auto &change : tracer_repl::compare_snapshots(previous.snapshot, decoded, keys, fields))
      emit(sample.tid, sample.time, previous.id, sample.method, sample.receiver, change, &previous.stack);
  }

  void drain() {
    while (true) {
      const auto dropped = queue.drops.load(std::memory_order_acquire);
      if (dropped != seen_drops) {
        const auto lost = dropped - seen_drops;
        const auto gaps = report.coverage_gaps() + lost;
        seen_drops = dropped;
        emit(0, 0, 0, 0, 0, {"", std::to_string(lost) + " method boundary samples dropped", true});
        report.set_coverage_gaps(gaps);
        // The missing sample can change receiver ownership or pairing.
        // Re-establish both from a later complete invocation.
        pending.clear();
        receivers.clear();
        report.set_admitted_receivers(0);
        drop_barrier = queue.tail.load(std::memory_order_acquire);
      }
      auto &slot = queue.records[cursor % arm_snapshot::slots];
      if (slot.turn.load(std::memory_order_acquire) != cursor + 1)
        return;
      try {
        if (cursor >= drop_barrier)
          observe(slot.sample);
      } catch (const std::exception &) {
        report.set_coverage_gaps(report.coverage_gaps() + 1);
      }
      slot.turn.store(cursor + arm_snapshot::slots, std::memory_order_release);
      ++cursor;
    }
  }

  void reclaim() {
    channel.reclaim_request.store(1, std::memory_order_release);
    (void)::syscall(SYS_futex, &channel.reclaim_request, FUTEX_WAKE, 1, nullptr, nullptr, 0);
  }

  std::vector<injector::CodePatch> patches(bool restore) const {
    std::vector<injector::CodePatch> result;
    for (const auto &patch : redirects)
      result.push_back({patch.instruction.pc, restore ? &patch.replacement : &patch.original,
                        restore ? &patch.original : &patch.replacement, sizeof(patch.original)});
    return result;
  }
};

MethodWatch::MethodWatch(const api::ObjectWatchRequest &request, std::uint64_t id,
    const api::Session &session, const api::ObjectWatchStatus &preview, const std::string &directory,
    const std::string &agent, std::chrono::seconds timeout, object_watch::Watch::Publish publish)
    : impl_(std::make_unique<Impl>(request, id, session, preview, directory, agent, timeout, std::move(publish))) {}

MethodWatch::~MethodWatch() {
  try { (void)stop(); } catch (...) {}
}

void MethodWatch::start() {
  auto &p = *impl_;
  auto bound = p.request;
  const auto &bindings = p.report.plan().bindings();
  std::vector<std::pair<Address, std::string>> verified;
  std::string module;
  Address bias = 0;
  for (int index = 0; index != bindings.size(); ++index) {
    const auto &binding = bindings[index];
    auto &method = *bound.mutable_methods(index);
    auto boundary = object_watch::Plan::method_boundary(binding.module(), method, nullptr, true, p.session.pid());
    if (binding.SerializeAsString() != boundary.binding.SerializeAsString())
      throw std::runtime_error("method binding changed during installation");
    const auto pc = boundary.origin.pc();
    if (module.empty()) {
      module = binding.module();
      bias = binding.runtime_pc() - binding.elf_pc();
    }
    if (module != binding.module() || bias != binding.runtime_pc() - binding.elf_pc())
      throw std::runtime_error("method snapshots require one mapped instance of the prepared ELF");
    p.origins.emplace(pc, std::move(boundary.origin));
    p.method_probes.emplace(pc, binding.runtime_pc());
    method.set_module_sha256(std::move(boundary.file_sha256));
    const auto code = native_entry::scan(pc, boundary.body);
    if (code.decode_failed)
      throw std::runtime_error("cannot decode the complete AArch64 snapshot method");
    const auto entry_pc = binding.runtime_pc();
    for (const auto &inst : code.instructions) {
      if (inst.flow == native_entry::Flow::branch && (!inst.target ||
          *inst.target < pc || *inst.target - pc >= boundary.body.size() || *inst.target <= entry_pc))
        throw std::runtime_error("snapshot method has an unverified tail or entry branch");
      if (inst.address != entry_pc && inst.flow != native_entry::Flow::returns)
        continue;
      if (inst.size != 4 || !inst.cache_copyable ||
          (inst.flow == native_entry::Flow::returns && inst.text != "ret" &&
           inst.text != "retaa" && inst.text != "retab"))
        throw std::runtime_error("snapshot method has an unsupported entry or return instruction");
      const bool is_entry = inst.address == entry_pc;
      auto &patch = p.redirects.emplace_back();
      patch.instruction.pc = inst.address;
      patch.instruction.size = inst.size;
      patch.instruction.hook = is_entry ? Hook::snapshot_entry : Hook::snapshot_return;
      patch.instruction.boundary_pc = pc;
      patch.instruction.immediate = index;
      std::memcpy(patch.instruction.bytes.data(), boundary.body.data() + inst.address - pc, inst.size);
      std::memcpy(&patch.original, patch.instruction.bytes.data(), inst.size);
    }
    verified.emplace_back(pc, std::move(boundary.body));
  }
  std::ranges::sort(p.redirects, {}, [](const auto &r) { return r.instruction.pc; });
  for (std::size_t i = 1; i < p.redirects.size(); ++i)
    if (p.redirects[i - 1].instruction.pc == p.redirects[i].instruction.pc)
      throw std::runtime_error("snapshot method redirect overlaps another method");

  injector::Process process;
  const auto operation_deadline = Clock::now() + p.timeout;
  for (;;) {
    if (!p.alive())
      throw std::runtime_error("target exited before method snapshot installation");
    try {
      process.attach(p.session.pid());
      break;
    } catch (const injector::IdlePointUnavailable &) {
      if (Clock::now() >= operation_deadline)
        throw;
      std::this_thread::sleep_for(1ms);
    }
  }
  arm_snapshot::Init init;
  init.code.event_buffer = 2;
  init.code.stack_depth = p.request.stack_depth();
  init.code.cfi_stack = p.request.stack_depth() != 0;
  init.code.function_count = p.redirects.size();
  init.code.module_bias = bias;
  if (p.control.path.size() >= sizeof(init.code.path) || p.payload.path.size() >= sizeof(init.queue) ||
      module.size() >= sizeof(init.code.module))
    throw std::runtime_error("snapshot path exceeds the native channel limit");
  std::memcpy(init.code.path, p.control.path.c_str(), p.control.path.size() + 1);
  std::memcpy(init.queue, p.payload.path.c_str(), p.payload.path.size() + 1);
  if (module != p.session.executable())
    std::memcpy(init.code.module, module.c_str(), module.size() + 1);
  std::vector<std::pair<Address, Address>> read_only;
  struct stat identity{};
  if (::stat(module.c_str(), &identity))
    throw std::runtime_error("cannot identify the method snapshot RTTI provider");
  std::ifstream maps("/proc/" + std::to_string(p.session.pid()) + "/maps");
  for (std::string line; std::getline(maps, line);) {
    unsigned long long begin = 0, end = 0, inode = 0;
    unsigned device_major = 0, device_minor = 0;
    char permissions[5]{};
    int path = 0;
    if (std::sscanf(line.c_str(), "%llx-%llx %4s %*x %x:%x %llu %n", &begin, &end, permissions,
                    &device_major, &device_minor, &inode, &path) != 6 || !inode || begin >= end ||
        (std::string_view(permissions) != "r--p" && std::string_view(permissions) != "r-xp") ||
        std::string_view(line).substr(path) != module)
      continue;
    if (inode != identity.st_ino ||
        makedev(device_major, device_minor) != identity.st_dev)
      throw std::runtime_error("cannot identify the method snapshot RTTI provider");
    read_only.emplace_back(begin, end);
  }
  if (read_only.empty() || read_only.size() > 32)
    throw std::runtime_error("cannot verify snapshot RTTI mapping ranges");
  init.read_only_count = read_only.size();
  for (std::size_t index = 0; index != read_only.size(); ++index)
    init.read_only[index] = {read_only[index].first, read_only[index].second};
  const auto spec = bound.SerializeAsString();
  constexpr char entry_name[] = "tracer_agent_arm_snapshot_v2";
  const auto count = p.redirects.size();
  const auto table_size = count * sizeof(Function);
  const auto code_size = count * sizeof(Instruction);
  const auto size = sizeof(init) + table_size + code_size + spec.size() + sizeof(entry_name) + p.agent.size() + 1;
  const auto remote = process.remote_allocate(size);
  try {
    const auto table = remote + sizeof(init);
    const auto instructions = table + table_size;
    const auto metadata = instructions + code_size;
    const auto name = metadata + spec.size();
    const auto library = name + sizeof(entry_name);
    init.code.functions = reinterpret_cast<Function *>(table);
    init.specifications = reinterpret_cast<const char *>(metadata);
    init.specifications_size = spec.size();
    std::vector<Function> functions;
    for (std::size_t i = 0; i != count; ++i) {
      const auto &inst = p.redirects[i].instruction;
      const auto address = instructions + i * sizeof(inst);
      functions.push_back({reinterpret_cast<const Instruction *>(address), 1, inst.pc, 0,
                           inst.hook == Hook::snapshot_entry ? inst.pc + 4 : 0});
      process.remote_write(address, &inst, sizeof(inst));
    }
    process.remote_write(table, functions.data(), table_size);
    process.remote_write(metadata, spec.data(), spec.size());
    process.remote_write(name, entry_name, sizeof(entry_name));
    process.remote_write(library, p.agent.c_str(), p.agent.size() + 1);
    process.remote_write(remote, &init, sizeof(init));
    const auto handle = process.remote_call(process.resolve_remote_symbol("dlopen"), {library, RTLD_NOW | RTLD_LOCAL}, 2);
    const auto entry = handle ? process.remote_call(process.resolve_remote_symbol("dlsym"), {handle, name}, 2) : 0;
    if (!entry)
      throw std::runtime_error("target could not load the prepared method snapshot agent");
    std::uintptr_t result;
    try {
      result = process.remote_call(entry, {remote}, 1);
    } catch (const std::exception &error) {
      throw std::runtime_error(std::string("method snapshot agent initialization: ") + error.what());
    }
    if (result) {
      process.remote_read(remote, &init, sizeof(init));
      throw std::runtime_error(std::string("cannot initialize method snapshot: ") + init.code.error);
    }
    process.remote_read(table, functions.data(), table_size);
    for (std::size_t i = 0; i != count; ++i) {
      auto &patch = p.redirects[i];
      patch.gateway = functions[i].translated;
      const auto branch = direct_branch(patch.instruction.pc, patch.gateway);
      if (!patch.gateway || !branch)
        throw std::runtime_error("snapshot redirect exceeds AArch64 branch reach");
      patch.replacement = *branch;
    }
  } catch (...) {
    if (!process.unsafe())
      process.remote_free(remote, size);
    p.reclaim();
    throw;
  }
  process.remote_free(remote, size);
  if (p.request.timeout_seconds())
    p.deadline = Clock::now() + std::chrono::seconds(p.request.timeout_seconds());
  p.reader = std::jthread([&p](std::stop_token stop) {
    while (!stop.stop_requested()) {
      {
        const std::lock_guard lock(p.mutex);
        p.drain();
      }
      std::this_thread::sleep_for(1ms);
    }
  });
  const auto validate = [&] {
    if (!p.alive() || !p.has_control_mapping())
      throw std::runtime_error("target mappings changed during method snapshot installation");
    for (const auto &[pc, bytes] : verified) {
      std::string actual(bytes.size(), '\0');
      process.remote_read(pc, actual.data(), actual.size());
      if (actual != bytes)
        throw std::runtime_error("live snapshot method differs from the verified ELF");
    }
  };
  try {
    if (!process.patch_code_batch(p.patches(false), validate, [&]() noexcept {
          if (p.request.timeout_seconds())
            p.channel.deadline_ns.store(std::chrono::duration_cast<std::chrono::nanoseconds>(
                p.deadline.time_since_epoch()).count(), std::memory_order_relaxed);
          p.installed = true;
          p.channel.activity.store(enabled, std::memory_order_release);
        }))
      throw std::runtime_error("could not install the verified method snapshot redirects");
    process.detach();
  } catch (...) {
    p.pending_restore = process.unsafe();
    if (!p.installed && !p.pending_restore) {
      p.reclaim();
      p.reader.request_stop();
      p.reader.join();
    }
    throw;
  }
}

api::ObjectWatchStatus MethodWatch::status() {
  const auto &p = *impl_;
  if (p.installed && !p.stopped && (!p.alive() || p.mapping_replaced()))
    return stop();
  const std::lock_guard lock(p.mutex);
  auto report = p.report;
  report.set_active(p.installed && !p.stopped);
  report.set_restoration_pending(p.pending_restore || p.draining || (p.installed && p.stopped));
  report.set_dropped(p.queue.drops.load(std::memory_order_acquire));
  report.set_static_fragments(p.channel.static_fragments.load(std::memory_order_acquire));
  report.set_static_reclaimed(p.channel.static_reclaimed.load(std::memory_order_acquire));
  if (p.channel.capture_limit.load(std::memory_order_relaxed) == Channel::event_limit)
    report.set_capture_limit("max-events");
  else if (Clock::now() >= p.deadline)
    report.set_capture_limit("timeout");
  for (const auto &patch : p.redirects) {
    auto &out = *report.add_redirects();
    out.set_pc(patch.instruction.pc);
    out.set_translated_pc(patch.gateway);
    out.set_original(&patch.original, sizeof(patch.original));
    out.set_installed(&patch.replacement, sizeof(patch.replacement));
  }
  for (const auto &binding : report.plan().bindings()) {
    auto &probe = *report.add_method_probes();
    probe.set_id(binding.runtime_pc());
    probe.set_active(report.active());
    probe.set_original_bytes_restored(p.stopped && !p.installed && !p.pending_restore && !p.draining);
  }
  return report;
}

api::ObjectWatchStatus MethodWatch::stop() {
  auto &p = *impl_;
  if (p.stopped && !p.installed && !p.pending_restore && !p.reader.joinable())
    return status();
  p.stopped = true;
  p.channel.activity.fetch_and(~enabled, std::memory_order_acq_rel);
  const auto deadline = Clock::now() + p.timeout;
  bool replaced = false;
  while ((p.installed || p.pending_restore) && p.alive()) {
    if ((replaced = p.mapping_replaced()))
      break;
    injector::Process process;
    try {
      process.attach_for_patch(p.session.pid());
      if (!p.alive() || (replaced = p.mapping_replaced())) {
        process.detach();
        break;
      }
      auto patches = p.patches(true);
      std::erase_if(patches, [&](const auto &patch) {
        std::uint32_t actual;
        process.remote_read(patch.address, &actual, sizeof(actual));
        return !std::memcmp(&actual, patch.replacement, sizeof(actual));
      });
      const auto restored = process.patch_code_batch(patches, [&] {
        if (!p.alive() || !p.has_control_mapping())
          throw std::runtime_error("target mappings changed during method snapshot restoration");
      });
      process.detach();
      if (restored)
        break;
      if (Clock::now() >= deadline)
        throw std::runtime_error("timed out restoring method snapshot redirects");
      std::this_thread::sleep_for(1ms);
    } catch (...) {
      p.pending_restore |= process.unsafe();
      if (p.alive() && !(replaced = p.mapping_replaced()))
        throw;
      break;
    }
  }
  p.installed = false;
  p.pending_restore = false;
  p.draining = true;
  p.reclaim();
  while (!replaced && p.alive() && p.channel.activity.load(std::memory_order_acquire) && Clock::now() < deadline)
    std::this_thread::sleep_for(1ms);
  if (!replaced && p.alive() && p.channel.activity.load(std::memory_order_acquire))
    throw std::runtime_error("timed out draining active method snapshot callbacks");
  p.reader.request_stop();
  if (p.reader.joinable())
    p.reader.join();
  {
    const std::lock_guard lock(p.mutex);
    p.drain();
    if (replaced) {
      p.report.set_diagnostic("target image changed and the original method redirects are gone");
      p.emit(p.session.pid(), 0, 0, 0, 0, {"", "target executable mapping changed", true}, nullptr,
             api::ObjectObservation::MAPPING_CHANGED);
    }
    for (const auto &[key, call] : p.pending) {
      const auto &[tid, method, stack] = key;
      p.emit(tid, 0, call.id, method, call.receiver,
             {"receiver", "method return unavailable at retirement", true}, &call.stack);
    }
    p.pending.clear();
    p.receivers.clear();
    p.report.set_admitted_receivers(0);
    p.draining = false;
  }
  return status();
}
}

#include "object_watch.hpp"

#include "debug_resolver.hpp"
#include "linux_syscalls.hpp"
#include "object_watch_plan.hpp"
#include "proc_identity.hpp"
#include "object_target.hpp"
#include "object_launch.hpp"
#include "tracer/protocol.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <limits>
#include <mutex>
#include <set>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <linux/audit.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

namespace tracer::object_watch {
namespace {
namespace api = clangtracer::v1;
using Observation = api::ObjectObservation;
using Clock = std::chrono::steady_clock;
using Address = std::uint64_t;
using object_target::Registers;
using object_target::control;
using object_target::reg;

void *ptrace_arg(unsigned long value) { return reinterpret_cast<void *>(value); }

bool read_memory(pid_t pid, Address address, void *output, std::size_t count) {
  iovec local{output, count}, remote{reinterpret_cast<void *>(address), count};
  return address && count - 1 <= std::numeric_limits<Address>::max() - address &&
      linux_api::process_vm_readv(pid, &local, 1, &remote, 1, 0) == static_cast<ssize_t>(count);
}

std::string read_bytes(pid_t pid, Address address, std::size_t count) {
  std::string result(count, '\0');
  if (count && !read_memory(pid, address, result.data(), count))
    throw std::runtime_error("object watch could not read target memory");
  return result;
}

template <typename T> T read(pid_t pid, Address address) {
  const auto bytes = read_bytes(pid, address, sizeof(T));
  T value{};
  std::memcpy(&value, bytes.data(), sizeof(T));
  return value;
}

Address add(Address base, Address offset) {
  if (offset > std::numeric_limits<Address>::max() - base)
    throw std::runtime_error("object watch address overflow");
  return base + offset;
}

struct Stop {
  int status = 0;
  int signal() const { return WSTOPSIG(status); }
  unsigned event() const { return static_cast<unsigned>(status) >> 16; }
  bool stopped() const { return WIFSTOPPED(status); }
};

bool breakpoint_signal(const siginfo_t &signal) {
  return signal.si_signo == SIGTRAP &&
         (signal.si_code == SI_KERNEL || signal.si_code == TRAP_BRKPT);
}

// Linux UAPI layout for PTRACE_GET_SYSCALL_INFO. Older libc headers lack the
// declaration even when the running kernel supports the operation.
struct SyscallInfo {
  enum Operation : std::uint8_t { entry_stop = 1, exit_stop = 2 };
  std::uint8_t op, reserved;
  std::uint16_t flags;
  std::uint32_t arch;
  std::uint64_t instruction_pointer, stack_pointer;
  union {
    struct { std::uint64_t nr, args[6]; } entry;
    struct { std::int64_t rval; std::uint8_t is_error; } exit;
  };
};
static_assert(offsetof(SyscallInfo, entry) == 24);

SyscallInfo syscall_info(pid_t tid) {
  constexpr auto get_syscall_info = static_cast<__ptrace_request>(0x420e);
  SyscallInfo info{};
  if (::ptrace(get_syscall_info, tid, ptrace_arg(sizeof(info)), &info) == -1)
    throw std::system_error(errno, std::generic_category(), "read Linux syscall stop");
  return info;
}

struct Lifetime {
  Address address = 0;
  std::uint64_t id = 0, generation = 0;
  Address owner = 0;
};
struct LogicalRange {
  RangeHeader header{};
  Address destination = 0;
  std::uint64_t generation = 0;
  bool valid = false;
  std::optional<std::int32_t> prior_value = std::nullopt;
};
struct Frame {
  Boundary role;
  Address entry, receiver, return_pc, return_sp;
  Lifetime life{};
  Address incoming = 0;
  Address incoming_view = 0;
  std::vector<Address> prior;
  std::vector<std::pair<Address, Lifetime>> prior_watched;
  std::string description;
  bool outer_constructor = true;
  bool uncertain_complete = false;
  api::StackTrace entry_stack;
  LogicalRange range;
  Address active_inline_mutation = 0;
  bool provisional = false;
};
struct Patch {
  std::string bytes;
  bool armed = false;
};

struct SyscallSample {
  Lifetime life;
  Address pc, destination;
  std::optional<std::int32_t> old;
  bool overlapping = false;
};

struct Task {
  Stop stop;
  bool stopped = false;
  bool child = false;
  bool listen = false;
  int delivery = 0;
  int trap_code = 0;
  std::optional<Address> breakpoint_pc;
  std::optional<SyscallSample> syscall;
  std::vector<Frame> frames;
};

bool creation_event(unsigned event) {
  return event == PTRACE_EVENT_CLONE || event == PTRACE_EVENT_FORK ||
         event == PTRACE_EVENT_VFORK;
}

constexpr unsigned common_ptrace_options = PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK |
    PTRACE_O_TRACEVFORK | PTRACE_O_TRACEEXEC;
} // namespace

struct Watch::Worker {
  const api::ObjectWatchRequest request;
  const std::uint64_t id;
  pid_t pid;
  std::uint64_t process_start;
  const std::string executable;
  std::vector<std::string> launch_argv;
  const std::chrono::seconds timeout;
  const Publish publish;
  std::unique_ptr<Plan> plan;
  std::map<Address, Patch> patches;
  std::map<pid_t, Task> tasks;
  std::map<Address, Lifetime> live;
  std::map<Address, std::uint64_t> generations;
  std::uint64_t next_id = 0, sequence = 0;
  std::optional<int> exit_status;
  bool lifecycle_stop = false, image_replaced = false;
  std::mutex mutex;
  std::condition_variable changed;
  api::ObjectWatchStatus report;
  bool ready = false, finished = false, abandon = false, armed_once = false;
  unsigned retry = 0;
  std::atomic<bool> stopping{false};
  std::atomic<Observation::ClosureReason> stop_reason{Observation::WATCH_STOPPED};
  std::thread thread;

  Worker(const api::ObjectWatchRequest &input, std::uint64_t watch_id,
         pid_t target, std::uint64_t start, std::string image,
         std::chrono::seconds operation_timeout, Publish sink,
         std::vector<std::string> argv = {})
      : request(input), id(watch_id), pid(target), process_start(start),
        executable(std::move(image)), launch_argv(std::move(argv)),
        timeout(operation_timeout), publish(std::move(sink)) {
    report.set_watch_id(id);
    report.set_no_values(request.no_values());
    thread = std::thread([this] { run(); });
  }

  ~Worker() {
    {
      std::lock_guard lock(mutex);
      stopping = true;
      abandon = true;
    }
    changed.notify_all();
    thread.join();
  }

  void startup() {
    std::unique_lock lock(mutex);
    if (!changed.wait_for(lock, timeout, [&] { return ready; })) {
      stopping = true;
      report.set_restoration_pending(true);
      report.set_diagnostic("restoration pending: timed out starting the watch");
      changed.notify_all();
      return;
    }
    if (!report.active() && !report.restoration_pending() && (launch_argv.empty() || !armed_once))
      throw std::runtime_error(report.diagnostic());
  }

  api::ObjectWatchStatus stop(bool detaching) {
    std::unique_lock lock(mutex);
    const auto attempt = ++retry;
    if (!stopping && detaching)
      stop_reason = Observation::WATCH_DETACHED;
    stopping = true;
    changed.notify_all();
    if (!changed.wait_for(lock, timeout, [&] { return finished || failed_attempt == attempt; })) {
      report.set_restoration_pending(true);
      report.set_diagnostic("restoration pending: timed out waiting for target stop or cleanup");
    }
    return report;
  }

  unsigned failed_attempt = std::numeric_limits<unsigned>::max();

  void pause(Clock::time_point deadline) {
    std::unique_lock lock(mutex);
    if (abandon || Clock::now() >= deadline)
      throw std::runtime_error("could not confirm object watch thread stops");
    changed.wait_for(lock, std::chrono::microseconds(250));
  }

  Registers registers(pid_t tid) const { return object_target::read_registers(tid); }

  siginfo_t signal_info(pid_t tid) const {
    siginfo_t info{};
    control(PTRACE_GETSIGINFO, tid, &info);
    return info;
  }

  void resume(pid_t tid, Task &task, __ptrace_request command) {
    if (command == PTRACE_CONT && request.syscall_output())
      command = PTRACE_SYSCALL;
    control(command, tid, ptrace_arg(task.delivery));
    task.stopped = false;
    task.delivery = 0;
  }

  void interrupt(pid_t tid) const {
    if (::ptrace(PTRACE_INTERRUPT, tid, nullptr, nullptr) == -1 && errno != ESRCH)
      throw std::system_error(errno, std::generic_category(), "interrupt object watch thread");
  }

  bool target_thread(pid_t tid) const {
    std::error_code error;
    return std::filesystem::exists(proc::directory(pid) / "task" / std::to_string(tid), error);
  }

  unsigned ptrace_options() const {
    return common_ptrace_options | (request.syscall_output() ? PTRACE_O_TRACESYSGOOD : 0);
  }

  void observe(pid_t tid, Stop stop) {
    if (!stop.stopped()) {
      if (const auto task = tasks.find(tid); ready && plan && task != tasks.end())
        for (const auto &frame : task->second.frames)
          close_unverified(tid, frame, Observation::THREAD_EXITED);
      tasks.erase(tid);
      if (tid == pid) {
        lifecycle_stop = true;
        const std::lock_guard lock(mutex);
        exit_status = stop.status;
      }
      return;
    }
    auto [it, inserted] = tasks.try_emplace(tid);
    auto &task = it->second;
    task.child |= inserted && !target_thread(tid);
    task.stop = stop;
    task.stopped = true;
    task.listen = false;
    task.delivery = 0;
    task.trap_code = 0;
    if (stop.event() == PTRACE_EVENT_EXEC) {
      // The execing task survives with the leader's TID and a different image.
      image_replaced = lifecycle_stop = true;
      patches.clear();
      tasks.clear();
      tasks[tid].stopped = true;
      return;
    }
    if (creation_event(stop.event())) {
      unsigned long child = 0;
      control(PTRACE_GETEVENTMSG, tid, &child);
      const auto child_tid = static_cast<pid_t>(child);
      auto &created = tasks[child_tid];
      created.child |= stop.event() != PTRACE_EVENT_CLONE || !target_thread(child_tid);
      lifecycle_stop |= created.child;
      return;
    }
    if (task.child)
      lifecycle_stop = true;
    if (stop.event() == PTRACE_EVENT_STOP) {
      task.listen = stop.signal() != SIGTRAP && !task.child;
      return;
    }
    if (stop.event())
      throw std::runtime_error("unexpected object watch ptrace event");
    if (stop.signal() == (SIGTRAP | 0x80) && request.syscall_output()) {
      if (plan && ready)
        observe_syscall(tid, task);
      return;
    }
    task.delivery = stop.signal();
    if (stop.signal() != SIGTRAP)
      return;
    const auto info = signal_info(tid);
    task.trap_code = info.si_code;
    if (!breakpoint_signal(info))
      return;
    const auto pc = object_target::trap_pc(registers(tid));
    const auto patch = patches.find(pc);
    if (patch != patches.end() && patch->second.armed) {
      task.delivery = 0;
      task.breakpoint_pc = pc;
    }
  }

  pid_t receive() {
    Stop stop;
    // Other server threads may be attaching or tracing unrelated processes.
    const auto tid = ::waitpid(-1, &stop.status, __WALL | __WNOTHREAD | WNOHANG);
    if (tid > 0)
      observe(tid, stop);
    else if (tid < 0 && errno != ECHILD && errno != EINTR)
      throw std::system_error(errno, std::generic_category(), "wait for object watch task");
    return tid;
  }

  void await_stops(Clock::time_point deadline) {
    while (std::any_of(tasks.begin(), tasks.end(), [](const auto &item) {
      return !item.second.stopped;
    })) {
      if (Clock::now() >= deadline || receive() <= 0)
        pause(deadline);
    }
  }

  void park_threads(Clock::time_point deadline) {
    // A peer may already have hit a breakpoint. Consume its queued stop before asking
    // the kernel for an additional interrupt.
    while (receive() > 0) {}
    for (const auto &[tid, task] : tasks)
      if (!task.stopped)
        interrupt(tid);
    await_stops(deadline);
  }

  void seize_threads(Clock::time_point deadline) {
    const auto directory = proc::directory(pid) / "task";
    if (proc::start_time(proc::directory(pid) / "stat") != process_start)
      throw std::runtime_error("object watch target identity changed");
    for (;;) {
      if (stopping || Clock::now() >= deadline)
        throw std::runtime_error("object watch thread admission expired");
      bool added = false;
      for (const auto &entry : std::filesystem::directory_iterator(directory)) {
        const auto tid = static_cast<pid_t>(std::stol(entry.path().filename().string()));
        if (tasks.contains(tid))
          continue;
        if (::ptrace(PTRACE_SEIZE, tid, nullptr, nullptr) == -1) {
          if (errno == ESRCH)
            continue;
          throw std::system_error(errno, std::generic_category(), "seize object watch thread");
        }
        tasks.try_emplace(tid);
        interrupt(tid);
        added = true;
      }
      if (!added)
        break;
      await_stops(deadline);
    }
    if (!tasks.contains(pid) || proc::start_time(proc::directory(pid) / "stat") != process_start)
      throw std::runtime_error("object watch target identity changed");
    for (const auto &[tid, task] : tasks) {
      if (!std::filesystem::exists(directory / std::to_string(tid)))
        throw std::runtime_error("object watch task identity changed");
      control(PTRACE_SETOPTIONS, tid, ptrace_arg(ptrace_options()));
    }
  }

  void launch_at_exec(Clock::time_point deadline) {
    auto child = object_launch::Child::stop_on_exec(launch_argv, ptrace_options(), deadline,
                                                    [this] { return stopping.load(); });
    const auto directory = proc::directory(child.pid());
    if (std::filesystem::read_symlink(directory / "exe") != executable)
      throw std::runtime_error("launched executable differs from the selected ELF");
    process_start = proc::start_time(directory / "stat");
    auto &task = tasks[child.pid()];
    task.stop.status = child.exec_status();
    task.stopped = true;
    pid = child.release();
  }

  void verify_bytes(Address pc, std::string_view expected, bool armed) const {
    auto actual = read_bytes(pid, pc, expected.size());
    const auto trap = object_target::trap;
    if (actual.size() < trap.size() || (armed && !actual.starts_with(trap)))
      throw std::runtime_error("object watch breakpoint ownership changed");
    if (armed)
      actual.replace(0, trap.size(), expected.substr(0, trap.size()));
    if (actual != expected)
      throw std::runtime_error("object watch code differs from the verified ELF");
  }

  void install(Address pc, std::string_view expected) {
    const auto trap = object_target::trap;
    if (expected.size() < trap.size() || expected.starts_with(trap) || pc % trap.size())
      throw std::runtime_error("cannot own an existing breakpoint or unmapped return address");
    const auto existing = patches.find(pc);
    if (existing != patches.end() && existing->second.armed) {
      verify_bytes(pc, expected, true);
      return;
    }
    verify_bytes(pc, expected, false);
    auto &patch = patches.try_emplace(pc, Patch{std::string(expected)}).first->second;
    patch.armed = true; // Retain ownership even if the write/read-back fails.
    object_target::write_stopped_code(pid, pc, trap);
    verify_bytes(pc, expected, true);
  }

  void restore_trap(pid_t target, Address pc, const Patch &patch) const {
    auto actual = read_bytes(target, pc, patch.bytes.size());
    if (actual == patch.bytes)
      return;
    const auto trap = object_target::trap;
    if (!actual.starts_with(trap) || actual.substr(trap.size()) != patch.bytes.substr(trap.size()))
      throw std::runtime_error("object watch code changed while a breakpoint was owned");
    object_target::write_stopped_code(target, pc, std::string_view(patch.bytes).substr(0, trap.size()));
    if (read_bytes(target, pc, patch.bytes.size()) != patch.bytes)
      throw std::runtime_error("object watch breakpoint restoration failed");
  }

  void restore() {
    park_threads(Clock::now() + timeout);
    if (!image_replaced) {
      const auto parent = std::find_if(tasks.begin(), tasks.end(), [](const auto &item) {
        return !item.second.child;
      });
      for (const auto &[pc, patch] : patches) {
        if (parent != tasks.end())
          restore_trap(parent->first, pc, patch);
        // A fork can inherit its own copy of the patched text.
        for (const auto &[tid, task] : tasks)
          if (task.child)
            restore_trap(tid, pc, patch);
      }
    }
    for (auto it = tasks.begin(); it != tasks.end();) {
      auto &[tid, task] = *it;
      if (task.breakpoint_pc)
        rewind(tid, task, registers(tid));
      if (::ptrace(PTRACE_DETACH, tid, nullptr, ptrace_arg(task.delivery)) == -1 &&
          errno != ESRCH)
        throw std::system_error(errno, std::generic_category(), "detach object watch thread");
      it = tasks.erase(it);
    }
    patches.clear();
  }

  void rewind(pid_t tid, Task &task, Registers regs) {
    object_target::set_pc(tid, regs, *task.breakpoint_pc);
    task.breakpoint_pc.reset();
  }

  std::optional<Address> complete_from_base(Address view) const {
    if (!view || view <= plan->layout.base_offset)
      return std::nullopt;
    const auto vptr = read<Address>(pid, view);
    if (vptr < 16 || read<Address>(pid, vptr - 8) != plan->typeinfo ||
        read<Address>(pid, vptr - 16) != Address{0} - plan->layout.base_offset)
      return std::nullopt;
    return view - plan->layout.base_offset;
  }

  bool rtti(Address object) const {
    if (!plan->typeinfo)
      return true;
    return complete_from_base(add(object, plan->layout.base_offset)) == object;
  }

  std::vector<Address> collection(Address owner) const {
    const auto &layout = plan->layout;
    const auto first = read<Address>(pid, add(owner, layout.vector_begin));
    const auto last = read<Address>(pid, add(owner, layout.vector_end));
    const auto limit = read<Address>(pid, add(owner, layout.vector_capacity));
    if (last < first || limit < last || (limit - first) % layout.stride ||
        (last - first) % layout.stride || (!first && (last || limit)) ||
        (last - first) / layout.stride > request.max_elements())
      throw std::runtime_error("owner vector layout is invalid or exceeds --max-elements");
    std::vector<Address> items;
    const auto bytes = read_bytes(pid, first, last - first);
    for (std::size_t pos = 0; pos < bytes.size(); pos += layout.stride) {
      Address pointer = 0;
      std::memcpy(&pointer, bytes.data() + pos + layout.pointee, sizeof(pointer));
      items.push_back(pointer);
    }
    return items;
  }

  Address map_element(const std::array<Address, 3> &header) const {
    const auto &layout = plan->layout;
    const auto destination = map_destination(layout.map, header, layout.scalar, request.map_key(),
        request.max_elements(), request.max_string_bytes(),
        [&](auto address, void *output, auto size) { return read_memory(pid, address, output, size); });
    if (!destination)
      throw std::runtime_error("logical map key or node links could not be verified within the read budget");
    return destination;
  }

  RangeHeader range_header(Address object) const {
    if (!plan->layout.pointer_offsets.empty()) {
      RangeHeader header{};
      if (!read_pointee_chain(object, plan->layout.pointer_offsets, header,
          [&](Address address, Address &value) {
            return read_memory(pid, address, &value, sizeof(value));
          }))
        throw std::runtime_error("pointee path is null, unreadable or outside the address range");
      return header;
    }
    if (plan->layout.logical_map)
      return {read<Address>(pid, add(object, plan->layout.map.root)),
              read<Address>(pid, add(object, plan->layout.map.count)),
              add(object, plan->layout.map.header)};
    return {read<Address>(pid, add(object, plan->layout.vector_begin)),
            read<Address>(pid, add(object, plan->layout.vector_end)),
            read<Address>(pid, add(object, plan->layout.vector_capacity))};
  }

  Address range_element(const RangeHeader &header) const {
    const auto &layout = plan->layout;
    if (!layout.pointer_offsets.empty()) {
      const auto address = add(header[layout.pointer_offsets.size() - 1], layout.scalar);
      (void)add(address, sizeof(std::int32_t) - 1);
      return address;
    }
    if (layout.logical_map)
      return map_element({header[0], header[1], header[2]});
    const auto begin = header[0], end = header[1], capacity = header[2];
    if (end < begin || capacity < end || (!begin && (end || capacity)) ||
        (capacity - begin) % layout.stride || (end - begin) % layout.stride ||
        (end - begin) / layout.stride > request.max_elements() || end - begin > (1ULL << 24))
      throw std::runtime_error("logical vector layout is invalid or exceeds the read budget");
    const auto bytes = read_bytes(pid, begin, end - begin);
    Address destination = 0;
    for (std::size_t offset = 0; offset < bytes.size(); offset += layout.stride) {
      std::uint32_t key = 0;
      std::memcpy(&key, bytes.data() + offset + layout.key_offset, sizeof(key));
      if (key != request.element_key())
        continue;
      if (destination)
        throw std::runtime_error("logical vector element key is duplicated");
      destination = add(add(begin, offset), layout.scalar);
    }
    if (!destination)
      throw std::runtime_error("logical vector element key is unavailable");
    return destination;
  }

  std::string field_path() const {
    return object_watch::field_path(request, plan->layout);
  }

  std::optional<std::uint32_t> union_tag(Address receiver) const {
    if (!plan->layout.union_tag_offset || request.no_values())
      return std::nullopt;
    std::uint32_t tag;
    if (read_memory(pid, add(receiver, *plan->layout.union_tag_offset), &tag, sizeof(tag)))
      return tag;
    return std::nullopt;
  }

  std::string description(Address object) const {
    const auto pointer = read<Address>(pid, add(object, plan->layout.string_pointer));
    const auto size = read<Address>(pid, add(object, plan->layout.string_length));
    if (size > request.max_string_bytes() || !pointer)
      throw std::runtime_error("snapshot string exceeds --max-string-bytes or has no storage");
    auto text = read_bytes(pid, pointer, size + 1);
    if (text.back() != '\0')
      throw std::runtime_error("snapshot string terminator could not be verified");
    text.pop_back();
    return text;
  }

  void stack(api::StackTrace &trace, Address pc, const Registers &regs) const {
    if (!request.stack_depth())
      return;
    trace.set_requested_depth(request.stack_depth());
    trace.set_status(api::StackTrace::PARTIAL);
    trace.set_detail("frame-pointer walk in the main executable");
    auto frame_pointer = object_target::fp(regs);
    auto sp = object_target::sp(regs);
    auto current = pc;
    for (unsigned depth = 0; depth < request.stack_depth(); ++depth) {
      const auto lookup = depth ? current - 1 : current;
      if (plan->code(lookup).empty())
        break;
      auto &frame = *trace.add_frames();
      const auto source = plan->source(lookup);
      frame.set_pc(current);
      frame.set_stack_pointer(sp);
      frame.set_kind(api::StackFrame::NATIVE);
      frame.set_application_frame(true);
      frame.set_module(plan->module_at(lookup));
      frame.set_module_offset(plan->elf_pc(lookup));
      frame.set_file(source.file);
      frame.set_line(source.line);
      frame.set_function(source.function);
      if (trace.frames_size() == static_cast<int>(request.stack_depth())) {
        trace.set_status(api::StackTrace::TRUNCATED);
        break;
      }
      if (frame_pointer < sp || frame_pointer - sp > 1024 * 1024 || frame_pointer % 8)
        break;
      try {
        current = read<Address>(pid, add(frame_pointer, 8));
        sp = add(frame_pointer, 16);
        frame_pointer = read<Address>(pid, frame_pointer);
        if (!current)
          break;
      } catch (const std::runtime_error &) {
        break;
      }
    }
    trace.set_captured_depth(trace.frames_size());
  }

  template <typename Fill>
  void emit(pid_t tid, Observation::Kind kind, const Lifetime &life, Address pc,
            const Registers &regs, Fill fill, const api::StackTrace *entry_stack = nullptr) {
    if (request.max_events() && sequence >= request.max_events())
      return;
    api::TraceEvent event;
    event.set_protocol_version(kProtocolVersion);
    event.set_session_id(request.session_id());
    event.set_pid(pid);
    event.set_tid(tid);
    event.set_probe_id(id);
    event.set_probe_sequence(++sequence);
    event.set_monotonic_time_ns(std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now().time_since_epoch()).count());
    event.set_function_name("object.trace");
    event.set_object_address(life.address);
    auto &observation = *event.mutable_object_observation();
    observation.set_kind(kind);
    observation.set_lifetime_id(life.id);
    observation.set_generation(life.generation);
    observation.set_concrete_type(request.concrete_type());
    observation.set_owner_address(life.owner);
    observation.set_pc(pc);
    const bool store = kind == Observation::FIELD_CHANGED || kind == Observation::FIELD_WRITTEN;
    observation.set_location_kind(store ? Observation::STORE :
        kind == Observation::SNAPSHOT_DELTA ? Observation::CALL_BOUNDARY : Observation::ENTRY_ANCHOR);
    if (pc && !plan->code(pc).empty()) {
      plan->describe_origin(pc, plan->source(pc), observation);
      if (entry_stack && entry_stack->requested_depth())
        *event.mutable_stack() = *entry_stack;
      else if (store && request.stack_depth())
        stack(*event.mutable_stack(), pc, regs);
    } else
      observation.set_source_unavailable(true);
    fill(observation);
    publish(std::move(event));
    std::lock_guard lock(mutex);
    report.set_observations(sequence);
    if (kind == Observation::COVERAGE_GAP)
      report.set_coverage_gaps(report.coverage_gaps() + 1);
    if (request.max_events() && sequence == request.max_events()) {
      report.set_capture_limit("max-events");
      stop_reason = Observation::EVENT_LIMIT;
      stopping = true;
    }
  }

  void observe_syscall(pid_t tid, Task &task) {
    const auto info = syscall_info(tid);
#if defined(__aarch64__)
    constexpr auto audit_arch = AUDIT_ARCH_AARCH64;
#else
    constexpr auto audit_arch = AUDIT_ARCH_X86_64;
#endif
    if (info.arch != audit_arch)
      return;
    if (info.op == SyscallInfo::entry_stop) {
      task.syscall.reset();
      const auto number = info.entry.nr;
      const auto destination = info.entry.args[1];
      if ((number != SYS_read && number != SYS_pread64) ||
          info.entry.args[2] != sizeof(std::int32_t) || destination < plan->layout.scalar ||
          (request.writer_tid() && request.writer_tid() != static_cast<unsigned>(tid)))
        return;
      const auto found = live.find(destination - plan->layout.scalar);
      if (found == live.end())
        return;
      const auto encoding = object_target::system_call;
      const auto pc = info.instruction_pointer > encoding.size() ?
          info.instruction_pointer - encoding.size() : 0;
      std::array<char, 4> instruction{};
      if (!read_memory(pid, pc, instruction.data(), encoding.size()) ||
          std::string_view(instruction.data(), encoding.size()) != encoding) {
        gap(tid, 0, {}, "kernel output site has no verified original syscall instruction");
        return;
      }
      SyscallSample sample{found->second, pc, destination, std::nullopt};
      std::int32_t value;
      if (!request.no_values() && read_memory(pid, destination, &value, sizeof(value)))
        sample.old = value;
      for (auto &[other_tid, other] : tasks) {
        if (other_tid == tid || !other.syscall || other.syscall->destination != destination)
          continue;
        other.syscall->overlapping = true;
        sample.overlapping = true;
      }
      task.syscall = sample;
      return;
    }
    if (info.op != SyscallInfo::exit_stop || !task.syscall)
      return;
    const auto sample = *std::exchange(task.syscall, std::nullopt);
    const auto found = live.find(sample.life.address);
    if (found == live.end() || found->second.id != sample.life.id)
      return;
    if (!info.exit.is_error && !info.exit.rval)
      return;
    const bool completed = !info.exit.is_error &&
        info.exit.rval == static_cast<std::int64_t>(sizeof(std::int32_t));
    std::optional<std::int32_t> value;
    std::int32_t bytes;
    if (completed && !sample.overlapping && !request.no_values() &&
        read_memory(pid, sample.destination, &bytes, sizeof(bytes)))
      value = bytes;
    const bool partial = !completed || sample.overlapping || (!request.no_values() && !value);
    if (!partial && value && sample.old == value)
      return;
    const auto kind = partial ? Observation::COVERAGE_GAP :
        (request.no_values() || !sample.old) ? Observation::FIELD_WRITTEN : Observation::FIELD_CHANGED;
    emit(tid, kind, sample.life, sample.pc, registers(tid), [&](auto &event) {
      event.set_location_kind(Observation::SYSCALL_OUTPUT);
      event.set_field(field_path());
      event.set_destination(sample.destination);
      event.set_synchronization(request.synchronization());
      if (partial)
        event.set_diagnostic(sample.overlapping ? "overlapping kernel output; field values unavailable" :
            "kernel output was partial, failed or unreadable; field values unavailable");
      else if (!request.no_values()) {
        if (sample.old)
          event.set_old_integer(*sample.old);
        event.set_new_integer(*value);
      }
      if (event.module().empty()) {
        try {
          const auto mappings = client::list_debug_module_mappings(pid, true);
          for (const auto &mapping : mappings.mappings())
            if (sample.pc >= mapping.begin_address() && sample.pc < mapping.end_address()) {
              event.set_module(mapping.path());
              event.set_module_build_id(mapping.build_id());
              break;
            }
        } catch (const std::exception &) {}
      }
    });
  }

  void gap(pid_t tid, Address pc, const Registers &regs, const std::string &message) {
    emit(tid, Observation::COVERAGE_GAP, {}, pc, regs,
         [&](auto &out) { out.set_diagnostic(message); });
  }

  void close_unverified(pid_t tid, const Frame &frame, Observation::ClosureReason reason) {
    if (frame.role == Boundary::construct && !frame.outer_constructor)
      return;
    const bool aborted = frame.role == Boundary::construct && reason == Observation::UNWOUND;
    const auto kind = aborted ? Observation::CONSTRUCTION_ABORTED : Observation::LIFETIME_UNCERTAIN;
    Lifetime identity = frame.life;
    if (!identity.address)
      identity.address = frame.receiver;
    emit(tid, kind, identity, frame.entry, {}, [&](auto &out) {
      out.set_closure_reason(reason);
      out.set_diagnostic(aborted ? "constructor unwound without a verified return" :
                                   "boundary ended without a verified return");
    }, &frame.entry_stack);
  }

  bool range_snapshot_enabled() const {
    using Request = clangtracer::v1::ObjectWatchRequest;
    return !request.no_values() &&
        (request.synchronization() == Request::THREAD_CONFINED ||
         request.synchronization() == Request::CALLER_LOCK);
  }

  void suspend_range(pid_t tid, Frame &scope, Address pc, const Registers &regs,
                     const api::StackTrace *anchor_stack = nullptr) {
    if (!scope.range.valid)
      return;
    scope.range.valid = false;
    if (range_snapshot_enabled()) {
      try {
        const auto header = range_header(scope.receiver);
        if (header != scope.range.header || range_element(header) != scope.range.destination)
          throw std::runtime_error("collection range changed outside a verified mutation return");
        scope.range.prior_value = read<std::int32_t>(pid, scope.range.destination);
      } catch (const std::runtime_error &error) {
        gap(tid, pc, regs, error.what());
      }
    }
    emit(tid, Observation::RANGE_SUSPENDED, scope.life, pc, regs, [&](auto &out) {
      out.set_field(field_path());
      out.set_storage_generation(scope.range.generation);
      out.set_destination(scope.range.destination);
      out.set_synchronization(request.synchronization());
    }, anchor_stack);
  }

  void bind_range(pid_t tid, Frame &scope, Address pc, const Registers &regs,
                   const api::StackTrace &anchor_stack) {
    const auto old = std::exchange(scope.range.prior_value, std::nullopt);
    scope.range.valid = false;
    try {
      const auto header = range_header(scope.receiver);
      const auto destination = range_element(header);
      if (!scope.range.generation || plan->layout.logical_map || !plan->layout.pointer_offsets.empty() ||
          header[0] != scope.range.header[0] ||
          destination != scope.range.destination)
        ++scope.range.generation;
      scope.range.header = header;
      scope.range.destination = destination;
      scope.range.valid = true;
      std::int32_t next = 0;
      const bool sampled = range_snapshot_enabled() &&
          read_memory(pid, destination, &next, sizeof(next));
      emit(tid, Observation::RANGE_BOUND, scope.life, pc, regs, [&](auto &out) {
        out.set_field(field_path());
        out.set_storage_generation(scope.range.generation);
        out.set_destination(destination);
        out.set_synchronization(request.synchronization());
        if (sampled)
          set_scalar_values(out, plan->layout, {old, {}}, {next, {}});
      }, &anchor_stack);
      if (range_snapshot_enabled() && !sampled)
        gap(tid, pc, regs, "bound field value is unreadable");
    } catch (const std::runtime_error &error) {
      scope.range.destination = 0;
      gap(tid, pc, regs, error.what());
    }
  }

  void entry(pid_t tid, Task &task, Boundary role, Address pc, const Registers &regs) {
    auto receiver = object_target::argument(regs, 0);
    if (role == Boundary::snapshot) {
      const auto complete = complete_from_base(receiver);
      if (!complete)
        return;
      receiver = *complete;
    }
    const auto found = live.find(receiver);
    if ((role == Boundary::destroy || role == Boundary::snapshot) && found == live.end())
      return;
    Frame frame{};
    frame.role = role;
    frame.entry = pc;
    frame.receiver = receiver;
    frame.return_pc = object_target::return_pc(regs, [&](Address address) { return read<Address>(pid, address); });
    frame.return_sp = add(object_target::sp(regs), object_target::return_stack_delta);
    if (role == Boundary::construct) {
      frame.outer_constructor = std::none_of(task.frames.begin(), task.frames.end(),
          [&](const auto &pending) {
            return pending.role == Boundary::construct && pending.receiver == receiver;
          });
      frame.uncertain_complete = plan->sites.at(pc).uncertain_complete;
    }
    stack(frame.entry_stack, pc, regs);
    if (role == Boundary::writer_scope) {
      if (!receiver || !rtti(receiver) ||
          std::any_of(task.frames.begin(), task.frames.end(), [&](const auto &pending) {
            return pending.receiver == receiver &&
                (pending.role == Boundary::construct || pending.role == Boundary::writer_scope);
          }))
        return;
      if (found == live.end()) {
        frame.life = Lifetime{receiver, ++next_id};
        frame.provisional = true;
      } else {
        frame.life = found->second;
      }
    } else if (role == Boundary::collection_mutation) {
      const auto scope = std::find_if(task.frames.rbegin(), task.frames.rend(), [&](const auto &pending) {
        return pending.role == Boundary::writer_scope &&
               receiver == add(pending.receiver, plan->layout.vector_object);
      });
      if (scope == task.frames.rend() || scope->active_inline_mutation)
        return;
      frame.life = scope->life;
      suspend_range(tid, *scope, pc, regs, &frame.entry_stack);
    } else if (role == Boundary::destroy) {
      frame.life = found->second;
      live.erase(found);
      emit(tid, Observation::DESTROY_BEGIN, frame.life, pc, regs, [](auto &) {}, &frame.entry_stack);
    } else if (role == Boundary::snapshot) {
      if (!rtti(receiver))
        throw std::runtime_error("snapshot receiver RTTI changed");
      frame.life = found->second;
      frame.description = description(receiver);
    } else if (role == Boundary::add || role == Boundary::remove) {
      frame.prior = collection(receiver);
      if (role == Boundary::add) {
        frame.incoming_view = read<Address>(pid, add(object_target::argument(regs, 1), plan->layout.pointee));
        const auto complete = complete_from_base(frame.incoming_view);
        if (!complete || !live.contains(*complete))
          return;
        frame.incoming = *complete;
      } else {
        // Keep identities while destructors can run inside the erase.
        for (const auto view : frame.prior) {
          const auto complete = complete_from_base(view);
          if (!complete)
            continue;
          const auto item = live.find(*complete);
          if (item == live.end() || item->second.owner != receiver)
            continue;
          frame.prior_watched.emplace_back(view, item->second);
        }
        if (frame.prior_watched.empty())
          return;
      }
    }
    const auto existing = patches.find(frame.return_pc);
    if (existing == patches.end()) {
      const auto bytes = plan->code(frame.return_pc).substr(0, 15);
      const auto decoded = native_entry::scan(frame.return_pc, bytes);
      if (decoded.instructions.empty())
        throw std::runtime_error("boundary return is outside verified executable code");
      install(frame.return_pc, bytes.substr(0, decoded.instructions.front().size));
    }
    task.frames.push_back(std::move(frame));
    if (role == Boundary::writer_scope) {
      auto &scope = task.frames.back();
      if (scope.provisional)
        emit(tid, Observation::FIRST_SEEN, scope.life, pc, regs, [](auto &out) {
          out.set_diagnostic("receiver identity is limited to this invocation");
        }, &scope.entry_stack);
      if (plan->layout.logical_collection())
        bind_range(tid, scope, pc, regs, scope.entry_stack);
    }
  }

  void boundary_return(pid_t tid, Task &task, Frame frame, const Registers &regs) {
    const auto pc = frame.entry;
    const auto receiver = frame.receiver;
    const auto found = live.find(receiver);
    switch (frame.role) {
    case Boundary::construct: {
      if (!frame.outer_constructor)
        return;
      if (frame.uncertain_complete) {
        emit(tid, Observation::LIFETIME_UNCERTAIN, Lifetime{receiver}, pc, regs, [](auto &out) {
          out.set_diagnostic("complete and base constructors share the same entry");
        }, &frame.entry_stack);
        return;
      }
      if (found != live.end() || !rtti(receiver))
        throw std::runtime_error("complete constructor returned with an unexpected type or a live generation");
      if (request.max_instances() && generations.size() >= request.max_instances() &&
          !generations.contains(receiver))
        throw std::runtime_error("receiver admission limit reached");
      std::optional<std::int32_t> initial;
      if (!request.no_values() && !plan->layout.logical_collection())
        initial = read<std::int32_t>(pid, add(receiver, plan->layout.scalar));
      Lifetime life{receiver, ++next_id, ++generations[receiver], 0};
      live.emplace(receiver, life);
      emit(tid, Observation::CREATED, life, pc, regs, [&](auto &out) {
        out.set_field(plan->layout.union_tag_offset ? field_path() :
                      plan->layout.opaque_union ? "<storage>" : request.field());
        set_scalar_values(out, plan->layout, {}, {initial, union_tag(receiver)});
        if (plan->layout.union_tag_offset)
          out.set_synchronization(request.synchronization());
      }, &frame.entry_stack);
      break;
    }
    case Boundary::destroy:
      emit(tid, Observation::TEARDOWN_END, frame.life, pc, regs, [](auto &out) {
        out.set_closure_reason(Observation::DESTRUCTOR_RETURN);
      }, &frame.entry_stack);
      break;
    case Boundary::snapshot: {
      if (found == live.end() || found->second.id != frame.life.id || !rtti(receiver))
        throw std::runtime_error("snapshot receiver changed generation");
      const auto after = description(receiver);
      if (frame.description != after)
        emit(tid, Observation::SNAPSHOT_DELTA, found->second, pc, regs, [&](auto &out) {
          out.set_field(request.base_type() + "::" + request.snapshot_field());
          out.set_old_string(frame.description);
          out.set_new_string(after);
        }, &frame.entry_stack);
      break;
    }
    case Boundary::add: {
      const auto item = live.find(frame.incoming);
      const auto after = collection(receiver);
      const auto position = std::find(after.begin(), after.end(), frame.incoming_view);
      if (item == live.end() || position == after.end() ||
          std::count(after.begin(), after.end(), frame.incoming_view) != 1)
        throw std::runtime_error("add did not commit the verified ownership change");
      const auto index = position - after.begin();
      auto expected = after;
      expected.erase(expected.begin() + index);
      if (frame.prior != expected)
        throw std::runtime_error("add made an ambiguous change to the owner collection");
      if (const auto previous = item->second.owner) {
        const auto previous_items = collection(previous);
        if (previous == receiver ||
            std::find(previous_items.begin(), previous_items.end(), frame.incoming_view) != previous_items.end())
          throw std::runtime_error("previous owner still contains the object");
        emit(tid, Observation::OWNER_REMOVED, item->second, pc, regs, [](auto &out) {
          out.set_diagnostic("previous membership absent at transfer boundary");
        }, &frame.entry_stack);
      }
      item->second.owner = receiver;
      emit(tid, Observation::OWNER_ADDED, item->second, pc, regs,
           [&](auto &out) { out.set_owner_index(index); }, &frame.entry_stack);
      break;
    }
    case Boundary::get: {
      const auto complete = complete_from_base(object_target::result(regs));
      if (!complete)
        return;
      const auto item = live.find(*complete);
      if (item == live.end())
        return;
      const auto after = collection(receiver);
      const auto position = std::find(after.begin(), after.end(), object_target::result(regs));
      if (item->second.owner != receiver || position == after.end())
        throw std::runtime_error("get did not return a verified owned base reference");
      emit(tid, Observation::BORROW_RETURN, item->second, pc, regs,
           [&](auto &out) { out.set_owner_index(position - after.begin()); }, &frame.entry_stack);
      break;
    }
    case Boundary::remove: {
      if (!(object_target::result(regs) & 0xff))
        return;
      const auto after = collection(receiver);
      if (after.size() >= frame.prior.size() ||
          std::set<Address>(after.begin(), after.end()).size() != after.size() ||
          std::any_of(after.begin(), after.end(), [&](Address address) {
            return std::find(frame.prior.begin(), frame.prior.end(), address) == frame.prior.end();
          }))
        throw std::runtime_error("remove did not commit an unambiguous ownership change");
      for (const auto &[view, life] : frame.prior_watched) {
        if (std::find(after.begin(), after.end(), view) != after.end())
          continue;
        const auto prior = std::find(frame.prior.begin(), frame.prior.end(), view);
        if (prior == frame.prior.end())
          continue;
        const auto current = live.find(life.address);
        if (current != live.end() && current->second.id == life.id) {
          if (current->second.owner != receiver)
            continue; // The transfer was observed at its new owner boundary.
          current->second.owner = 0;
        }
        emit(tid, Observation::OWNER_REMOVED, life, pc, regs,
             [&](auto &out) { out.set_owner_index(prior - frame.prior.begin()); }, &frame.entry_stack);
      }
      break;
    }
    case Boundary::writer_scope:
      if (frame.provisional)
        emit(tid, Observation::OBSERVATION_ENDED, frame.life, pc, regs, [](auto &out) {
          out.set_closure_reason(Observation::METHOD_RETURN);
        }, &frame.entry_stack);
      break;
    case Boundary::collection_mutation: {
      const auto scope = std::find_if(task.frames.rbegin(), task.frames.rend(), [&](const auto &pending) {
        return pending.role == Boundary::writer_scope && pending.life.id == frame.life.id;
      });
      if (scope != task.frames.rend())
        bind_range(tid, *scope, pc, regs, frame.entry_stack);
      break;
    }
    }
  }

  struct Sample {
    Lifetime life;
    Address destination;
    const native_entry::ScalarStore &store;
    std::optional<std::int32_t> old;
    std::uint64_t storage_generation = 0;
    bool scoped = false;
    std::optional<std::uint32_t> old_tag = std::nullopt;
  };

  std::optional<Sample> sample(pid_t tid, Task &task, const Site &site, Address pc,
                               const Registers &regs) {
    if (!site.store || (request.writer_tid() && request.writer_tid() != static_cast<unsigned>(tid)))
      return std::nullopt;
    const auto &s = *site.store;
    const auto next = pc + site.bytes.size();
    const auto destination = reg(regs, s.base, next) + s.scale * reg(regs, s.index, next) +
                             static_cast<Address>(s.displacement);
    Sample pending{{}, destination, s, std::nullopt};
    if (plan->layout.logical_collection()) {
      for (auto frame = task.frames.rbegin(); frame != task.frames.rend(); ++frame) {
        if (frame->role != Boundary::writer_scope || !frame->range.valid)
          continue;
        try {
          const auto header = range_header(frame->receiver);
          if (header != frame->range.header ||
              (destination == frame->range.destination && range_element(header) != destination))
            throw std::runtime_error("logical container range changed outside a verified mutation return");
        } catch (const std::runtime_error &error) {
          frame->range.valid = false;
          gap(tid, pc, regs, error.what());
          continue;
        }
        if (destination == frame->range.destination) {
          pending.life = frame->life;
          pending.storage_generation = frame->range.generation;
          pending.scoped = true;
          break;
        }
      }
    } else if (destination >= plan->layout.scalar) {
      const auto address = destination - plan->layout.scalar;
      if (const auto item = live.find(address); item != live.end()) {
        pending.life = item->second;
      } else if (request.first_seen()) {
        const auto scope = std::find_if(task.frames.rbegin(), task.frames.rend(), [&](const auto &frame) {
          return frame.role == Boundary::writer_scope && frame.receiver == address && frame.provisional;
        });
        if (scope != task.frames.rend()) {
          pending.life = scope->life;
          pending.scoped = true;
        }
      }
    }
    if (!pending.life.id)
      return std::nullopt;
    if (!request.no_values() && !plan->layout.atomic)
      pending.old = read<std::int32_t>(pid, destination);
    pending.old_tag = union_tag(pending.life.address);
    return pending;
  }

  void complete(pid_t tid, const Sample &sample, Address pc,
                const Registers &before, const Registers &after) {
    using Kind = native_entry::ScalarStore::Kind;
    const auto found = live.find(sample.life.address);
    if (!sample.scoped && (found == live.end() || found->second.id != sample.life.id))
      return;
    const auto &store = sample.store;
    const auto expected = static_cast<std::uint32_t>(store.result_register.empty() ?
        object_target::result(before) : reg(before, store.result_register, 0));
    if (store.kind == Kind::compare_exchange) {
      const auto succeeded = store.result_register.empty() ? object_target::zero(after) :
          static_cast<std::uint32_t>(reg(after, store.result_register, 0)) == expected;
      if (!succeeded)
        return;
    }
    auto old = sample.old;
    std::optional<std::int32_t> value;
    std::optional<std::uint32_t> next_tag;
    if (!request.no_values()) {
      if (plan->layout.atomic) {
        auto input = store.immediate ? *store.immediate :
            static_cast<std::uint32_t>(reg(before, store.value_register, 0));
        if (store.kind == Kind::exchange_add) {
          const auto &result = store.result_register.empty() ? store.value_register : store.result_register;
          const auto returned = static_cast<std::uint32_t>(reg(after, result, 0));
          old = std::bit_cast<std::int32_t>(returned);
          input += returned;
        } else if (store.kind == Kind::compare_exchange) {
          old = std::bit_cast<std::int32_t>(expected);
        }
        value = std::bit_cast<std::int32_t>(input);
      } else {
        value = read<std::int32_t>(pid, sample.destination);
        next_tag = union_tag(sample.life.address);
        if (value == old && next_tag == sample.old_tag)
          return;
      }
    }
    emit(tid, request.no_values() ? Observation::FIELD_WRITTEN : Observation::FIELD_CHANGED,
         sample.life, pc, before, [&](auto &out) {
      out.set_field(field_path());
      out.set_storage_generation(sample.storage_generation);
      out.set_destination(sample.destination);
      set_scalar_values(out, plan->layout, {old, sample.old_tag}, {value, next_tag});
      if (plan->layout.union_tag_offset)
        out.set_synchronization(request.synchronization());
    });
  }

  bool needs_breakpoint(Address pc) const {
    return plan->sites.contains(pc) ||
        std::any_of(tasks.begin(), tasks.end(), [&](const auto &item) {
      const auto &task = item.second;
      return task.breakpoint_pc == pc ||
          std::any_of(task.frames.begin(), task.frames.end(),
                      [&](const auto &frame) { return frame.return_pc == pc; });
    });
  }

  // Peers are parked from the pre-sample until the original byte is re-armed.
  // Other threads already at a breakpoint keep their pending PC until serviced.
  void hit(pid_t tid) {
    auto &task = tasks.at(tid);
    const auto pc = *task.breakpoint_pc;
    const auto regs = registers(tid);
    rewind(tid, task, regs);
    const auto site = plan->sites.find(pc);
    auto &patch = patches.at(pc);
    const std::string_view expected = patch.bytes;
    verify_bytes(pc, expected, true);

    std::optional<Sample> observation;
    try {
      auto &frames = task.frames;
      while (!frames.empty()) {
        const auto &pending = frames.back();
        const auto sp = object_target::sp(regs);
        const bool new_call_at_same_depth = site != plan->sites.end() && site->second.boundary &&
            sp == pending.return_sp - object_target::return_stack_delta &&
            object_target::return_pc(regs, [&](Address address) { return read<Address>(pid, address); }) !=
                pending.return_pc;
        const bool epilogue = sp == pending.return_sp && pc != pending.return_pc &&
            pc >= pending.entry && pc - pending.entry < plan->boundaries.at(pending.entry);
        if ((sp < pending.return_sp || epilogue) && !new_call_at_same_depth)
          break;
        const bool matched = sp == pending.return_sp && pc == pending.return_pc;
        auto frame = std::move(frames.back());
        frames.pop_back();
        if (matched)
          boundary_return(tid, task, std::move(frame), regs);
        else
          close_unverified(tid, frame, Observation::UNWOUND);
      }
      if (site != plan->sites.end()) {
        if (site->second.boundary)
          entry(tid, task, *site->second.boundary, pc, regs);
        if (site->second.inline_mutation) {
          const auto &mutation = plan->inline_mutations.at(site->second.inline_mutation);
          const auto scope = std::find_if(task.frames.rbegin(), task.frames.rend(), [](const auto &frame) {
            return frame.role == Boundary::writer_scope;
          });
          if (scope != task.frames.rend()) {
            if (pc == mutation.begin && !scope->active_inline_mutation &&
                reg(regs, mutation.receiver_register, pc) + static_cast<Address>(mutation.receiver_offset) ==
                    add(scope->receiver, plan->layout.vector_object)) {
              scope->active_inline_mutation = mutation.begin;
              suspend_range(tid, *scope, pc, regs);
            } else if (pc != mutation.begin && scope->active_inline_mutation == mutation.begin) {
              scope->active_inline_mutation = 0;
              bind_range(tid, *scope, pc, regs, scope->entry_stack);
            }
          }
        }
        if (auto pending = sample(tid, task, site->second, pc, regs))
          observation.emplace(std::move(*pending));
      }
    } catch (const std::runtime_error &error) {
      gap(tid, pc, regs, error.what());
    }

    restore_trap(tid, pc, patch);
    patch.armed = false;
    const auto deadline = Clock::now() + timeout;
    for (;;) {
      resume(tid, tasks.at(tid), PTRACE_SINGLESTEP);
      await_stops(deadline);
      const auto current = tasks.find(tid);
      if (lifecycle_stop || current == tasks.end() ||
          current->second.stop.event() != PTRACE_EVENT_STOP ||
          current->second.stop.signal() != SIGTRAP || object_target::pc(registers(tid)) != pc)
        break;
      // An interrupt requested before a peer's breakpoint stop was reaped can
      // arrive before the step executes. Retry only at the unchanged PC.
      if (stopping)
        return;
    }
    if (lifecycle_stop)
      return;
    const auto stepped = tasks.find(tid);
    const bool trace_trap = stepped != tasks.end() && stepped->second.delivery == SIGTRAP &&
                            stepped->second.trap_code == TRAP_TRACE;
    if (trace_trap)
      stepped->second.delivery = 0; // Consume only our single-step trap.
    if (needs_breakpoint(pc)) {
      install(pc, expected);
    } else {
      patches.erase(pc);
    }
    if (observation) {
      const auto after = trace_trap ? registers(tid) : Registers{};
      if (trace_trap && object_target::pc(after) == pc + expected.size()) {
        try {
          complete(tid, *observation, pc, regs, after);
        } catch (const std::runtime_error &error) {
          gap(tid, pc, regs, error.what());
        }
      } else {
        const int signal = stepped == tasks.end() ? 0 : stepped->second.stop.signal();
        emit(tid, Observation::COVERAGE_GAP, observation->life, pc, regs, [&](auto &out) {
          out.set_location_kind(Observation::STORE);
          out.set_field(field_path());
          out.set_destination(observation->destination);
          out.set_storage_generation(observation->storage_generation);
          out.set_diagnostic("store did not reach a verified successor; signal " +
                             std::to_string(signal));
        });
      }
    }
  }

  void trace() {
    if (launch_argv.empty())
      seize_threads(Clock::now() + timeout);
    else
      launch_at_exec(Clock::now() + timeout);
    if (stopping)
      throw std::runtime_error("object watch startup cancelled");
    if (request.syscall_output())
      (void)syscall_info(pid);
    plan = std::make_unique<Plan>(pid, executable, request);
    plan->verify_live(pid);
    if (request.has_prepared_bindings())
      (void)compare_prepared(request.prepared_bindings(), plan->describe(true), request.no_values());
    std::vector<Address> ips;
    for (const auto &[tid, task] : tasks)
      ips.push_back(object_target::pc(registers(tid)));
    for (const auto &[pc, site] : plan->sites) {
      if (std::any_of(ips.begin(), ips.end(),
                      [&](Address ip) { return ip > pc && ip - pc < site.bytes.size(); }))
        throw std::runtime_error("target stopped inside an object watch instruction");
      verify_bytes(pc, site.bytes, false);
    }
    for (const auto &[pc, site] : plan->sites)
      install(pc, site.bytes);
    {
      std::lock_guard lock(mutex);
      if (stopping)
        throw std::runtime_error("object watch startup cancelled");
      report.set_active(true);
      armed_once = true;
      report.set_store_sites(plan->stores);
      report.set_unsupported_sites(plan->unsupported);
#if defined(__aarch64__)
      report.set_diagnostic(plan->layout.atomic
          ? "partial coverage: admitted threads, selected ELF function, atomic STR/STLR/LDADD/CAS"
          : "partial coverage: admitted threads, selected ELF function, scalar STR/STUR");
#else
      report.set_diagnostic(plan->layout.atomic
          ? "partial coverage: admitted threads, selected ELF function, atomic MOV/XADD/CMPXCHG"
          : "partial coverage: admitted threads, selected ELF function, scalar mov stores");
#endif
      ready = true;
    }
    changed.notify_all();
    const auto cutoff = request.timeout_seconds()
        ? Clock::now() + std::chrono::seconds(request.timeout_seconds()) : Clock::time_point::max();
    while (!stopping && !lifecycle_stop && !tasks.empty()) {
      if (Clock::now() >= cutoff) {
        std::lock_guard lock(mutex);
        if (!stopping) {
          stop_reason = Observation::WATCH_TIMEOUT;
          report.set_capture_limit("timeout");
          stopping = true;
        }
        break;
      }
      auto pending = std::find_if(tasks.begin(), tasks.end(), [](const auto &item) {
        return item.second.breakpoint_pc.has_value();
      });
      if (pending != tasks.end()) {
        const auto tid = pending->first;
        park_threads(Clock::now() + timeout);
        if (!stopping && !lifecycle_stop)
          hit(tid);
        continue;
      }
      for (auto &[tid, task] : tasks)
        if (task.stopped)
          resume(tid, task, task.listen ? PTRACE_LISTEN : PTRACE_CONT);
      if (receive() <= 0)
        pause(Clock::time_point::max());
    }
    if (lifecycle_stop && !stopping && (image_replaced || tasks.contains(pid)))
      gap(pid, 0, {}, "object watch ended at a thread/process lifecycle event");
    const auto reason = image_replaced ? Observation::MAPPING_CHANGED :
        !tasks.contains(pid) ? Observation::TARGET_EXITED :
        stopping ? stop_reason.load() : Observation::SCOPE_CHANGED;
    for (const auto &[tid, task] : tasks)
      for (const auto &frame : task.frames)
        close_unverified(tid, frame, reason);
    for (const auto &[address, life] : live)
      emit(pid, Observation::OBSERVATION_ENDED, life, 0, {}, [reason](auto &out) {
        out.set_closure_reason(reason);
      });
    live.clear();
  }

  void run() noexcept {
    std::string failure;
    try {
      trace();
    } catch (const std::exception &error) {
      failure = error.what();
      if (!ready && !launch_argv.empty() && tasks.contains(pid)) {
        ::kill(pid, SIGKILL);
        while (::waitpid(pid, nullptr, __WALL) == -1 && errno == EINTR) {}
        tasks.clear();
        patches.clear();
      }
      if (ready && plan) {
        try { gap(pid, 0, {}, failure); } catch (...) {}
      }
    }
    for (;;) {
      unsigned attempt;
      {
        std::lock_guard lock(mutex);
        attempt = retry;
      }
      try {
        restore();
        break;
      } catch (const std::exception &error) {
        std::unique_lock lock(mutex);
        report.set_active(false);
        report.set_restoration_pending(true);
        report.set_diagnostic("restoration pending: " + std::string(error.what()));
        failed_attempt = attempt;
        ready = true;
        changed.notify_all();
        changed.wait(lock, [&] { return abandon || retry != attempt; });
        if (abandon) {
          // Last resort on server shutdown. Keep an unsafe target stopped.
          if (!tasks.empty()) {
            try {
              if (proc::start_time(proc::directory(pid) / "stat") == process_start)
                (void)::kill(pid, SIGSTOP);
            } catch (const std::exception &) {
            }
            for (const auto &[tid, task] : tasks) {
              if (task.child)
                (void)::kill(tid, SIGSTOP);
              if (task.stopped)
                (void)::ptrace(PTRACE_DETACH, tid, nullptr, ptrace_arg(SIGSTOP));
            }
          }
          break;
        }
      }
    }
    std::lock_guard lock(mutex);
    if (!abandon)
      report.set_restoration_pending(false);
    report.set_active(false);
    if (!failure.empty() && !report.restoration_pending())
      report.set_diagnostic(failure);
    else if (!report.restoration_pending() && ready)
      report.set_diagnostic("original breakpoint bytes restored or target exited");
    ready = finished = true;
    changed.notify_all();
  }
};

Watch::Watch(const api::ObjectWatchRequest &request, std::uint64_t id,
             std::uint32_t pid, std::uint64_t process_start, std::string executable,
             std::chrono::seconds timeout, Publish publish)
    : worker_(std::make_unique<Worker>(request, id, pid, process_start,
                                      std::move(executable), timeout, std::move(publish))) {
  worker_->startup();
}

Watch::~Watch() = default;

Watch::Watch(const api::ObjectWatchRequest &request, std::uint64_t id,
             std::vector<std::string> argv, std::chrono::seconds timeout, Publish publish) {
  if (argv.empty() || argv.front().empty())
    throw std::invalid_argument("object launch requires an executable");
  const auto executable = std::filesystem::canonical(argv.front()).string();
  argv.front() = executable;
  worker_ = std::make_unique<Worker>(request, id, 0, 0, executable, timeout,
                                     std::move(publish), std::move(argv));
  worker_->startup();
}

api::ObjectWatchStatus Watch::status() const {
  std::lock_guard lock(worker_->mutex);
  return worker_->report;
}

api::ObjectWatchStatus Watch::stop(bool detaching) { return worker_->stop(detaching); }
std::uint32_t Watch::target_pid() const {
  const std::lock_guard lock(worker_->mutex);
  return worker_->ready ? worker_->pid : 0;
}
std::optional<int> Watch::target_exit_status() const {
  const std::lock_guard lock(worker_->mutex);
  return worker_->exit_status;
}
} // namespace tracer::object_watch

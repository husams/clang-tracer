#include "session.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>

#include <dlfcn.h>
#include <sys/types.h>

#include "injector.hpp"
#include "proc_identity.hpp"
#include "tracer/instruction_trace.h"

namespace tracer {
namespace api = clangtracer::v1;
namespace {

std::string gum_plugin(const std::string &agent_path) {
  const auto *override_path = std::getenv("CLANG_TRACER_GUM_PLUGIN");
  const auto path = override_path && *override_path
      ? std::filesystem::path(override_path)
      : std::filesystem::path(agent_path).parent_path() / "gum-backend.so";
  std::error_code error;
  const auto canonical = std::filesystem::canonical(path, error);
  if (!path.is_absolute() || error || !std::filesystem::is_regular_file(canonical, error))
    throw std::invalid_argument("install the supported gum-backend.so beside the agent or set CLANG_TRACER_GUM_PLUGIN");
  return canonical.string();
}

std::string callback_library(const std::string &value) {
  if (value.empty())
    return {};
  const std::filesystem::path path(value);
  std::error_code error;
  if (value.size() >= 4096 || value.find('\0') != std::string::npos || !path.is_absolute())
    throw std::invalid_argument("instruction callback library must be an absolute shared library path");
  const auto canonical = std::filesystem::canonical(path, error);
  if (error || !std::filesystem::is_regular_file(canonical, error))
    throw std::invalid_argument("instruction callback library must be a readable regular file");
  return canonical.string();
}

void require_unrestricted_ptrace() {
  std::ifstream policy("/proc/sys/kernel/yama/ptrace_scope");
  unsigned scope = 0;
  if (!(policy >> scope) || scope != 0)
    throw std::invalid_argument("instruction tracing requires kernel.yama.ptrace_scope=0");
}

void close_plugin(injector::Process &process, std::uintptr_t handle) {
  const auto dlclose = process.resolve_remote_symbol("dlclose");
  if (process.remote_call(dlclose, {handle}, 1) != 0)
    throw std::runtime_error("could not close an instruction tracing module in the target");
}

std::uint64_t thread_identity(const std::filesystem::path &stat) {
  try {
    return proc::start_time(stat);
  } catch (const std::runtime_error &) {
    throw std::invalid_argument("instruction thread is unavailable in the selected target");
  }
}

} // namespace

api::InstructionTrace Session::start_instruction_trace(
    const api::StartInstructionTraceRequest &request, const std::string &expected_code) {
  std::lock_guard lock(mutex_);
  check_identity_locked();
  if (state_ != api::Session::ATTACHED || unsafe_target_)
    throw std::invalid_argument("instruction tracing requires an attached session");
  if (instructions_ || page_monitor_)
    throw std::invalid_argument("stop the existing instruction capture before starting another");
  require_unrestricted_ptrace();
  const auto path = gum_plugin(agent_path_);
  const auto callback_path = callback_library(request.callback_library());
  if (request.discover_calls() && (request.registers() || !callback_path.empty() || request.memory_size()))
    throw std::invalid_argument("call discovery is a separate capture mode");
  const bool extended = request.registers() || !callback_path.empty() ||
                        request.memory_size() || request.discover_calls();
  const auto tid = static_cast<pid_t>(request.tid());
  const auto stat = proc::directory(pid_) / "task" / std::to_string(tid) / "stat";
  const auto thread_start = thread_identity(stat);
  const auto companions = injector::target_runtime_companions(
      static_cast<pid_t>(pid_), {"libdl.so.2", "libpthread.so.0", "libm.so.6", "libresolv.so.2"});
  auto scratch_size = std::max({path.size() + 1, callback_path.size() + 1, std::size_t{64}});
  for (const auto &companion : companions)
    scratch_size = std::max(scratch_size, companion.size() + 1);
  const auto deadline = std::chrono::steady_clock::now() + operation_timeout_;
  injector::Process process;
  std::uintptr_t scratch = 0;
  std::uintptr_t handle = 0;
  std::uintptr_t callback_handle = 0;
  std::vector<std::uintptr_t> companion_handles;
  companion_handles.reserve(companions.size());
  const auto release_companions = [&] {
    while (!companion_handles.empty()) {
      close_plugin(process, companion_handles.back());
      companion_handles.pop_back();
    }
  };
  try {
    attach_process(process, deadline, tid);
    scratch = process.remote_allocate(scratch_size);
    const auto dlopen = process.resolve_remote_symbol("dlopen");
    const auto dlsym = process.resolve_remote_symbol("dlsym");
    const auto open = [&](const std::string &module) {
      process.remote_write(scratch, module.c_str(), module.size() + 1);
      return process.remote_call(dlopen, {scratch, RTLD_NOW | RTLD_LOCAL}, 2);
    };
    for (const auto &companion : companions) {
      const auto loaded = open(companion);
      if (!loaded)
        throw std::runtime_error("cannot load target glibc companion " + companion);
      companion_handles.push_back(loaded);
    }
    handle = open(path);
    if (!handle)
      throw std::invalid_argument("target could not load the Gum module and its dependencies");
    const auto symbol = [&](std::uintptr_t module, const char *name) {
      process.remote_write(scratch, name, std::strlen(name) + 1);
      return process.remote_call(dlsym, {module, scratch}, 2);
    };
    const auto start = symbol(handle, request.discover_calls() ? "tracer_gum_instruction_start_v3" :
                                      extended ? "tracer_gum_instruction_start_v2"
                                                : "tracer_gum_instruction_start_v1");
    const auto stop = symbol(handle, extended ? "tracer_gum_instruction_stop_v2"
                                               : "tracer_gum_instruction_stop_v1");
    if (!start || !stop)
      throw std::invalid_argument("Gum module does not support the native instruction tracing ABI");
    std::uintptr_t callback = 0;
    if (!callback_path.empty()) {
      callback_handle = open(callback_path);
      if (!callback_handle ||
          !(callback = symbol(callback_handle, "tracer_instruction_callback_v1")))
        throw std::invalid_argument("target could not load tracer_instruction_callback_v1 from the library");
    }
    release_companions();
    std::string live(expected_code.size(), '\0');
    process.remote_read(request.address(), live.data(), live.size());
    if (live != expected_code || thread_identity(stat) != thread_start)
      throw std::invalid_argument("instruction range or thread changed during setup");
    api::InstructionTrace trace;
    trace.set_tid(request.tid());
    trace.set_address(request.address());
    trace.set_size(request.size());
    trace.set_discover_calls(request.discover_calls());
    const TracerInstructionRequestV2 options{
        request.address(), request.address() + request.size(), request.memory_address(),
        request.memory_address() + request.memory_size(), callback, request.tid(),
        request.max_records(), request.timeout_seconds(),
        request.discover_calls() ? std::uint32_t{TRACER_INSTRUCTION_DISCOVER_CALLS} :
        request.registers() ? std::uint32_t{TRACER_INSTRUCTION_REGISTERS} : 0};
    if (extended)
      process.remote_write(scratch, &options, sizeof(options));
    instructions_ = InstructionCapture{trace, request.max_records(), handle, stop, callback_handle,
                                       callback != 0, request.registers(),
                                       request.memory_address(), request.memory_size()};
    const auto status = static_cast<std::int32_t>(extended
        ? process.remote_call(start, {scratch}, 1)
        : process.remote_call(start, {options.tid, options.begin, options.end,
                                      options.limit, options.seconds}, 5));
    if (status != 0 && status != -2)
      instructions_.reset();
    if (status == -2)
      throw std::runtime_error("Gum startup did not finish; run %instructions stop to drain the capture");
    if (status != 0)
      throw std::invalid_argument("Gum could not follow the selected thread");
    last_instruction_trace_.reset();
    process.remote_free(scratch, scratch_size);
    scratch = 0;
    process.detach();
    return trace;
  } catch (...) {
    if (!process.unsafe()) {
      try { release_companions(); } catch (...) {}
    }
    if (!process.unsafe() && callback_handle && !instructions_) {
      try { close_plugin(process, callback_handle); } catch (...) {}
    }
    if (!process.unsafe() && handle && !instructions_) {
      try { close_plugin(process, handle); } catch (...) {}
    }
    if (!process.unsafe() && scratch) {
      try { process.remote_free(scratch, scratch_size); } catch (...) {}
    }
    if (process.unsafe()) {
      unsafe_target_ = true;
      set_failed_locked("Gum start recovery is uncertain; target remains stopped");
    }
    throw;
  }
}

api::InstructionTrace Session::stop_instruction_trace() {
  std::lock_guard lock(mutex_);
  check_identity_locked();
  if (!instructions_ && last_instruction_trace_ && state_ == api::Session::ATTACHED)
    return *last_instruction_trace_;
  return stop_instruction_trace_locked(std::chrono::steady_clock::now() + operation_timeout_);
}

api::InstructionTrace Session::stop_instruction_trace_locked(
    std::chrono::steady_clock::time_point deadline) {
  check_identity_locked();
  if (unsafe_target_)
    throw std::runtime_error(error_);
  if (!instructions_)
    throw std::invalid_argument("session has no instruction capture");
  auto &capture = *instructions_;
  auto result = capture.trace;
  TracerInstructionResultV1 collected{};
  const bool discover = result.discover_calls();
  const bool extended = capture.registers || capture.has_callback || capture.memory_size || discover;
  const auto result_size = extended ? sizeof(TracerInstructionResultV2) : sizeof(collected);
  injector::Process process;
  std::uintptr_t scratch = 0;
  try {
    attach_process(process, deadline, static_cast<pid_t>(result.tid()));
    scratch = process.remote_allocate(result_size);
    if (process.remote_call(capture.stop, {scratch}, 1) != 0)
      throw std::runtime_error("Gum has not drained; retry %instructions stop");
    process.remote_read(scratch, &collected, sizeof(collected));
    if (collected.count > capture.max_records ||
        collected.reason < TRACER_INSTRUCTION_STOPPED ||
        collected.reason > TRACER_INSTRUCTION_TIMEOUT)
      throw std::runtime_error("Gum returned an invalid instruction capture");
    result.set_reason(static_cast<api::InstructionTrace::Reason>(collected.reason));
    std::vector<TracerInstructionDetailV2> details(extended ? collected.count : 0);
    if (!details.empty())
      process.remote_read(scratch + offsetof(TracerInstructionResultV2, details),
                          details.data(), details.size() * sizeof(details.front()));
    for (std::uint32_t i = 0; i < collected.count; ++i) {
      if (collected.offsets[i] >= result.size())
        throw std::runtime_error("Gum returned a PC outside the requested range");
      const auto pc = result.address() + collected.offsets[i];
      result.add_pcs(pc);
      if (discover) {
        result.add_call_targets(details[i].call_target);
      } else if (extended) {
        const auto &detail = details[i];
        auto *output = result.add_details();
        if (capture.registers) {
          const auto &r = detail.registers;
          if (r.rip != pc)
            throw std::runtime_error("Gum register snapshot PC does not match its instruction");
          for (auto value : {r.rax, r.rbx, r.rcx, r.rdx, r.rsi, r.rdi, r.rbp, r.rsp,
                             r.r8, r.r9, r.r10, r.r11, r.r12, r.r13, r.r14, r.r15})
            output->add_registers(value);
        }
        if (capture.has_callback)
          output->set_callback_value(detail.callback_value);
        if (capture.memory_size) {
          if (!detail.memory_size || detail.memory_size > 8 ||
              detail.memory_access < 1 || detail.memory_access > 3 ||
              detail.memory_address >= capture.memory_address + capture.memory_size ||
              (detail.memory_address < capture.memory_address &&
               capture.memory_address - detail.memory_address >= detail.memory_size))
            throw std::runtime_error("Gum returned a memory access outside the requested range");
          output->set_memory_address(detail.memory_address);
          output->set_memory_size(detail.memory_size);
          output->set_memory_access(detail.memory_access);
        }
      }
    }
    process.remote_free(scratch, result_size);
    scratch = 0;
    if (capture.callback_handle) {
      close_plugin(process, capture.callback_handle);
      capture.callback_handle = 0;
    }
    close_plugin(process, capture.handle);
    instructions_.reset();
    last_instruction_trace_ = result;
    process.detach();
    return result;
  } catch (...) {
    if (!process.unsafe() && scratch) {
      try { process.remote_free(scratch, result_size); } catch (...) {}
    }
    if (process.unsafe()) {
      unsafe_target_ = true;
      set_failed_locked("Gum stop recovery is uncertain; target remains stopped");
    }
    throw;
  }
}
} // namespace tracer

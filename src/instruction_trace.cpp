#include "session.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

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

void require_unrestricted_ptrace() {
  std::ifstream policy("/proc/sys/kernel/yama/ptrace_scope");
  unsigned scope = 0;
  if (!(policy >> scope) || scope != 0)
    throw std::invalid_argument("instruction tracing requires kernel.yama.ptrace_scope=0");
}

void close_plugin(injector::Process &process, std::uintptr_t handle) {
  const auto dlclose = process.resolve_remote_symbol("dlclose");
  if (process.remote_call(dlclose, {handle}, 1) != 0)
    throw std::runtime_error("could not close the target Gum module");
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
  if (instructions_)
    throw std::invalid_argument("stop the existing instruction capture before starting another");
  require_unrestricted_ptrace();
  const auto path = gum_plugin(agent_path_);
  const auto tid = static_cast<pid_t>(request.tid());
  const auto stat = proc::directory(pid_) / "task" / std::to_string(tid) / "stat";
  const auto thread_start = thread_identity(stat);
  const auto scratch_size = std::max(path.size() + 1, std::size_t{64});
  const auto deadline = std::chrono::steady_clock::now() + operation_timeout_;
  injector::Process process;
  std::uintptr_t scratch = 0;
  std::uintptr_t handle = 0;
  try {
    attach_process(process, deadline, tid);
    scratch = process.remote_allocate(scratch_size);
    process.remote_write(scratch, path.c_str(), path.size() + 1);
    const auto dlopen = process.resolve_remote_symbol("dlopen");
    const auto dlsym = process.resolve_remote_symbol("dlsym");
    handle = process.remote_call(dlopen, {scratch, RTLD_NOW | RTLD_LOCAL}, 2);
    if (!handle)
      throw std::invalid_argument("target could not load the Gum module and its dependencies");
    const auto symbol = [&](const char *name) {
      process.remote_write(scratch, name, std::strlen(name) + 1);
      return process.remote_call(dlsym, {handle, scratch}, 2);
    };
    const auto start = symbol("tracer_gum_instruction_start_v1");
    const auto stop = symbol("tracer_gum_instruction_stop_v1");
    if (!start || !stop)
      throw std::invalid_argument("Gum module does not support the native instruction tracing ABI");
    std::string live(expected_code.size(), '\0');
    process.remote_read(request.address(), live.data(), live.size());
    if (live != expected_code || thread_identity(stat) != thread_start)
      throw std::invalid_argument("instruction range or thread changed during setup");
    api::InstructionTrace trace;
    trace.set_tid(request.tid());
    trace.set_address(request.address());
    trace.set_size(request.size());
    instructions_ = InstructionCapture{trace, request.max_records(), handle, stop};
    const auto status = static_cast<std::int32_t>(process.remote_call(
        start, {request.tid(), request.address(), request.address() + request.size(),
                request.max_records(), request.timeout_seconds()}, 5));
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
  injector::Process process;
  std::uintptr_t scratch = 0;
  try {
    attach_process(process, deadline, static_cast<pid_t>(result.tid()));
    scratch = process.remote_allocate(sizeof(collected));
    if (process.remote_call(capture.stop, {scratch}, 1) != 0)
      throw std::runtime_error("Gum has not drained; retry %instructions stop");
    process.remote_read(scratch, &collected, sizeof(collected));
    if (collected.count > capture.max_records ||
        collected.reason < TRACER_INSTRUCTION_STOPPED ||
        collected.reason > TRACER_INSTRUCTION_TIMEOUT)
      throw std::runtime_error("Gum returned an invalid instruction capture");
    result.set_reason(static_cast<api::InstructionTrace::Reason>(collected.reason));
    for (std::uint32_t i = 0; i < collected.count; ++i) {
      if (collected.offsets[i] >= result.size())
        throw std::runtime_error("Gum returned a PC outside the requested range");
      result.add_pcs(result.address() + collected.offsets[i]);
    }
    process.remote_free(scratch, sizeof(collected));
    scratch = 0;
    close_plugin(process, capture.handle);
    instructions_.reset();
    last_instruction_trace_ = result;
    process.detach();
    return result;
  } catch (...) {
    if (!process.unsafe() && scratch) {
      try { process.remote_free(scratch, sizeof(collected)); } catch (...) {}
    }
    if (process.unsafe()) {
      unsafe_target_ = true;
      set_failed_locked("Gum stop recovery is uncertain; target remains stopped");
    }
    throw;
  }
}
} // namespace tracer

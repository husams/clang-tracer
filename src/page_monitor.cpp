#include "session.hpp"

#include <filesystem>
#include <csignal>

#include "injector.hpp"
#include "proc_identity.hpp"
#include "tracer/page_monitor.h"

namespace tracer {
namespace api = clangtracer::v1;

api::PageMonitorResult Session::start_page_monitor(const api::StartPageMonitorRequest &request) {
  std::lock_guard lock(mutex_);
  check_identity_locked();
  if (state_ != api::Session::ATTACHED || unsafe_target_ || page_monitor_ || instructions_)
    throw std::invalid_argument("page monitoring requires an attached session with no live capture");
  if (!start_page_monitor_address_ || !stop_page_monitor_address_)
    throw std::invalid_argument("target agent does not support page monitoring");
  const auto tid = static_cast<pid_t>(request.tid());
  const auto task = proc::directory(pid_) / "task" / std::to_string(tid) / "stat";
  const auto identity = proc::start_time(task);
  injector::Process process;
  try {
    attach_process(process, std::chrono::steady_clock::now() + operation_timeout_, 0, tid);
    if (proc::start_time(task) != identity)
      throw std::invalid_argument("page monitor thread changed during setup");
    if (process.signal_blocked(SIGSEGV))
      throw std::invalid_argument("page monitor thread must have SIGSEGV unblocked");
    api::PageMonitorResult response;
    response.set_tid(tid);
    response.set_address(request.address());
    response.set_size(request.size());
    page_monitor_ = response;
    const auto status = static_cast<int>(process.remote_call(start_page_monitor_address_,
        {request.address(), request.size(), request.timeout_seconds()}, 3));
    if (status && status != -2)
      page_monitor_.reset();
    process.detach();
    if (status == -3)
      throw std::invalid_argument("page monitor requires the default SIGSEGV disposition");
    if (status)
      throw std::runtime_error(status == -2 ? "page setup incomplete; run %memory monitor-stop"
                                           : "agent rejected page monitoring setup");
    return response;
  } catch (...) {
    if (process.unsafe()) {
      unsafe_target_ = true;
      set_failed_locked("page monitor setup recovery is uncertain; target remains stopped");
    }
    throw;
  }
}

api::PageMonitorResult Session::stop_page_monitor() {
  std::lock_guard lock(mutex_);
  return stop_page_monitor_locked(std::chrono::steady_clock::now() + operation_timeout_);
}

api::PageMonitorResult Session::stop_page_monitor_locked(std::chrono::steady_clock::time_point deadline) {
  check_identity_locked();
  if (unsafe_target_ || !page_monitor_)
    throw std::invalid_argument("session has no stoppable page monitor");
  auto response = *page_monitor_;
  auto tid = static_cast<pid_t>(response.tid());
  std::error_code error;
  if (!std::filesystem::exists(proc::directory(pid_) / "task" / std::to_string(tid), error) && !error)
    tid = 0;
  injector::Process process;
  std::uintptr_t scratch = 0;
  TracerPageMonitorResult records{};
  try {
    attach_process(process, deadline, 0, tid);
    scratch = process.remote_allocate(sizeof(records));
    if (process.remote_call(stop_page_monitor_address_, {scratch}, 1) != 0)
      throw std::runtime_error("page monitor restoration failed; retry stop");
    page_monitor_.reset();
    process.remote_read(scratch, &records, sizeof(records));
    process.remote_free(scratch, sizeof(records));
    scratch = 0;
    process.detach();
    if (records.count > TRACER_PAGE_MONITOR_MAX_PAGES)
      throw std::runtime_error("invalid page monitor record count");
    response.set_expired(records.expired != 0);
    for (unsigned i = 0; i < records.count; ++i) {
      const auto &record = records.accesses[i];
      if (record.tid != response.tid() || record.address < response.address() ||
          record.address - response.address() >= response.size() ||
          (record.access != 1 && record.access != 2))
        throw std::runtime_error("invalid page monitor record");
      auto *access = response.add_accesses();
      access->set_address(record.address);
      access->set_pc(record.pc);
      access->set_write(record.access == 2);
    }
    return response;
  } catch (...) {
    if (scratch && !process.unsafe()) {
      try { process.remote_free(scratch, sizeof(records)); } catch (...) {}
    }
    if (process.unsafe()) {
      unsafe_target_ = true;
      set_failed_locked("page monitor stop recovery is uncertain; target remains stopped");
    }
    throw;
  }
}
} // namespace tracer

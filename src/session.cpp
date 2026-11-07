#include "session.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <llvm/ADT/StringExtras.h>
#include <llvm/Object/BuildID.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Error.h>

#include "agent_runtime_profile.hpp"
#include "injector.hpp"
#include "linux_syscalls.hpp"
#include "proc_identity.hpp"
#include "tracer/compat.hpp"

namespace tracer {
namespace {

std::atomic<std::uint64_t> validation_sequence{1};

template <std::size_t N>
void copy_field(char (&target)[N], const std::string &value,
                const char *label) {
  if (value.size() >= N || value.find('\0') != std::string::npos) {
    throw std::invalid_argument(std::string(label) +
                                " is too long or contains NUL");
  }
  std::memcpy(target, value.c_str(), value.size() + 1);
}

void write_file_0600(const std::string &path, const std::string &content) {
  const int fd =
      ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
  if (fd < 0) {
    throw std::runtime_error("cannot create private callback source: " +
                             std::string(std::strerror(errno)));
  }
  std::size_t offset = 0;
  while (offset < content.size()) {
    const auto count =
        ::write(fd, content.data() + offset, content.size() - offset);
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count <= 0) {
      const auto reason = std::string(std::strerror(errno));
      ::close(fd);
      ::unlink(path.c_str());
      throw std::runtime_error("cannot write private callback source: " +
                               reason);
    }
    offset += static_cast<std::size_t>(count);
  }
  if (::fsync(fd) != 0) {
    const auto reason = std::string(std::strerror(errno));
    ::close(fd);
    ::unlink(path.c_str());
    throw std::runtime_error("cannot flush private callback source: " + reason);
  }
  ::close(fd);
}

void copy_signature(const clangtracer::v1::Signature &signature,
                    Request &request) {
  request.kind = static_cast<CallableKind>(signature.kind());
  request.result = static_cast<ValueType>(signature.result());
  request.argument_count =
      static_cast<std::uint32_t>(signature.arguments_size());
  for (int i = 0; i < signature.arguments_size(); ++i) {
    request.arguments[i] = static_cast<ValueType>(signature.arguments(i));
  }
}

std::string configured_agent_build_id(const std::string &path) {
  auto binary = llvm::object::createBinary(path);
  if (!binary)
    throw AgentRuntimeIncompatible("cannot read configured agent ELF: " +
                                   llvm::toString(binary.takeError()));
  const auto *object =
      llvm::dyn_cast<llvm::object::ObjectFile>(binary->getBinary());
  const auto id = object ? llvm::object::getBuildID(object)
                         : llvm::ArrayRef<std::uint8_t>{};
  if (id.empty())
    throw AgentRuntimeIncompatible("configured agent has no ELF build ID");
  return llvm::toHex(id, /*LowerCase=*/true);
}

template <std::size_t N> std::string profile_text(const char (&value)[N]) {
  const auto *end = std::find(value, value + N, '\0');
  if (end == value + N)
    throw AgentRuntimeIncompatible(
        "loaded agent returned invalid runtime profile text");
  return {value, end};
}

clangtracer::v1::JitRuntimeProfile read_agent_profile(
    injector::Process &process, std::uintptr_t profile_entry,
    std::uintptr_t scratch, const std::string &expected_id) {
  if (!profile_entry)
    throw AgentRuntimeIncompatible(
        "loaded agent has no JIT runtime profile handshake; use a matching agent or restart the target");
  AgentRuntimeProfile profile{};
  if (process.remote_call(profile_entry, {scratch, sizeof(profile)}, 2) != 0)
    throw AgentRuntimeIncompatible(
        "loaded agent could not report its JIT runtime profile");
  process.remote_read(scratch, &profile, sizeof(profile));
  if (profile.bootstrap_protocol != kBootstrapVersion)
    throw AgentRuntimeIncompatible(
        "loaded agent bootstrap protocol " +
        std::to_string(profile.bootstrap_protocol) +
        " differs from server protocol " + std::to_string(kBootstrapVersion) +
        "; use a matching agent or restart the target");
  const auto build_id = llvm::toHex(llvm::ArrayRef(profile.build_id),
                                    /*LowerCase=*/true);
  if (build_id != expected_id)
    throw AgentRuntimeIncompatible(
        "loaded agent build " + build_id +
        " differs from configured agent build " +
        expected_id + "; restart the target to load the configured agent");
  if (profile.pointer_bytes != 8 || profile.libstdcxx_default_cxx11_abi < -1 ||
      profile.libstdcxx_default_cxx11_abi > 1)
    throw AgentRuntimeIncompatible(
        "loaded agent returned an invalid x86-64 runtime profile");

  clangtracer::v1::JitRuntimeProfile result;
  result.set_agent_build_id(build_id);
  result.set_bootstrap_protocol(profile.bootstrap_protocol);
  result.set_target_abi("linux-x86-64-sysv");
  result.set_clang_version(profile_text(profile.clang_version));
  result.set_llvm_version(profile_text(profile.llvm_version));
  result.set_glibc_version(profile_text(profile.glibc_version));
  result.set_cxx_runtime_path(profile_text(profile.cxx_runtime_path));
  result.set_libstdcxx_header_release(profile.libstdcxx_header_release);
  if (profile.libstdcxx_default_cxx11_abi >= 0)
    result.set_libstdcxx_default_cxx11_abi(
        profile.libstdcxx_default_cxx11_abi == 1);
  return result;
}

} // namespace

Session::Session(std::string id, std::uint32_t pid, std::uint64_t process_start,
                 std::string agent_path, std::string event_path,
                 std::string context_endpoint,
                 std::string context_durable_directory,
                 std::chrono::seconds operation_timeout,
                 ContextTransportBudget context_budget)
    : id_(std::move(id)), pid_(pid), process_start_(process_start),
      agent_path_(std::move(agent_path)), event_path_(std::move(event_path)),
      context_endpoint_(std::move(context_endpoint)),
      context_durable_directory_(std::move(context_durable_directory)),
      operation_timeout_(operation_timeout),
      context_budget_(context_budget) {}

std::uint64_t Session::ProcessStartTime(std::uint32_t pid) {
  return proc::start_time(proc::directory(pid) / "stat");
}

void Session::attach_process(injector::Process &process,
                             std::chrono::steady_clock::time_point deadline) {
  for (;;) {
    check_identity_locked();
    try {
      process.attach(static_cast<pid_t>(pid_), application_modules_);
      check_identity_locked();
      return;
    } catch (const injector::IdlePointUnavailable &) {
      if (std::chrono::steady_clock::now() >= deadline) {
        throw;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    } catch (...) {
      if (process.unsafe()) {
        unsafe_target_ = true;
        set_failed_locked("thread attachment recovery is uncertain; target "
                          "remains stopped");
      }
      throw;
    }
  }
}

void Session::check_identity_locked() {
  try {
    if (ProcessStartTime(pid_) != process_start_) {
      state_ = clangtracer::v1::Session::EXITED;
      application_modules_.clear();
      error_ = "target process exited; PID was reused";
      throw std::runtime_error(error_);
    }
  } catch (const std::runtime_error &error) {
    if (std::string(error.what()) == "target process is no longer available") {
      state_ = clangtracer::v1::Session::EXITED;
      application_modules_.clear();
      error_ = error.what();
    }
    throw;
  }
}

void Session::set_failed_locked(const std::string &message) {
  state_ = clangtracer::v1::Session::FAILED;
  error_ = message;
}

void Session::attach(const std::string &expected_executable) {
  std::lock_guard lock(mutex_);
  check_identity_locked();
  if (state_ != clangtracer::v1::Session::ATTACHED) {
    throw std::runtime_error("session is not attachable");
  }
  const auto expected_agent_id = configured_agent_build_id(agent_path_);
  injector::Process process;
  const auto deadline = std::chrono::steady_clock::now() + operation_timeout_;
  try {
    application_modules_ =
        injector::discover_application_modules(static_cast<pid_t>(pid_));
    discovered_module_paths_.clear();
    for (const auto &module : application_modules_)
      discovered_module_paths_.push_back(module.path());
    check_identity_locked();
    attach_process(process, deadline);
  } catch (const injector::IdlePointUnavailable &error) {
    throw injector::InitialAdmissionUnavailable(error.what());
  }
  std::error_code executable_error;
  executable_ = std::filesystem::read_symlink(proc::directory(pid_) / "exe",
                                             executable_error).string();
  if (executable_error ||
      (!expected_executable.empty() && executable_ != expected_executable)) {
    process.detach();
    throw injector::InitialAdmissionUnavailable(
        "target executable changed or became unavailable; refresh process selection");
  }
  std::uintptr_t scratch = 0;
  // Each synchronous loader call has finished using its string before the
  // scratch buffer is reused for another symbol name or the profile.
  const auto scratch_size =
      std::max(agent_path_.size() + 1, sizeof(AgentRuntimeProfile));
  try {
    scratch = process.remote_allocate(scratch_size);
    process.remote_write(scratch, agent_path_.c_str(), agent_path_.size() + 1);
    const auto dlopen_address = process.resolve_remote_symbol("dlopen");
    const auto dlsym_address = process.resolve_remote_symbol("dlsym");
    const auto handle = process.remote_call(
        dlopen_address, {scratch, RTLD_NOW | RTLD_GLOBAL}, 2);
    if (!handle) {
      throw std::runtime_error(
          "target dlopen failed to load the agent and its dependencies");
    }
    const auto agent_symbol = [&](const char *name) {
      process.remote_write(scratch, name, std::strlen(name) + 1);
      return process.remote_call(dlsym_address, {handle, scratch}, 2);
    };
    const auto profile_entry = agent_symbol("tracer_agent_runtime_profile_v1");
    try {
      jit_runtime_ =
          read_agent_profile(process, profile_entry, scratch, expected_agent_id);
    } catch (const AgentRuntimeIncompatible &) {
      // The handshake does not install hooks. Return a definitive failure
      // only after scratch memory and ptrace ownership have been released.
      process.remote_free(scratch, scratch_size);
      scratch = 0;
      process.detach();
      throw;
    }
    const auto start = agent_symbol("tracer_agent_start");
    const auto stop = agent_symbol("tracer_agent_stop");
    const auto validate = agent_symbol("tracer_agent_validate");
    const auto capacity = agent_symbol("tracer_agent_capacity_v1");
    const auto pthread_detour =
        agent_symbol("tracer_context_pthread_create_detour");
    if (!start || !stop) {
      throw std::runtime_error(
          "native agent start/stop exports were not found");
    }
    process.remote_free(scratch, scratch_size);
    scratch = 0;
    process.detach();
    start_address_ = start;
    stop_address_ = stop;
    validate_address_ = validate;
    capacity_address_ = capacity;
    pthread_detour_address_ = pthread_detour;
  } catch (...) {
    if (process.unsafe()) {
      unsafe_target_ = true;
      set_failed_locked("agent bootstrap recovery is uncertain; target "
                        "remains stopped");
    }
    if (scratch) {
      try {
        process.remote_free(scratch, scratch_size);
      } catch (...) {
      }
    }
    throw;
  }
}

clangtracer::v1::ValidationReport
Session::validate_probe(const clangtracer::v1::ProbeSpec &spec,
                        const std::string &source_directory,
                        clangtracer::v1::ValidateProbeRequest::Level level) {
  std::lock_guard lock(mutex_);
  check_identity_locked();
  if (state_ != clangtracer::v1::Session::ATTACHED) {
    throw std::runtime_error("session is not attached");
  }
  if (!validate_address_) {
    throw std::runtime_error("loaded native agent does not support validation");
  }
  const auto sequence = validation_sequence.fetch_add(1);
  const auto stem = source_directory + "/" + id_ + "-validate-" +
                    std::to_string(sequence);
  const auto spec_path = stem + ".pb";
  const auto report_path = stem + ".report.pb";
  write_file_0600(spec_path, spec.SerializeAsString());
  struct RemoveFiles {
    std::string first;
    std::string second;
    ~RemoveFiles() {
      ::unlink(first.c_str());
      ::unlink(second.c_str());
    }
  } remove_files{spec_path, report_path};

  injector::Process process;
  const auto deadline = std::chrono::steady_clock::now() + operation_timeout_;
  std::uintptr_t remote_request = 0;
  try {
    attach_process(process, deadline);
    remote_request = process.remote_allocate(sizeof(ValidationRequest));
    ValidationRequest request{};
    request.level = static_cast<std::uint32_t>(level);
    copy_field(request.spec_path, spec_path, "validation spec path");
    copy_field(request.report_path, report_path, "validation report path");
    process.remote_write(remote_request, &request, sizeof(request));
    const auto status = process.remote_call(validate_address_, {remote_request}, 1);
    process.remote_read(remote_request, &request, sizeof(request));
    process.remote_free(remote_request, sizeof(ValidationRequest));
    remote_request = 0;
    process.detach();
    if (status != 0) {
      throw std::runtime_error(std::string("agent validation failed: ") +
                               request.error);
    }
    std::ifstream report_input(report_path, std::ios::binary);
    clangtracer::v1::ValidationReport report;
    if (!report_input || !report.ParseFromIstream(&report_input)) {
      throw std::runtime_error("agent returned an invalid validation report");
    }
    return report;
  } catch (...) {
    if (remote_request) {
      try {
        process.remote_free(remote_request, sizeof(ValidationRequest));
      } catch (...) {
      }
    }
    if (process.unsafe()) {
      unsafe_target_ = true;
      set_failed_locked("validation recovery is uncertain; target remains stopped");
    }
    throw;
  }
}

Session::ProbeRecord &Session::find_probe_locked(std::uint64_t probe_id) {
  const auto found = probes_.find(probe_id);
  if (found == probes_.end()) {
    throw std::invalid_argument("probe does not exist in this session");
  }
  return found->second;
}

clangtracer::v1::Probe
Session::add_probe(const clangtracer::v1::ProbeSpec &spec,
                   const std::string &source_directory,
                   std::uint64_t probe_id,
                   const std::array<std::uint8_t, 16> &lineage,
                   const std::string &bulk_batch_id, std::uint64_t reload_probe_id,
                   const std::function<bool()> &cancelled) {
  std::lock_guard lock(mutex_);
  check_identity_locked();
  if (state_ != clangtracer::v1::Session::ATTACHED) {
    throw std::runtime_error("session is not attached");
  }
  if (probes_.size() >= kMaxProbes) {
    throw std::invalid_argument("session probe limit reached");
  }
  if (probe_id == 0 || probes_.count(probe_id) != 0) {
    throw std::invalid_argument("probe id is invalid or already used");
  }
  if (cancelled && cancelled())
    throw std::runtime_error("reload cancelled before preparing the replacement");
  const auto batch_id = reload_probe_id ? find_probe_locked(reload_probe_id).value.bulk_batch_id()
                                        : bulk_batch_id;
  const auto source_file =
      source_directory + "/" + id_ + "-" + std::to_string(probe_id) + ".pb";
  write_file_0600(source_file, spec.SerializeAsString());
  injector::Process process;
  const auto deadline = std::chrono::steady_clock::now() + operation_timeout_;
  std::uintptr_t remote_request = 0;
  bool prepared = false;
  bool cleanup_attempted = false;
  const auto discard_prepared = [&] {
    // The prepared wrapper is unreachable. Deactivate it before releasing
    // the request record and resuming the process.
    cleanup_attempted = true;
    if (!process.attached())
      attach_process(process, std::chrono::steady_clock::now() + operation_timeout_);
    constexpr unsigned cleanup_attempts = 12;
    for (unsigned attempt = 0; attempt < cleanup_attempts; ++attempt) {
      std::int32_t cleanup_status = 0;
      try {
        cleanup_status = static_cast<std::int32_t>(
            process.remote_call(stop_address_, {remote_request}, 1));
      } catch (const std::exception &error) {
        process.mark_unsafe();
        unsafe_target_ = true;
        set_failed_locked(std::string("prepared probe cleanup outcome is "
                                      "uncertain; target remains stopped: ") +
                          error.what());
        throw std::runtime_error(error_);
      } catch (...) {
        process.mark_unsafe();
        unsafe_target_ = true;
        set_failed_locked("prepared probe cleanup outcome is uncertain; "
                          "target remains stopped");
        throw std::runtime_error(error_);
      }
      if (cleanup_status == 0)
        break;
      if (cleanup_status != 1 || attempt + 1 == cleanup_attempts) {
        process.mark_unsafe();
        unsafe_target_ = true;
        set_failed_locked("could not deactivate prepared probe; target "
                          "remains stopped");
        throw std::runtime_error(error_);
      }
    }
    process.remote_free(remote_request, sizeof(Request));
    remote_request = 0;
    probes_.erase(probe_id);
    prepared = false;
  };
  try {
    attach_process(process, deadline);
    remote_request = process.remote_allocate(sizeof(Request));
    Request request{};
    request.probe_id = probe_id;
    request.reject_replacement = !reload_probe_id && !batch_id.empty();
    request.expected_replaced_probe_id = reload_probe_id;
    copy_field(request.session_id, id_, "session id");
    request.context_native_pid = pid_;
    request.context_budget = context_budget_;
    request.context_process_start_time = process_start_;
    copy_field(request.symbol, spec.symbol(), "symbol");
    copy_field(request.source_path, source_file, "callback source path");
    copy_field(request.event_socket, event_path_, "event socket path");
    copy_field(request.context_endpoint, context_endpoint_,
               "shared-context endpoint");
    if (context_durable_directory_.empty()) {
      request.context_delivery_profile = 1;
    } else {
      request.context_delivery_profile = 2;
      const auto durable_id = "pid-" + std::to_string(pid_) + "-" +
                              std::to_string(process_start_);
      std::filesystem::create_directories(context_durable_directory_);
      if (::chmod(context_durable_directory_.c_str(), 0700) != 0) {
        throw std::runtime_error("cannot secure shared-context journal directory");
      }
      copy_field(request.context_journal_dir, context_durable_directory_,
                 "shared-context journal directory");
      copy_field(request.context_recording_id, durable_id,
                 "shared-context recording id");
      copy_field(request.context_sink_id, "default",
                 "shared-context sink id");
    }
    std::copy(lineage.begin(), lineage.end(), request.source_lineage_id);
    copy_signature(spec.signature(), request);

    ProbeRecord record;
    record.remote_request = remote_request;
    record.source_file = source_file;
    record.value.set_id(probe_id);
    record.value.set_session_id(id_);
    record.value.set_symbol(spec.symbol());
    record.value.set_bulk_batch_id(batch_id);
    record.value.mutable_signature()->CopyFrom(spec.signature());
    record.reload_spec.CopyFrom(spec);
    record.reload_spec.clear_callback_source();
    auto [it, inserted] = probes_.emplace(probe_id, std::move(record));
    if (!inserted)
      throw std::invalid_argument("probe id is already in use");

    process.remote_write(remote_request, &request, sizeof(request));
    std::uintptr_t result = 0;
    try {
      result = process.remote_call(start_address_, {remote_request}, 1);
    } catch (const std::exception &error) {
      // The agent may have completed preparation before the injector lost
      // the return value. Keep the request and registry entry for recovery.
      it->second.value.set_active(true);
      prepared = true;
      ::unlink(it->second.source_file.c_str());
      it->second.source_file.clear();
      process.mark_unsafe();
      unsafe_target_ = true;
      set_failed_locked(std::string("agent prepare outcome is uncertain; "
                                    "target remains stopped: ") +
                        error.what());
      throw std::runtime_error(error_);
    } catch (...) {
      it->second.value.set_active(true);
      prepared = true;
      ::unlink(it->second.source_file.c_str());
      it->second.source_file.clear();
      process.mark_unsafe();
      unsafe_target_ = true;
      set_failed_locked("agent prepare outcome is uncertain; target remains "
                        "stopped");
      throw std::runtime_error(error_);
    }
    const auto status = static_cast<std::int32_t>(result);
    if (status != 0) {
      Request response{};
      try {
        process.remote_read(remote_request, &response, sizeof(response));
      } catch (...) {
      }
      if (status != -1) {
        it->second.value.set_active(true);
        process.mark_unsafe();
        unsafe_target_ = true;
        set_failed_locked("agent prepare outcome is uncertain; target "
                          "remains stopped");
        throw std::runtime_error(error_);
      }
      process.remote_free(remote_request, sizeof(Request));
      remote_request = 0;
      probes_.erase(it);
      ::unlink(source_file.c_str());
      throw std::runtime_error(std::string("agent rejected probe: ") +
                               response.error);
    }

    // Preparation publishes the immutable probe, but leaves entry text alone.
    it->second.value.set_active(true);
    prepared = true;
    ::unlink(it->second.source_file.c_str());
    it->second.source_file.clear();
    Request response{};
    process.remote_read(remote_request, &response, sizeof(response));
    it->second.value.set_callback_address(response.callback_address);
    it->second.value.set_entry_address(response.original_entry);
    if (!response.patch_size || response.patch_size > kMaxPatchBytes) {
      throw std::runtime_error("agent returned invalid patch metadata");
    }
    if (!std::memcmp(response.original, response.installed,
                     response.patch_size))
      throw std::runtime_error("agent returned identical patch byte sequences");

    std::optional<PthreadCreatePatch> scheduler_patch;
    if (response.pthread_create_patch_size > kMaxPatchBytes) {
      throw std::runtime_error("agent returned invalid pthread_create patch metadata");
    }
    if (response.pthread_create_patch_size != 0) {
      if (!response.pthread_create_entry || !response.pthread_create_trampoline ||
          !response.pthread_create_wrapper ||
          response.pthread_create_patch_size < 14 ||
          std::memcmp(response.pthread_create_original,
                      response.pthread_create_installed,
                      response.pthread_create_patch_size) == 0) {
        throw std::runtime_error("agent returned incomplete pthread_create patch metadata");
      }
      PthreadCreatePatch candidate;
      candidate.entry = response.pthread_create_entry;
      candidate.trampoline = response.pthread_create_trampoline;
      candidate.wrapper = response.pthread_create_wrapper;
      candidate.size = response.pthread_create_patch_size;
      std::copy_n(response.pthread_create_original, candidate.size,
                  candidate.original.begin());
      std::copy_n(response.pthread_create_installed, candidate.size,
                  candidate.installed.begin());
      injector::CodePatch owned_scheduler_patch{};
      if (pthread_create_patch_committed_ && pthread_create_patch_) {
        const auto& owned = *pthread_create_patch_;
        owned_scheduler_patch = {owned.entry, owned.original.data(), owned.installed.data(),
                                 owned.size};
      }
      const auto proven_entry = process.resolve_remote_symbol(
          "pthread_create", owned_scheduler_patch.size ? &owned_scheduler_patch : nullptr);
      bool proven_patch_entry = candidate.entry == proven_entry;
      if (!proven_patch_entry &&
          proven_entry <= std::numeric_limits<std::uintptr_t>::max() - 4 &&
          candidate.entry == proven_entry + 4) {
        std::array<unsigned char, 4> landing_pad{};
        process.remote_read(proven_entry, landing_pad.data(), landing_pad.size());
        constexpr std::array<unsigned char, 4> kEndbr64{0xf3, 0x0f, 0x1e, 0xfa};
        proven_patch_entry = landing_pad == kEndbr64;
      }
      if (!proven_patch_entry ||
          candidate.wrapper != pthread_detour_address_) {
        throw std::runtime_error(
            "agent pthread_create detour does not match the proven libc symbol and wrapper");
      }
      scheduler_patch = candidate;
      if (pthread_create_patch_committed_ &&
          (!pthread_create_patch_ ||
           pthread_create_patch_->entry != candidate.entry ||
           pthread_create_patch_->trampoline != candidate.trampoline ||
           pthread_create_patch_->wrapper != candidate.wrapper ||
           pthread_create_patch_->size != candidate.size ||
           pthread_create_patch_->original != candidate.original ||
           pthread_create_patch_->installed != candidate.installed)) {
        throw std::runtime_error(
            "agent changed the active pthread_create detour metadata");
      }
    } else if (response.context_required) {
      throw std::runtime_error(
          "shared-context activation requires a proven pthread_create detour");
    }

    ProbeRecord *previous = nullptr;
    Request previous_patch{};
    if (reload_probe_id && response.replaced_probe_id != reload_probe_id)
      throw std::runtime_error("agent returned the wrong reload generation");
    if (response.replaced_probe_id) {
      if (request.reject_replacement)
        throw std::runtime_error("bulk installation cannot replace an existing probe");
      previous = &find_probe_locked(response.replaced_probe_id);
      if (!previous->value.active() || !previous->patch_committed ||
          previous->value.entry_address() != response.original_entry)
        throw std::runtime_error("agent returned an invalid replacement probe");
      process.remote_read(previous->remote_request, &previous_patch, sizeof(previous_patch));
      if (previous_patch.probe_id != previous->value.id() ||
          previous_patch.original_entry != response.original_entry ||
          previous_patch.patch_size != response.patch_size ||
          std::memcmp(previous_patch.original, response.original, response.patch_size) != 0)
        throw std::runtime_error("agent returned inconsistent replacement patch metadata");
    }

    const auto patch_started = std::chrono::steady_clock::now();
    constexpr unsigned kPatchAttempts = 12;
    bool committed = false;
    unsigned patch_attempts = 0;
    std::exception_ptr switch_error;
    for (unsigned attempt = 0; attempt < kPatchAttempts; ++attempt) {
      if (cancelled && cancelled())
        throw std::runtime_error("reload cancelled before switching the active observer");
      if (std::chrono::steady_clock::now() >= deadline)
        break;
      ++patch_attempts;
      bool patch_applied = false;
      try {
        const injector::CodePatch target_patch{
            response.original_entry, previous ? previous_patch.installed : response.original,
            response.installed,
            response.patch_size};
        if (scheduler_patch && !pthread_create_patch_committed_) {
          const injector::CodePatch task_patch{
              scheduler_patch->entry, scheduler_patch->original.data(),
              scheduler_patch->installed.data(), scheduler_patch->size};
          const std::array<injector::CodePatch, 2> patch_set{
              target_patch, task_patch};
          patch_applied = process.patch_code_batch(patch_set);
        } else {
          patch_applied = process.patch_code_batch(
              ::tracer::compat::span<const injector::CodePatch>(&target_patch, 1));
        }
      } catch (const std::exception &error) {
        if (previous && !process.unsafe()) {
          switch_error = std::current_exception();
          break;
        }
        // A failed peer release can report an error after the bytes changed.
        // Preserve enough state to identify the probe and leave recovery to a
        // caller that can inspect the parked process.
        it->second.patch_committed = true;
        it->second.value.set_active(true);
        if (scheduler_patch && !pthread_create_patch_committed_) {
          pthread_create_patch_ = scheduler_patch;
          pthread_create_patch_committed_ = true;
        }
        process.mark_unsafe();
        unsafe_target_ = true;
        set_failed_locked(std::string("entry patch outcome is uncertain; "
                                      "target remains stopped: ") +
                          error.what());
        throw std::runtime_error(error_);
      } catch (...) {
        if (previous && !process.unsafe()) {
          switch_error = std::current_exception();
          break;
        }
        it->second.patch_committed = true;
        it->second.value.set_active(true);
        if (scheduler_patch && !pthread_create_patch_committed_) {
          pthread_create_patch_ = scheduler_patch;
          pthread_create_patch_committed_ = true;
        }
        process.mark_unsafe();
        unsafe_target_ = true;
        set_failed_locked("entry patch outcome is uncertain; target remains "
                          "stopped");
        throw std::runtime_error(error_);
      }
      if (patch_applied) {
        committed = true;
        break;
      }
      process.detach();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      attach_process(process, deadline);
    }
    if (!committed) {
      discard_prepared();
      process.detach();
      if (switch_error)
        std::rethrow_exception(switch_error);
      if (!patch_attempts)
        throw std::runtime_error("operation deadline expired before the first entry patch attempt; increase --timeout");
      throw std::runtime_error("entry remained busy; probe was not committed");
    }
    it->second.patch_committed = true;
    if (previous) {
      // The entry now admits only the new wrapper. Calls already routed
      // through the old wrapper keep its callbacks and event transport.
      previous->patch_committed = false;
      previous->value.set_active(false);
      previous->value.set_draining(true);
      previous->value.set_superseded_by_probe_id(probe_id);
      it->second.value.set_replaces_probe_id(previous->value.id());
    }
    it->second.value.set_jit_compile_ns(response.jit_compile_ns);
    it->second.value.set_patch_install_ns(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - patch_started).count());
    if (scheduler_patch && !pthread_create_patch_committed_) {
      pthread_create_patch_ = scheduler_patch;
      pthread_create_patch_committed_ = true;
    }
    it->second.value.set_original_bytes_restored(false);
    it->second.value.set_agent_dropped_events(response.event_drops);
    it->second.value.set_callback_errors(response.callback_errors);
    try {
      process.detach();
    } catch (const std::exception &error) {
      process.mark_unsafe();
      unsafe_target_ = true;
      set_failed_locked(
          std::string("probe committed but target detach failed: ") +
          error.what());
      throw std::runtime_error(error_);
    }
    return it->second.value;
  } catch (...) {
    if (process.unsafe()) {
      unsafe_target_ = true;
      set_failed_locked("probe preparation recovery is uncertain; target "
                        "remains stopped");
    }
    if (reload_probe_id && prepared && !cleanup_attempted && !unsafe_target_ &&
        !find_probe_locked(probe_id).patch_committed) {
      try {
        discard_prepared();
      } catch (...) {
        // Leave the retained request available for recovery.
      }
    }
    const auto found = probes_.find(probe_id);
    if (found != probes_.end() && !prepared && !unsafe_target_) {
      if (remote_request) {
        try {
          process.remote_free(remote_request, sizeof(Request));
        } catch (...) {
        }
      }
      probes_.erase(found);
    }
    if (!prepared)
      ::unlink(source_file.c_str());
    else if (found != probes_.end() && !unsafe_target_ &&
             state_ != clangtracer::v1::Session::EXITED &&
             !found->second.patch_committed) {
      // Metadata or commit failed after preparation. Leave the immutable
      // retained probe discoverable for a later safe remove.
      set_failed_locked("prepared probe could not be committed safely");
    }
    if (unsafe_target_ && found != probes_.end()) {
      ::unlink(found->second.source_file.c_str());
      found->second.source_file.clear();
    }
    if (!unsafe_target_ && process.attached()) {
      try {
        process.detach();
      } catch (const std::exception &error) {
        process.mark_unsafe();
        unsafe_target_ = true;
        set_failed_locked(
            std::string("could not resume target after prepare: ") +
            error.what());
      }
    }
    throw;
  }
}

clangtracer::v1::ProbeSpec Session::reload_spec(std::uint64_t probe_id, std::string source) {
  std::lock_guard lock(mutex_);
  const auto &probe = find_probe_locked(probe_id);
  if (state_ != clangtracer::v1::Session::ATTACHED || !probe.value.active() ||
      !probe.patch_committed)
    throw std::runtime_error("reload requires the active probe that owns the entry");
  auto spec = probe.reload_spec;
  spec.set_callback_source(std::move(source));
  return spec;
}

bool Session::stop_probe_locked(
    ProbeRecord &probe, bool shutdown_mode,
    std::chrono::steady_clock::time_point shutdown_deadline) {
  if (!probe.value.active() && !probe.value.draining()) {
    return true;
  }
  const auto operation_deadline =
      std::chrono::steady_clock::now() + operation_timeout_;
  const auto deadline = std::min(operation_deadline, shutdown_deadline);
  const bool last_active_probe = std::none_of(
      probes_.begin(), probes_.end(), [&](const auto &entry) {
        return entry.first != probe.value.id() &&
               (entry.second.value.active() || entry.second.value.draining());
      });
  for (;;) {
    if (std::chrono::steady_clock::now() >= deadline) {
      const std::string message = "probe cleanup reached shutdown deadline";
      if (!shutdown_mode)
        set_failed_locked(message);
      throw std::runtime_error(message);
    }
    check_identity_locked();
    injector::Process process;
    attach_process(process, deadline);
    if (probe.patch_committed) {
      Request patch{};
      try {
        process.remote_read(probe.remote_request, &patch, sizeof(patch));
      } catch (const std::exception &error) {
        process.mark_unsafe();
        unsafe_target_ = true;
        set_failed_locked(std::string("cannot read prepared patch metadata: ") +
                          error.what());
        throw std::runtime_error(error_);
      }
      bool restored = false;
      constexpr unsigned kPatchAttempts = 12;
      for (unsigned attempt = 0; attempt < kPatchAttempts; ++attempt) {
        if (std::chrono::steady_clock::now() >= deadline)
          break;
        try {
          if (process.patch_code(patch.original_entry, patch.installed,
                                 patch.original, patch.patch_size)) {
            restored = true;
            break;
          }
        } catch (const std::exception &error) {
          process.mark_unsafe();
          unsafe_target_ = true;
          set_failed_locked(
              std::string("entry restore outcome is uncertain: ") +
              error.what());
          throw std::runtime_error(error_);
        }
        process.detach();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        attach_process(process, deadline);
      }
      if (!restored) {
        if (!shutdown_mode)
          set_failed_locked("entry remained busy; original bytes were not "
                            "restored");
        try {
          process.detach();
        } catch (const std::exception &error) {
          process.mark_unsafe();
          unsafe_target_ = true;
          set_failed_locked(std::string("could not resume target after busy "
                                        "entry check: ") +
                            error.what());
          throw std::runtime_error(error_);
        }
        throw std::runtime_error("entry remained busy; removal can be retried");
      }
      probe.patch_committed = false;
    }
    std::uintptr_t result = 0;
    try {
      result = process.remote_call(stop_address_, {probe.remote_request}, 1);
    } catch (const std::exception &error) {
      process.mark_unsafe();
      unsafe_target_ = true;
      set_failed_locked(
          std::string(
              "probe stop outcome is uncertain; target remains stopped: ") +
          error.what());
      throw std::runtime_error(error_);
    }
    const auto status = static_cast<std::int32_t>(result);
    Request response{};
    try {
      process.remote_read(probe.remote_request, &response, sizeof(response));
    } catch (const std::exception &error) {
      process.mark_unsafe();
      unsafe_target_ = true;
      set_failed_locked(
          std::string("cannot read stop result; target remains stopped: ") +
          error.what());
      throw std::runtime_error(error_);
    }
    if (status == 1) {
      try {
        process.detach();
      } catch (const std::exception &error) {
        process.mark_unsafe();
        unsafe_target_ = true;
        set_failed_locked(std::string("could not resume target while waiting "
                                      "for callback drain: ") +
                          error.what());
        throw std::runtime_error(error_);
      }
      if (std::chrono::steady_clock::now() >= deadline) {
        const std::string message =
            "callback remained active until stop timeout";
        if (!shutdown_mode)
          set_failed_locked(message);
        throw std::runtime_error(message);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }
    if (status == -1) {
      try {
        process.detach();
      } catch (const std::exception &error) {
        process.mark_unsafe();
        unsafe_target_ = true;
        set_failed_locked(std::string("could not resume target after rejected "
                                      "probe stop: ") +
                          error.what());
        throw std::runtime_error(error_);
      }
      set_failed_locked(std::string("agent rejected probe stop: ") +
                        response.error);
      throw std::runtime_error(error_);
    }
    if (status == -2 || status != 0) {
      process.mark_unsafe();
      unsafe_target_ = true;
      const std::string message =
          status == -2
              ? std::string("agent could not restore the original patch: ") +
                    response.error
              : std::string("agent stop failed; target remains stopped: ") +
                    response.error;
      set_failed_locked(message);
      throw std::runtime_error(message);
    }
    if (last_active_probe && pthread_create_patch_committed_ &&
        response.pthread_create_active_tasks != 0) {
      try {
        process.detach();
      } catch (const std::exception &error) {
        process.mark_unsafe();
        unsafe_target_ = true;
        set_failed_locked(std::string("could not resume target while waiting "
                                      "for pthread task completion: ") +
                          error.what());
        throw std::runtime_error(error_);
      }
      if (std::chrono::steady_clock::now() >= deadline) {
        const std::string message =
            "accepted pthread tasks remained active until stop timeout";
        if (!shutdown_mode)
          set_failed_locked(message);
        throw std::runtime_error(message);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }
    if (last_active_probe && pthread_create_patch_committed_) {
      if (!pthread_create_patch_) {
        process.mark_unsafe();
        unsafe_target_ = true;
        set_failed_locked("pthread_create patch ownership metadata is missing");
        throw std::runtime_error(error_);
      }
      const auto &scheduler = *pthread_create_patch_;
      bool restored_scheduler = false;
      constexpr unsigned kSchedulerRestoreAttempts = 12;
      for (unsigned attempt = 0; attempt < kSchedulerRestoreAttempts; ++attempt) {
        if (std::chrono::steady_clock::now() >= deadline)
          break;
        try {
          const injector::CodePatch restore{
              scheduler.entry, scheduler.installed.data(),
              scheduler.original.data(), scheduler.size};
          if (process.patch_code_batch(
                  ::tracer::compat::span<const injector::CodePatch>(&restore, 1))) {
            restored_scheduler = true;
            break;
          }
        } catch (const std::exception &error) {
          process.mark_unsafe();
          unsafe_target_ = true;
          set_failed_locked(
              std::string("pthread_create restore outcome is uncertain: ") +
              error.what());
          throw std::runtime_error(error_);
        }
        process.detach();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        attach_process(process, deadline);
      }
      if (!restored_scheduler) {
        if (!shutdown_mode)
          set_failed_locked(
              "pthread_create remained busy; detour bytes were not restored");
        process.detach();
        throw std::runtime_error(
            "pthread_create remained busy; removal can be retried");
      }
      pthread_create_patch_committed_ = false;
      pthread_create_patch_.reset();
    }
    if (!response.patch_size || response.patch_size > kMaxPatchBytes) {
      process.mark_unsafe();
      unsafe_target_ = true;
      set_failed_locked("agent returned invalid original patch metadata; "
                        "target remains stopped");
      throw std::runtime_error(error_);
    }
    if (!probe.value.draining()) {
      std::array<std::uint8_t, kMaxPatchBytes> bytes{};
      try {
        process.remote_read(response.original_entry, bytes.data(),
                            response.patch_size);
      } catch (const std::exception &error) {
        process.mark_unsafe();
        unsafe_target_ = true;
        set_failed_locked(std::string("could not verify restored bytes: ") +
                          error.what());
        throw std::runtime_error(error_);
      }
      if (std::memcmp(bytes.data(), response.original, response.patch_size) != 0) {
        process.mark_unsafe();
        unsafe_target_ = true;
        set_failed_locked("original function bytes changed during removal");
        throw std::runtime_error(error_);
      }
      probe.value.set_original_bytes_restored(true);
    }
    probe.events_sent = response.events_sent;
    probe.value.set_active(false);
    probe.value.set_draining(false);
    probe.value.set_callback_address(response.callback_address);
    probe.value.set_entry_address(response.original_entry);
    probe.value.set_agent_dropped_events(response.event_drops);
    probe.value.set_callback_errors(response.callback_errors);
    try {
      process.remote_free(probe.remote_request, sizeof(Request));
      probe.remote_request = 0;
    } catch (const std::exception &error) {
      set_failed_locked(
          std::string("restored probe but could not free agent request: ") +
          error.what());
      unsafe_target_ = unsafe_target_ || process.unsafe();
      try {
        process.detach();
      } catch (...) {
        process.mark_unsafe();
        unsafe_target_ = true;
      }
      throw std::runtime_error(error_);
    }
    try {
      process.detach();
    } catch (const std::exception &error) {
      process.mark_unsafe();
      unsafe_target_ = true;
      set_failed_locked(
          std::string("restored probe but target detach failed: ") +
          error.what());
      throw std::runtime_error(error_);
    }
    ::unlink(probe.source_file.c_str());
    probe.source_file.clear();
    return true;
  }
}

void Session::release_request_locked(
    ProbeRecord &probe, std::chrono::steady_clock::time_point deadline) {
  if (!probe.remote_request)
    return;
  check_identity_locked();
  injector::Process process;
  attach_process(process, deadline);
  try {
    process.remote_free(probe.remote_request, sizeof(Request));
    probe.remote_request = 0;
  } catch (const std::exception &error) {
    const std::string message =
        std::string("could not free inactive probe request: ") + error.what();
    set_failed_locked(message);
    unsafe_target_ = unsafe_target_ || process.unsafe();
    try {
      process.detach();
    } catch (...) {
      process.mark_unsafe();
      unsafe_target_ = true;
    }
    throw std::runtime_error(message);
  }
  try {
    process.detach();
  } catch (const std::exception &error) {
    process.mark_unsafe();
    unsafe_target_ = true;
    const std::string message =
        std::string("could not detach after freeing request: ") + error.what();
    set_failed_locked(message);
    throw std::runtime_error(message);
  }
}

std::uint64_t Session::events_sent(std::uint64_t probe_id) const {
  std::lock_guard lock(mutex_);
  const auto found = probes_.find(probe_id);
  return found == probes_.end() ? 0 : found->second.events_sent;
}

clangtracer::v1::Probe Session::remove_probe(std::uint64_t probe_id) {
  std::lock_guard lock(mutex_);
  if (unsafe_target_) {
    throw std::runtime_error(error_.empty() ? "session is in failed state"
                                            : error_);
  }
  check_identity_locked();
  auto &probe = find_probe_locked(probe_id);
  if (probe.value.active() || probe.value.draining())
    stop_probe_locked(probe, false);
  else
    release_request_locked(probe, std::chrono::steady_clock::now() + operation_timeout_);
  return probe.value;
}

void Session::detach() {
  std::lock_guard lock(mutex_);
  if (state_ == clangtracer::v1::Session::DETACHED ||
      state_ == clangtracer::v1::Session::EXITED)
    return;
  if (unsafe_target_) {
    throw std::runtime_error(error_.empty() ? "session is in failed state"
                                            : error_);
  }
  check_identity_locked();
  for (auto &[id, probe] : probes_) {
    (void)id;
    if (probe.value.active() || probe.value.draining())
      stop_probe_locked(probe, false);
    else
      release_request_locked(probe, std::chrono::steady_clock::now() +
                                        operation_timeout_);
  }
  state_ = clangtracer::v1::Session::DETACHED;
  application_modules_.clear();
  error_.clear();
}

void Session::shutdown() noexcept {
  std::lock_guard lock(mutex_);
  if (state_ == clangtracer::v1::Session::DETACHED ||
      state_ == clangtracer::v1::Session::EXITED || unsafe_target_)
    return;
  std::string first_error =
      state_ == clangtracer::v1::Session::FAILED ? error_ : std::string{};
  const auto shutdown_deadline =
      std::chrono::steady_clock::now() + operation_timeout_;
  for (auto &[id, probe] : probes_) {
    (void)id;
    if (!probe.value.active() && !probe.value.draining() && !probe.remote_request)
      continue;
    try {
      if (probe.value.active() || probe.value.draining())
        stop_probe_locked(probe, true, shutdown_deadline);
      else
        release_request_locked(probe, shutdown_deadline);
    } catch (const std::exception &error) {
      if (first_error.empty())
        first_error = error.what();
      if (unsafe_target_)
        break;
    }
  }
  const bool any_active =
      std::any_of(probes_.begin(), probes_.end(),
                  [](const auto &item) {
                    return item.second.value.active() || item.second.value.draining();
                  });
  if (state_ == clangtracer::v1::Session::EXITED)
    return;
  if (!any_active && first_error.empty()) {
    state_ = clangtracer::v1::Session::DETACHED;
    application_modules_.clear();
  } else if (!first_error.empty())
    set_failed_locked(first_error);
}

void Session::record_failure(const std::string &message) {
  std::lock_guard lock(mutex_);
  if (state_ != clangtracer::v1::Session::EXITED)
    set_failed_locked(message);
  if (!unsafe_target_ && start_address_ == 0 && probes_.empty())
    application_modules_.clear();
}

void Session::refresh_process_state() noexcept {
  std::lock_guard lock(mutex_);
  if (state_ != clangtracer::v1::Session::ATTACHED &&
      state_ != clangtracer::v1::Session::FAILED)
    return;
  try {
    check_identity_locked();
  } catch (...) {
  }
}

std::optional<unsigned> Session::agent_retained_count() const {
  std::lock_guard lock(mutex_);
  if (state_ != clangtracer::v1::Session::ATTACHED || !capacity_address_)
    return std::nullopt;
  std::uint64_t word = 0;
  iovec local{&word, sizeof(word)};
  iovec remote{reinterpret_cast<void *>(capacity_address_), sizeof(word)};
  if (linux_api::process_vm_readv(pid_, &local, 1, &remote, 1, 0) !=
          static_cast<ssize_t>(sizeof(word)) ||
      (word & 0xffffffff00000000ULL) != kAgentCapacityTag ||
      (word & 0xffffffffULL) > kMaxProbes)
    return std::nullopt;
  return static_cast<unsigned>(word & 0xffffffffULL);
}

clangtracer::v1::Session Session::snapshot() const {
  std::lock_guard lock(mutex_);
  clangtracer::v1::Session result;
  result.set_id(id_);
  result.set_pid(pid_);
  result.set_process_start_time(process_start_);
  result.set_executable(executable_);
  if (jit_runtime_)
    result.mutable_jit_runtime()->CopyFrom(*jit_runtime_);
  result.set_state(state_);
  result.set_error(error_);
  for (const auto &path : discovered_module_paths_) {
    result.add_application_modules(path);
  }
  for (const auto &[id, probe] : probes_) {
    (void)id;
    result.add_probes()->CopyFrom(probe.value);
  }
  return result;
}

bool Session::accepts_event(std::uint64_t probe_id, std::uint32_t pid) const {
  std::lock_guard lock(mutex_);
  if (pid != pid_ || state_ != clangtracer::v1::Session::ATTACHED)
    return false;
  const auto found = probes_.find(probe_id);
  return found != probes_.end() &&
         (found->second.value.active() || found->second.value.draining());
}

} // namespace tracer

#include "cpp_type_spelling.hpp"
#include "standard_headers.hpp"
#include "object_code_cache_identity.hpp"
#include "agent_plugin_observers.hpp"
#include "native_detour.hpp"
#include "native_entry.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <bits/cxxabi_forced.h>
#include <chrono>
#include <cctype>
#include <cstdarg>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "value_snapshot_codegen.hpp"
#include "snapshot_encoder.hpp"
#include "snapshot_runtime_limits.hpp"
#include "value_snapshot_storage.hpp"
#include "context_paths.hpp"
#include "prepared_types.hpp"
#include "prepared_contract.hpp"
#include "prepared_header_search.hpp"
#include "probe_extra_arguments.hpp"

#include <clang/AST/ASTImporter.h>
#include <clang/AST/DeclCXX.h>
#include <clang/Sema/Sema.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/DiagnosticCategories.h>
#include <clang/Basic/Version.h>
#include <clang/CodeGen/CGFunctionInfo.h>
#include <clang/CodeGen/CodeGenABITypes.h>
#include <clang/CodeGen/ModuleBuilder.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Interpreter/Interpreter.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Parse/ParseAST.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SHA256.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Triple.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "callback_bridge.hpp"
#include "agent_log_level.hpp"
#include "agent_runtime_profile.hpp"
#include "callback_pch.hpp"
#include "callback_source.hpp"
#include "context_activation.hpp"
#include "context_agent_delivery.hpp"
#include "context_analysis.hpp"
#include "context_engine.hpp"
#include "context_execution.hpp"
#include "context_pthread_detour.hpp"
#include "context_runtime.hpp"
#include "context_stdlib.hpp"
#include "debug_resolver.hpp"
#include "event_transport.hpp"
#include "linux_syscalls.hpp"
#include "native_stack.hpp"
#include "runtime_identity.hpp"
#include "runtime_paths.hpp"
#include "tracer.pb.h"
#include "tracer/compat.hpp"
#include "tracer/compiler_options.hpp"
#include "tracer/protocol.hpp"
#include "scalar_overrides.hpp"
#include "tracer/reclaim.h"
#include "tracer/receiver_abi.hpp"

#if !defined(__linux__) || !defined(__x86_64__)
#error "clang-tracer's native agent currently supports Linux x86_64 only"
#endif

namespace {
constexpr std::size_t kJumpBytes = tracer::native_detour::kJumpBytes;
using tracer::native_detour::initialize_llvm;
thread_local unsigned callback_depth = 0;

namespace agent_log = tracer::agent_log;
using agent_log::Level;

// Failure text of a prepare or stop record is capped at this size.
constexpr std::size_t kLogMessageSize = 192;

// Startup configuration, captured before the sender begins logging.
struct AgentLogConfig {
  std::array<char, PATH_MAX> path{};
  Level level = agent_log::kDefaultLevel;
  // Highest level number written; -1 turns logging off. Callers compare a
  // record's level against this one cached value.
  int threshold = -1;
  // Shown once in the log when the level text was not recognized.
  bool level_invalid = false;
  std::array<char, 32> invalid_text{};
};

const AgentLogConfig &agent_log_config() noexcept {
  static const AgentLogConfig config = [] {
    AgentLogConfig result;
    const char *configured = ::secure_getenv("CLANG_TRACER_AGENT_LOG");
    if (configured && configured[0] == '/') {
      const auto size = ::strnlen(configured, result.path.size());
      if (size < result.path.size())
        std::memcpy(result.path.data(), configured, size);
    }
    const char *level_text = ::secure_getenv("CLANG_TRACER_AGENT_LOG_LEVEL");
    const auto parsed = agent_log::parse_level(level_text);
    result.level = parsed.level;
    result.level_invalid = !parsed.valid;
    if (result.level_invalid) {
      const auto size = ::strnlen(level_text, result.invalid_text.size() - 1);
      for (std::size_t i = 0; i < size; ++i) {
        const auto byte = static_cast<unsigned char>(level_text[i]);
        result.invalid_text[i] = byte < 32 || byte == 127 ? '?' : level_text[i];
      }
    }
    if (result.path[0])
      result.threshold = static_cast<int>(result.level);
    return result;
  }();
  return config;
}

const char *agent_log_path() noexcept { return agent_log_config().path.data(); }

// True when a record at this level would be written. Callers use it to skip
// formatting work.
bool agent_log_allows(Level level) noexcept {
  return static_cast<int>(level) <= agent_log_config().threshold;
}

int open_agent_log() noexcept {
  const char *path = agent_log_path();
  if (!path[0])
    return -1;
  // A bootstrap call borrows an application thread. Avoid introducing
  // cancellation points while opening and writing its diagnostics.
  const int fd = static_cast<int>(::syscall(
      SYS_openat, AT_FDCWD, path,
      O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC | O_NONBLOCK | O_NOFOLLOW |
          O_NOCTTY,
      S_IRUSR | S_IWUSR));
  if (fd < 0)
    return -1;
  struct stat info {};
  if (::fstat(fd, &info) == 0 && S_ISREG(info.st_mode) &&
      info.st_uid == ::geteuid() && !(info.st_mode & (S_IRWXG | S_IRWXO)))
    return fd;
  (void)::syscall(SYS_close, fd);
  return -1;
}

// Formats and appends one record without any level check.
void write_agent_record(Level level, const char *operation, const char *stage,
                        std::uint64_t probe_id, const char *outcome,
                        int error_number, std::uint64_t callback_errors,
                        std::uint64_t event_drops, std::uint64_t call_id,
                        const char *message) noexcept {
  const int fd = open_agent_log();
  if (fd >= 0) {
    timespec now{};
    ::clock_gettime(CLOCK_REALTIME, &now);
    try {
      std::string line(512 + (message ? std::strlen(message) : 0), '\0');
      const int size = std::snprintf(
          line.data(), line.size(),
          "clang-tracer agent time=%lld.%09ld pid=%ld level=%s probe_id=%llu call_id=%llu "
          "operation=%s stage=%s outcome=%s errno=%d callback_errors=%llu "
          "event_drops=%llu%s%s\n",
          static_cast<long long>(now.tv_sec), now.tv_nsec, static_cast<long>(::getpid()), agent_log::level_name(level),
          static_cast<unsigned long long>(probe_id), static_cast<unsigned long long>(call_id),
          operation, stage, outcome, error_number, static_cast<unsigned long long>(callback_errors),
          static_cast<unsigned long long>(event_drops), message ? " message=" : "",
          message ? message : "");
      // Linux appends pwrite on an O_APPEND descriptor. The positional write
      // also avoids a SIGPIPE if the application replaces this descriptor.
      if (size > 0 && static_cast<std::size_t>(size) < line.size()) {
        (void)::syscall(SYS_pwrite64, fd, line.data(), static_cast<std::size_t>(size), 0LL);
      }
    } catch (...) {
    }
    (void)::syscall(SYS_close, fd);
  }
}

// Writes one record when the configured level admits it. This runs on borrowed
// application threads: preserve errno and suppress recursive callbacks.
void log_agent(Level level, const char *operation, const char *stage,
               std::uint64_t probe_id, const char *outcome,
               int error_number = 0, std::uint64_t callback_errors = 0,
               std::uint64_t event_drops = 0, std::uint64_t call_id = 0,
               const char *message = nullptr) noexcept {
  if (!agent_log_allows(level))
    return;
  const int saved_errno = errno;
  ++callback_depth;
  const auto &config = agent_log_config();
  static std::atomic_flag invalid_reported = ATOMIC_FLAG_INIT;
  if (config.level_invalid &&
      !invalid_reported.test_and_set(std::memory_order_relaxed)) {
    char text[96];
    std::snprintf(text, sizeof(text),
                  "unrecognized CLANG_TRACER_AGENT_LOG_LEVEL '%s'; using info",
                  config.invalid_text.data());
    write_agent_record(Level::Warning, "log", "level", 0, "invalid", 0, 0, 0, 0,
                       text);
  }
  write_agent_record(level, operation, stage, probe_id, outcome, error_number,
                     callback_errors, event_drops, call_id, message);
  --callback_depth;
  errno = saved_errno;
}

// Copies message text with control characters replaced by '?', capped at
// kLogMessageSize bytes. The destination holds kLogMessageSize + 1 bytes.
void sanitize_log_message(char *destination, const char *source,
                          std::size_t size) noexcept {
  size = std::min(size, kLogMessageSize);
  for (std::size_t i = 0; i < size; ++i) {
    const auto byte = static_cast<unsigned char>(source[i]);
    destination[i] = byte < 32 || byte == 127 ? '?' : source[i];
  }
  destination[size] = '\0';
}

// Writes "xx xx xx" for the bytes. The output holds three bytes per input byte.
void format_hex(char *destination, const std::uint8_t *bytes,
                std::size_t size) noexcept {
  static constexpr char digits[] = "0123456789abcdef";
  std::size_t at = 0;
  for (std::size_t i = 0; i < size; ++i) {
    if (i)
      destination[at++] = ' ';
    destination[at++] = digits[bytes[i] >> 4];
    destination[at++] = digits[bytes[i] & 15];
  }
  destination[at] = '\0';
}

struct ErrnoGuard {
  int value = errno;
  ~ErrnoGuard() { errno = value; }
};

class AgentOperation {
public:
  AgentOperation(const char *name, const tracer::Request *request) noexcept
      : request_(request && request->version == tracer::kBootstrapVersion
                     ? request
                     : nullptr),
        probe_id_(request_ ? request_->probe_id : 0), name_(name) {}

  // Enters a stage that the default level reports.
  void progress(const char *next_stage) noexcept {
    complete_open_stage();
    stage = next_stage;
    open_ = true;
    log_agent(Level::Info, name_, stage, probe_id_, "started");
  }

  // Enters a stage that only the debug level reports.
  void enter(const char *next_stage) noexcept {
    complete_open_stage();
    stage = next_stage;
    open_ = true;
    log_agent(Level::Debug, name_, stage, probe_id_, "started");
  }

  // Facts about the stage that just succeeded, at debug level.
  __attribute__((format(printf, 2, 3))) void detail(const char *format,
                                                    ...) const noexcept {
    open_ = false;
    if (!agent_log_allows(Level::Debug))
      return;
    const ErrnoGuard guard;
    char text[512];
    std::va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    log_agent(Level::Debug, name_, stage, probe_id_, "done", 0, 0, 0, 0, text);
  }

  // A record at an explicit level and outcome, such as a byte dump.
  __attribute__((format(printf, 4, 5))) void note(Level level,
                                                  const char *outcome,
                                                  const char *format,
                                                  ...) const noexcept {
    if (!agent_log_allows(level))
      return;
    const ErrnoGuard guard;
    char text[512];
    std::va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    log_agent(level, name_, stage, probe_id_, outcome, 0, 0, 0, 0, text);
  }

  void waiting(std::uint64_t callback_errors, std::uint64_t event_drops) const
      noexcept {
    log_agent(Level::Info, name_, stage, probe_id_, "pending", 0,
              callback_errors, event_drops);
  }

  // The caller can retry. This is not a failure, so only debug reports it.
  void retry(std::uint64_t callback_errors = 0,
             std::uint64_t event_drops = 0) const noexcept {
    if (!agent_log_allows(Level::Debug))
      return;
    char text[kLogMessageSize + 1];
    error_text(text);
    log_agent(Level::Debug, name_, stage, probe_id_, "retry", 0,
              callback_errors, event_drops, 0, text[0] ? text : nullptr);
  }

  int finish(int status, int error_number = 0,
             std::uint64_t callback_errors = 0,
             std::uint64_t event_drops = 0) const noexcept {
    if (status == 0) {
      log_agent(Level::Info, name_, stage, probe_id_, "ok", error_number,
                callback_errors, event_drops);
    } else if (agent_log_allows(Level::Error)) {
      char text[kLogMessageSize + 1];
      error_text(text);
      log_agent(Level::Error, name_, stage, probe_id_, "failed", error_number,
                callback_errors, event_drops, 0, text[0] ? text : nullptr);
    }
    return status;
  }

  const char *stage = "request";

private:
  // A stage left by moving to the next one succeeded, so it gets its done
  // record unless detail() already wrote one.
  void complete_open_stage() const noexcept {
    if (!open_)
      return;
    open_ = false;
    log_agent(Level::Debug, name_, stage, probe_id_, "done");
  }

  // The text the operation placed in the request's error field, if any.
  void error_text(char (&text)[kLogMessageSize + 1]) const noexcept {
    text[0] = '\0';
    if (request_)
      sanitize_log_message(
          text, request_->error,
          ::strnlen(request_->error, sizeof(request_->error)));
  }

  const tracer::Request *request_;
  const std::uint64_t probe_id_;
  const char *name_;
  mutable bool open_ = false;
};


void report_agent_failures() noexcept;
void write_agent_logs() noexcept;

struct CallbackLog {
  struct Entry {
    std::uint64_t call_id;
    tracer::TracePhase phase;
    Level level;
    // Both name static strings: a hook outcome, a per-call dispatch record, or
    // a callback message whose outcome is its callback log level.
    const char *operation;
    const char *outcome;
    std::string message;
  };
  // Producers try once. The sender releases this before writing any records.
  std::atomic_flag busy = ATOMIC_FLAG_INIT;
  std::vector<Entry> entries;
  // Set after queueing so the sender skips probes with nothing to write.
  std::atomic<bool> pending{false};
  std::atomic<std::uint64_t> dropped{0};
  // Only the event sender reads and updates the reported total.
  std::uint64_t reported_dropped = 0;
};

struct DebugRecord {
  clang::CXXRecordDecl *decl;
  clangtracer::v1::ReceiverContext layout;
  bool opaque = false;
};
using DebugRecords = std::vector<DebugRecord>;

struct OrcErrorCapture {
  std::mutex mutex;
  std::condition_variable ready;
  std::string message;
};

struct VerifiedDebugEntry {
  const clangtracer::v1::ProbeSpec *spec = nullptr;
  std::uint64_t runtime_entry = 0;
};

struct Probe {
  static constexpr std::uint64_t admission_closed = std::uint64_t{1} << 63;
  tracer::Request request{};
  std::vector<tracer::ValueType> signature_arguments;
  std::shared_ptr<OrcErrorCapture> orc_errors;
  std::unique_ptr<clang::Interpreter> interpreter;
  std::unique_ptr<clang::ASTUnit> receiver_ast;
  DebugRecords debug_records;
  void *trampoline = MAP_FAILED;
  std::size_t mapping_size = 0;
  // The entry stub of a variadic probe runs after the wrapper has left its in-flight window,
  // so a late thread can still be inside it when the probe is removed: keep the page mapped.
  bool variadic_forwarder = false;
  std::size_t entry_jump_size = kJumpBytes;
  std::atomic<std::uint64_t> sequence{0};
  tracer::EventDelivery delivery;
  std::atomic<std::uint64_t> callback_errors{0};
  // Only the event sender reads and updates the reported totals.
  std::uint64_t reported_callback_errors = 0;
  std::uint64_t reported_event_drops = 0;
  // Last state and counters written by the debug-level runtime pass. Only the
  // event sender reads and updates them.
  int reported_patch_state = -1;
  std::uint64_t reported_calls = 0;
  std::uint64_t reported_events_sent = 0;
  CallbackLog log;
  std::atomic<std::uint64_t> call_ids{0};
  std::atomic<std::uint64_t> in_flight{0};
  std::atomic<bool> callback_enabled{true};
  std::atomic<bool> active{false};
  bool callbacks_active() const noexcept {
    return active.load(std::memory_order_seq_cst) &&
           !(in_flight.load(std::memory_order_seq_cst) & admission_closed);
  }
  std::uint64_t invocations_in_flight() const noexcept {
    return in_flight.load(std::memory_order_seq_cst) & ~admission_closed;
  }
  std::string session_id;
  long long (*callback)(const tracer::TraceContext *, void *, bool *) = nullptr;
  std::vector<std::string> argument_names;
  bool class_callback = false;
  bool has_pre = false;
  bool has_post = false;
  bool receiver_sampled_at_return = false;
  bool object_snapshot = false;
  bool transport_active = false;
  bool scheduler_counted = false;
  bool reclaimable = false;
  void *wrapper = nullptr;
  std::vector<std::string> external_link_candidates;
  std::shared_ptr<const tracer::ContextActivationGeneration> shared_generation;
  std::shared_ptr<tracer::context_engine::ContextCapturePool> capture_pool;
  std::shared_ptr<tracer::context_runtime::SharedSourceRuntime> shared_runtime;
  std::shared_ptr<tracer::context_delivery::ContextAgentDelivery>
      context_delivery;

  ~Probe() {
    if (delivery.socket_fd >= 0) {
      ::close(delivery.socket_fd);
    }
    if (trampoline != MAP_FAILED && !variadic_forwarder) {
      ::munmap(trampoline, mapping_size);
    }
  }
};

// Registry nodes outlive reclaimed probes so readers can traverse a stable chain.
struct ProbeSlot {
  ProbeSlot *next = nullptr; // Immutable after publication.
  std::atomic<Probe *> probe{nullptr};
  std::atomic<std::uint64_t> id{0};
};

struct Agent {
  std::vector<std::unique_ptr<Probe>> probes;
  std::vector<std::unique_ptr<ProbeSlot>> probe_slots;
  std::atomic<ProbeSlot *> published_slots{nullptr};
  std::size_t retained = 0;
  pid_t pid = 0;
  tracer::ContextRegistry context_registry;
  std::shared_ptr<tracer::context_engine::ContextCapturePool> context_capture;
  std::uint64_t next_context_hook = 0;
  const bool logging_enabled = agent_log_path()[0] != '\0';
  // Same cached value as agent_log_allows, reachable from the loaded agent.
  const int log_threshold = agent_log_config().threshold;
  bool log_allows(Level level) const noexcept {
    return static_cast<int>(level) <= log_threshold;
  }
  tracer::EventTransport transport;
  std::shared_ptr<tracer::context_engine::PropagationLedger> propagation_ledger;
  std::unique_ptr<tracer::context_scheduler::ContextPthreadDetour>
      pthread_detour;
  void *pthread_trampoline = MAP_FAILED;
  std::size_t pthread_trampoline_size = 0;
  tracer::Request pthread_patch{};
  std::size_t context_probe_count = 0;

  Agent()
      : transport([] { ++callback_depth; },
                  logging_enabled ? &report_agent_failures : nullptr,
                  logging_enabled ? &write_agent_logs : nullptr) {}
};

struct CapturedDiagnostic {
  std::string severity;
  std::string file;
  std::uint32_t line = 0;
  std::uint32_t column = 0;
  std::string message;
};

class DiagnosticCapture final : public clang::DiagnosticConsumer {
public:
  explicit DiagnosticCapture(clang::DiagnosticConsumer *forward)
      : forward_(forward) {}

  void HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                        const clang::Diagnostic &info) override {
    clang::DiagnosticConsumer::HandleDiagnostic(level, info);
    if (level >= clang::DiagnosticsEngine::Error) {
      had_error_ = true;
    }
    llvm::SmallString<256> formatted;
    info.FormatDiagnostic(formatted);
    CapturedDiagnostic diagnostic;
    diagnostic.severity =
        level >= clang::DiagnosticsEngine::Error
            ? "error"
            : (level == clang::DiagnosticsEngine::Warning ? "warning"
                                                          : "note");
    diagnostic.message.assign(formatted.begin(), formatted.end());
    const auto location = info.getLocation();
    if (location.isValid() && info.hasSourceManager()) {
      const auto presumed = info.getSourceManager().getPresumedLoc(location);
      if (presumed.isValid()) {
        diagnostic.file = presumed.getFilename();
        diagnostic.line = presumed.getLine();
        diagnostic.column = presumed.getColumn();
        // Recovered type aliases can fail to parse before the real receiver
        // is imported. Only the callback body can prove malformed user input.
        if (level >= clang::DiagnosticsEngine::Error &&
            diagnostic.file == "callback_source") {
          const auto category =
              clang::DiagnosticIDs::getCategoryNumberForDiag(info.getID());
          had_syntax_error_ |=
              category == clang::diag::DiagCat_Parse_Issue ||
              category == clang::diag::DiagCat_Lexical_or_Preprocessor_Issue;
        }
      }
    }
    if (!diagnostic.message.empty())
      diagnostics_.push_back(std::move(diagnostic));
    if (!formatted.empty()) {
      if (!message_.empty()) {
        message_ += '\n';
      }
      message_.append(formatted.begin(), formatted.end());
    }
    if (forward_) {
      forward_->HandleDiagnostic(level, info);
    }
  }

  const std::string &message() const { return message_; }

  bool had_error() const { return had_error_; }
  bool had_syntax_error() const { return had_syntax_error_; }
  const std::vector<CapturedDiagnostic> &diagnostics() const {
    return diagnostics_;
  }

  void reset_capture() {
    message_.clear();
    diagnostics_.clear();
    had_error_ = false;
    had_syntax_error_ = false;
  }

private:
  clang::DiagnosticConsumer *forward_;
  std::string message_;
  std::vector<CapturedDiagnostic> diagnostics_;
  bool had_error_ = false;
  bool had_syntax_error_ = false;
};

void append_diagnostics(clangtracer::v1::ValidationReport &report,
                        const DiagnosticCapture &capture,
                        const char *stage) {
  for (const auto &diagnostic : capture.diagnostics()) {
    auto *output = report.add_diagnostics();
    output->set_stage(stage);
    output->set_severity(diagnostic.severity);
    output->set_file(diagnostic.file);
    output->set_line(diagnostic.line);
    output->set_column(diagnostic.column);
    output->set_message(diagnostic.message);
  }
}

void append_source_errors(
    clangtracer::v1::ValidationReport &report,
    const std::vector<tracer::ContextSourceError> &errors) {
  for (const auto &error : errors) {
    auto *output = report.add_diagnostics();
    output->set_stage("syntax");
    output->set_severity("error");
    output->set_file(error.file);
    output->set_line(error.line);
    output->set_column(error.column);
    output->set_message(error.message);
  }
}

void append_failure(clangtracer::v1::ValidationReport &report,
                    const char *stage, const std::string &message) {
  if (message.empty())
    return;
  auto *diagnostic = report.add_diagnostics();
  diagnostic->set_stage(stage);
  diagnostic->set_severity("error");
  diagnostic->set_message(message);
  // Context analysis ends its errors with " at <file>:<line>:<column>".
  const auto at = message.rfind(" at ");
  if (at == std::string::npos) return;
  const auto location = message.substr(at + 4);
  const auto column_colon = location.rfind(':');
  const auto line_colon = column_colon == std::string::npos
                              ? std::string::npos
                              : location.rfind(':', column_colon - 1);
  if (line_colon == std::string::npos || line_colon == 0) return;
  const auto file = location.substr(0, line_colon);
  const auto line = location.substr(line_colon + 1, column_colon - line_colon - 1);
  const auto column = location.substr(column_colon + 1);
  const auto digits = [](const std::string &text) {
    return !text.empty() && text.size() < 10 &&
           std::all_of(text.begin(), text.end(),
                       [](unsigned char c) { return std::isdigit(c) != 0; });
  };
  if (!digits(line) || !digits(column) ||
      file.find_first_of(" \t\r\n:") != std::string::npos)
    return;
  diagnostic->set_file(file);
  diagnostic->set_line(static_cast<std::uint32_t>(std::stoul(line)));
  diagnostic->set_column(static_cast<std::uint32_t>(std::stoul(column)));
}

void append_diagnostic_text(std::string &error, const std::string &message) {
  if (message.empty() || error.find(message) != std::string::npos)
    return;
  if (!error.empty())
    error += '\n';
  error += message;
}

extern "C" void tracer_probe_enter(unsigned long long) noexcept;
extern "C" void tracer_probe_leave(unsigned long long) noexcept;
extern "C" unsigned long long tracer_observer_clock() noexcept;
extern "C" void tracer_observer_return(unsigned long long, unsigned long long,
                                       unsigned long long, unsigned long long) noexcept;
extern "C" bool tracer_object_identity_lookup(unsigned long long,
                                               unsigned long long,
                                               std::uint64_t *,
                                               std::uint64_t *,
                                               bool *) noexcept;
extern "C" unsigned long long tracer_next_call_id(unsigned long long) noexcept;
extern "C" void tracer_capture_stack(const tracer::TraceContext *, unsigned,
                                     bool) noexcept;
extern "C" void tracer_capture_entry_stack(const tracer::TraceContext *,
                                           unsigned) noexcept;
extern "C" long long tracer_callback_dispatch(unsigned long long,
                                              const tracer::TraceContext *,
                                              void *);
extern "C" bool tracer_callback_lifecycle_enter(unsigned long long) noexcept;
extern "C" void tracer_callback_cleanup_enter() noexcept;
extern "C" void tracer_callback_lifecycle_leave() noexcept;
extern "C" void tracer_callback_failure(unsigned long long,
                                         const tracer::TraceContext *,
                                         const char *) noexcept;
extern "C" void tracer_event_dispatch(unsigned long long,
                                      const tracer::TraceContext *,
                                      long long) noexcept;
static void tracer_event_dispatch_impl(unsigned long long,
                                       const tracer::TraceContext *, long long,
                                       bool, const char *) noexcept;
extern "C" void tracer_context_check_entry();
extern "C" void *tracer_shared_enter(unsigned long long,
                                     const tracer::TraceContext *, unsigned);
extern "C" void tracer_shared_leave(void *) noexcept;
extern "C" void tracer_shared_release(void *) noexcept;
extern "C" void tracer_shared_dispatch(void *, const tracer::TraceContext *);
extern "C" void tracer_shared_original_enter(void *);
extern "C" void tracer_shared_original_leave(void *) noexcept;
extern "C" void tracer_shared_resume(void *);

std::mutex agent_mutex;
std::atomic<Agent *> agent{nullptr};
std::atomic<std::uint64_t> validation_report_sequence{0};

const char *patch_backend_name(tracer::NativePatch backend) noexcept {
  switch (backend) {
  case tracer::NativePatch::Entry:
    return "entry";
  case tracer::NativePatch::CallSite:
    return "call_site";
  case tracer::NativePatch::Trap:
    return "trap";
  }
  return "unknown";
}

const char *log_level_name(tracer::LogLevel level) noexcept {
  switch (level) {
  case tracer::LogLevel::Info:
    return "info";
  case tracer::LogLevel::Warning:
    return "warning";
  case tracer::LogLevel::Error:
    return "error";
  }
  return "unknown";
}

// State of the bytes at a probe's patch address, relative to the two images the
// agent prepared.
enum class PatchState : int {
  Installed,
  NotInstalled,
  Modified,
  Replaced,
  Unreadable
};

const char *patch_state_name(PatchState state) noexcept {
  switch (state) {
  case PatchState::Installed:
    return "installed";
  case PatchState::NotInstalled:
    return "not_installed";
  case PatchState::Modified:
    return "modified";
  case PatchState::Replaced:
    return "replaced";
  case PatchState::Unreadable:
    return "unreadable";
  }
  return "unreadable";
}

// Compares the live bytes with the prepared detour and the original. The read
// cannot fault, because the module can be unmapped concurrently.
PatchState observe_patch(const tracer::Request &request,
                         std::uint8_t (&live)[tracer::kMaxPatchBytes],
                         std::size_t &size) noexcept {
  size = request.patch_size;
  if (!request.patch_address || size == 0 || size > tracer::kMaxPatchBytes)
    return PatchState::Unreadable;
  iovec local{live, size};
  iovec remote{reinterpret_cast<void *>(request.patch_address), size};
  if (tracer::linux_api::process_vm_readv(::getpid(), &local, 1, &remote, 1,
                                          0) != static_cast<ssize_t>(size))
    return PatchState::Unreadable;
  if (std::memcmp(live, request.installed, size) == 0)
    return PatchState::Installed;
  if (std::memcmp(live, request.original, size) == 0)
    return PatchState::NotInstalled;
  return PatchState::Modified;
}

// Debug-level view of one probe: whether its detour is in place and whether
// the wrapper is being entered. Writes only what changed since the last pass.
void report_probe_activity(Agent &owner, Probe &probe) noexcept {
  const auto id = probe.request.probe_id;
  if (probe.active.load(std::memory_order_acquire)) {
    std::uint8_t live[tracer::kMaxPatchBytes];
    std::size_t size = 0;
    auto state = observe_patch(probe.request, live, size);
    std::uint64_t replaced_by = 0;
    if (state == PatchState::Modified) {
      // A newer probe of the same entry owns these bytes after a reload. Their
      // request fields are immutable once the probe is published.
      for (auto *slot = owner.published_slots.load(std::memory_order_acquire);
           slot; slot = slot->next) {
        const auto *other = slot->probe.load(std::memory_order_acquire);
        if (other && other != &probe &&
            other->request.patch_address == probe.request.patch_address &&
            other->request.patch_size == probe.request.patch_size &&
            std::memcmp(live, other->request.installed, size) == 0) {
          state = PatchState::Replaced;
          replaced_by = other->request.probe_id;
          break;
        }
      }
    }
    if (static_cast<int>(state) != probe.reported_patch_state) {
      probe.reported_patch_state = static_cast<int>(state);
      char text[160];
      int length = std::snprintf(
          text, sizeof(text), "address=0x%llx size=%u",
          static_cast<unsigned long long>(probe.request.patch_address),
          probe.request.patch_size);
      if (state == PatchState::Replaced && length > 0 &&
          static_cast<std::size_t>(length) < sizeof(text))
        std::snprintf(text + length, sizeof(text) - length, " by_probe=%llu",
                      static_cast<unsigned long long>(replaced_by));
      log_agent(Level::Debug, "probe", "patch", id, patch_state_name(state), 0,
                0, 0, 0, text);
      if (state == PatchState::Modified && owner.log_allows(Level::Trace)) {
        char line[5 + tracer::kMaxPatchBytes * 3];
        std::memcpy(line, "live=", 5);
        format_hex(line + 5, live, size);
        log_agent(Level::Trace, "probe", "patch_bytes", id, "live", 0, 0, 0, 0,
                  line);
      }
    }
  }
  const auto calls = probe.call_ids.load(std::memory_order_relaxed);
  const auto sent = probe.delivery.sent.load(std::memory_order_relaxed);
  if (calls != probe.reported_calls || sent != probe.reported_events_sent) {
    probe.reported_calls = calls;
    probe.reported_events_sent = sent;
    const auto drops = probe.delivery.dropped.load(std::memory_order_relaxed);
    const auto errors = probe.callback_errors.load(std::memory_order_relaxed);
    char text[160];
    std::snprintf(text, sizeof(text),
                  "calls=%llu events_sent=%llu event_drops=%llu "
                  "callback_errors=%llu",
                  static_cast<unsigned long long>(calls),
                  static_cast<unsigned long long>(sent),
                  static_cast<unsigned long long>(drops),
                  static_cast<unsigned long long>(errors));
    log_agent(Level::Debug, "probe", "runtime", id, "active", 0, errors, drops,
              0, text);
  }
}

void report_agent_failures() noexcept {
  auto *current = agent.load(std::memory_order_acquire);
  if (!current)
    return;
  const bool activity = current->log_allows(Level::Debug);
  for (auto *slot = current->published_slots.load(std::memory_order_acquire);
       slot; slot = slot->next) {
    auto *probe = slot->probe.load(std::memory_order_acquire);
    if (!probe)
      continue;
    const auto errors = probe->callback_errors.load(std::memory_order_relaxed);
    const auto drops = probe->delivery.dropped.load(std::memory_order_acquire);
    if (errors != probe->reported_callback_errors ||
        drops != probe->reported_event_drops) {
      log_agent(Level::Warning, "probe", "runtime", probe->request.probe_id,
                "degraded",
                probe->delivery.last_send_errno.load(std::memory_order_relaxed),
                errors, drops);
      probe->reported_callback_errors = errors;
      probe->reported_event_drops = drops;
    }
    if (activity)
      report_probe_activity(*current, *probe);
  }
}

// Writes queued hook records and callback messages in the order they ran.
void write_agent_logs() noexcept {
  auto *current = agent.load(std::memory_order_acquire);
  if (!current)
    return;
  for (auto *slot = current->published_slots.load(std::memory_order_acquire);
       slot; slot = slot->next) {
    auto *probe = slot->probe.load(std::memory_order_acquire);
    if (!probe)
      continue;
    auto &log = probe->log;
    if (log.pending.exchange(false, std::memory_order_acq_rel)) {
      if (log.busy.test_and_set(std::memory_order_acquire)) {
        // A producer holds the queue. Leave its entries for the next pass.
        log.pending.store(true, std::memory_order_release);
        continue;
      }
      std::vector<CallbackLog::Entry> pending;
      pending.swap(log.entries);
      const auto count = pending.size();
      log.busy.clear(std::memory_order_release);
      for (std::size_t i = 0; i < count; ++i) {
        const auto &entry = pending[i];
        const char *phase =
            entry.phase == tracer::TracePhase::Pre ? "pre" : "post";
        log_agent(entry.level, entry.operation, phase, probe->request.probe_id,
                  entry.outcome, 0, 0, 0, entry.call_id,
                  entry.message.empty() ? nullptr : entry.message.c_str());
      }
    }
    const auto dropped = log.dropped.load(std::memory_order_relaxed);
    if (dropped != log.reported_dropped) {
      char lost[48];
      std::snprintf(lost, sizeof(lost), "%llu records lost",
                    static_cast<unsigned long long>(dropped -
                                                    log.reported_dropped));
      log_agent(Level::Warning, "log", "queue", probe->request.probe_id,
                "dropped", 0, 0, 0, 0, lost);
      log.reported_dropped = dropped;
    }
  }
}

// Queues one record for the sender. A record above the configured level is not
// queued and does not use a slot. Allocation failure or contention drops it.
bool enqueue_agent_log(Probe &probe, const tracer::TraceContext &context,
                       Level level, const char *operation, const char *outcome,
                       const char *message, std::size_t size) noexcept {
  if (!agent_log_allows(level))
    return false;
  auto &log = probe.log;
  bool queued = false;
  if (!log.busy.test_and_set(std::memory_order_acquire)) {
    try {
      CallbackLog::Entry entry{};
      entry.call_id = context.call_id;
      entry.phase = context.phase;
      entry.level = level;
      entry.operation = operation;
      entry.outcome = outcome;
      entry.message.resize(message ? size : 0);
      for (std::size_t i = 0; i < entry.message.size(); ++i) {
        const auto byte = static_cast<unsigned char>(message[i]);
        entry.message[i] = byte < 32 || byte == 127 ? '?' : message[i];
      }
      log.entries.push_back(std::move(entry));
      queued = true;
    } catch (...) {
      queued = false;
    }
    log.busy.clear(std::memory_order_release);
  }
  if (queued)
    log.pending.store(true, std::memory_order_release);
  else
    log.dropped.fetch_add(1, std::memory_order_relaxed);
  return queued;
}

// Records that the agent ran a PRE or POST hook, when logging is enabled.
void log_hook(Probe &probe, const tracer::TraceContext &context,
              bool succeeded) noexcept {
  auto *owner = agent.load(std::memory_order_acquire);
  const auto level = succeeded ? Level::Info : Level::Error;
  if (owner && owner->log_allows(level))
    (void)enqueue_agent_log(probe, context, level, "hook",
                            succeeded ? "ok" : "failed", nullptr, 0);
}

// Trace level only: records that a phase is being dispatched to its hook. This
// is the one check on the per-call path when the level is lower.
void log_dispatch(Probe &probe, const tracer::TraceContext &context) noexcept {
  auto *owner = agent.load(std::memory_order_acquire);
  if (owner && owner->log_allows(Level::Trace))
    (void)enqueue_agent_log(probe, context, Level::Trace, "call", "dispatched",
                            nullptr, 0);
}

Probe *find_probe(std::uint64_t id) noexcept {
  Agent *current = agent.load(std::memory_order_acquire);
  if (!current) {
    return nullptr;
  }
  for (auto *slot = current->published_slots.load(std::memory_order_acquire);
       slot; slot = slot->next) {
    if (id && slot->id.load(std::memory_order_acquire) == id)
      return slot->probe.load(std::memory_order_acquire);
  }
  return nullptr;
}

struct SharedInvocationToken {
  tracer::detail::NamedValueStorage named;
  Probe *probe = nullptr;
  tracer::TraceContext raw{};
  tracer::TraceContext entry_context{};
  std::vector<tracer::StackFrame> filtered_frames;
  tracer::Call call{raw};
  std::optional<tracer::ContextStdlibImage::Frame> support;
  std::optional<tracer::context_runtime::SharedSourceRuntime::Invocation>
      invocation;
  std::optional<tracer::context_execution::OriginalDispatch> original;
  alignas(64) std::byte object[4096];
  int cancel_state = PTHREAD_CANCEL_ENABLE;
  std::atomic<bool> occupied{false};
  bool cancel_disabled = false, constructed = false, enabled = false,
       finished = false;
};

struct SharedInvocationStorage {
  std::vector<std::unique_ptr<SharedInvocationToken>> tokens;

  ~SharedInvocationStorage() {
    ++callback_depth;
    std::vector<std::unique_ptr<SharedInvocationToken>>().swap(tokens);
    --callback_depth;
  }
};

thread_local SharedInvocationStorage shared_invocation_storage;

template <class T>
void copy_shared_value(T &destination, const T &source) noexcept {
  static_assert(std::is_trivially_copyable_v<T>);
#if defined(__clang__)
  // Keep the complete object copy inline, without an interposable memcpy call.
  __builtin_memcpy_inline(&destination, &source, sizeof(T));
#else
  auto *out = reinterpret_cast<volatile unsigned char *>(&destination);
  const auto *in = reinterpret_cast<const volatile unsigned char *>(&source);
  for (std::size_t i = 0; i < sizeof(T); ++i)
    out[i] = in[i];
#endif
}

// Symbolization may allocate or take metadata locks. Shared hooks only select
// from the immutable invocation-entry snapshot prepared before scope access.
void shared_capture_stack(const tracer::TraceContext *context, unsigned depth,
                          bool application_only) noexcept {
  for (auto& owned : shared_invocation_storage.tokens) {
    auto& token = *owned;
    if (&token.raw != context ||
        !token.occupied.load(std::memory_order_acquire))
      continue;
    const auto &entry = token.entry_context.stack;
    auto &stack = context->stack;
    copy_shared_value(stack, entry);
    const auto count = std::min(depth, entry.count);
    stack.requested_depth = depth;
    stack.application_only = application_only;
    stack.captured_depth = count;
    if (count < entry.count) {
      stack.status = tracer::StackStatus::Truncated;
      constexpr char detail[] = "entry stack exceeds requested depth";
      for (std::size_t i = 0; i < sizeof(detail); ++i)
        stack.detail[i] = detail[i];
    }
    stack.count = count;
    if (application_only) {
      stack.count = 0;
      for (unsigned i = 0; i < count; ++i) {
        if (entry.frames[i].application_frame)
          copy_shared_value(token.filtered_frames[stack.count++], entry.frames[i]);
      }
      stack.hidden_frames = count - stack.count;
      stack.frames = token.filtered_frames.data();
    }
    return;
  }
}

bool invoke_shared(SharedInvocationToken &token, std::uintptr_t entry,
                   bool destruction = false) noexcept {
  const auto &code = token.probe->shared_generation->callback_code();
  tracer::ContextStdlibImage::Frame frame(
      code->standard_library(), code.get(),
      &tracer::PrivateContextCode::unwind_lookup);
  if (!frame.entered())
    return false;
  struct Arguments {
    SharedInvocationToken *token;
    std::uintptr_t entry;
    bool destruction;
    bool result = false;
  } args{&token, entry, destruction};
  return code->standard_library()->invoke(
             [](void *opaque) {
               auto &args = *static_cast<Arguments *>(opaque);
               if (args.destruction) {
                 reinterpret_cast<void (*)(void *)>(args.entry)(
                     args.token->object);
                 args.result = true;
               } else {
                 args.result = reinterpret_cast<bool (*)(
                     void *, const tracer::TraceContext *)>(args.entry)(
                     args.token->object, &args.token->raw);
               }
             },
             &args) &&
         args.result;
}

extern "C" void tracer_context_check_entry() {
  using namespace tracer::context_execution;
  if (check_entry(Callee::ApplicationEndpoint) == EntryDecision::Reject) {
    runtime_diagnostics().record(Severity::error,
                                 Reason::reentrant_application_call,
                                 Phase::target, Action::call_rejected);
    throw tracer::context_engine::Error(
        tracer::context_engine::ErrorCode::active_access);
  }
}

extern "C" void *tracer_shared_enter(unsigned long long id,
                                     const tracer::TraceContext *raw,
                                     unsigned stack_depth) {
  auto *probe = find_probe(id);
  if (!probe || !raw || callback_depth || !probe->shared_runtime ||
      !probe->callbacks_active() ||
      tracer::context_execution::check_entry(
          tracer::context_execution::Callee::ApplicationEndpoint) !=
          tracer::context_execution::EntryDecision::Trace)
    return nullptr;
  const int saved_errno = errno;
  int prior_type = PTHREAD_CANCEL_DEFERRED;
  ::pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &prior_type);
  if (prior_type == PTHREAD_CANCEL_ASYNCHRONOUS) {
    ::pthread_setcanceltype(prior_type, nullptr);
    tracer::context_execution::runtime_diagnostics().record(
        tracer::context_execution::Severity::warning,
        tracer::context_execution::Reason::unsupported_effect,
        tracer::context_execution::Phase::construction,
        tracer::context_execution::Action::callback_skipped, raw->call_id);
    errno = saved_errno;
    return nullptr;
  }
  SharedInvocationToken *token = nullptr;
  for (auto& owned : shared_invocation_storage.tokens) {
    auto& candidate = *owned;
    bool free = false;
    if (candidate.occupied.compare_exchange_strong(free, true,
                                                   std::memory_order_acq_rel)) {
      token = &candidate;
      break;
    }
  }
  if (!token) {
    ++callback_depth;
    try {
      auto owned = std::make_unique<SharedInvocationToken>();
      token = owned.get();
      shared_invocation_storage.tokens.push_back(std::move(owned));
      token->occupied.store(true, std::memory_order_release);
    } catch (...) {
      token = nullptr;
    }
    --callback_depth;
  }
  if (!token) {
    tracer::context_execution::runtime_diagnostics().record(
        tracer::context_execution::Severity::warning,
        tracer::context_execution::Reason::access_failure,
        tracer::context_execution::Phase::construction,
        tracer::context_execution::Action::callback_skipped, raw->call_id);
    errno = saved_errno;
    return nullptr;
  }
  token->finished = false;
  token->probe = probe;
  copy_shared_value(token->raw, *raw);
  ++callback_depth;
  token->named.clear();
  --callback_depth;
  token->raw.named_storage = &token->named;
  token->raw.named_values = nullptr;
  token->raw.named_count = 0;
  token->raw.append_named = &tracer::detail::append_named_value;
  for (unsigned i = 0; i < token->raw.argument_count; ++i) {
    token->raw.argument_names[i] =
        i < probe->argument_names.size() ? probe->argument_names[i].c_str() : nullptr;
  }
  ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &token->cancel_state);
  token->cancel_disabled = true;
  ++callback_depth;
  token->raw.stack_capture = &shared_capture_stack;
  try {
    if (probe->shared_generation->requires_stack()) {
      // The callback can choose its depth while a shared scope is active.
      // Capture once before entering that scope and reuse it for automatic
      // entry capture and for any callback-requested views.
      copy_shared_value(token->entry_context, *raw);
      token->entry_context.stack_storage = nullptr;
      tracer::capture_native_stack(token->entry_context,
                                   tracer::kUnlimitedStackDepth, false);
      token->filtered_frames.resize(token->entry_context.stack.count);
      if (stack_depth)
        shared_capture_stack(&token->raw, stack_depth, raw->stack_application_only);
    } else if (stack_depth) {
      tracer::capture_native_stack(*raw, stack_depth, raw->stack_application_only);
      copy_shared_value(token->raw.stack, raw->stack);
    }
    const auto &code = probe->shared_generation->callback_code();
    // Reserve the teardown lane before constructing any generated frame. It
    // stays pinned through the original call and final owner destruction.
    token->support.emplace(code->standard_library(), code.get(),
                           &tracer::PrivateContextCode::unwind_lookup);
    if (token->support->entered())
      token->invocation.emplace(*probe->shared_runtime, &token->call);
    if (token->invocation && token->invocation->entered()) {
      tracer::context_runtime::LifecycleBoundary lifecycle;
      token->constructed =
          lifecycle.entered() &&
          invoke_shared(*token, probe->shared_generation->construct_entry());
      token->enabled = token->constructed;
    }
  } catch (...) {
    token->enabled = false;
  }
  if (!token->constructed) {
    probe->callback_errors.fetch_add(1, std::memory_order_relaxed);
    tracer::context_execution::runtime_diagnostics().record(
        tracer::context_execution::Severity::error,
        tracer::context_execution::Reason::constructor_failure,
        tracer::context_execution::Phase::construction,
        tracer::context_execution::Action::callback_skipped, raw->call_id);
  }
  --callback_depth;
  if (!token->constructed)
    tracer_event_dispatch_impl(id, &token->raw, 0, true,
                               "shared interceptor construction failed");
  copy_shared_value(raw->stack, token->raw.stack);
  errno = saved_errno;
  return token;
}

extern "C" void tracer_shared_resume(void *opaque) {
  auto *token = static_cast<SharedInvocationToken *>(opaque);
  if (token && token->cancel_disabled) {
    token->cancel_disabled = false;
    ::pthread_setcancelstate(token->cancel_state, nullptr);
  }
}

extern "C" void tracer_shared_dispatch(void *opaque,
                                       const tracer::TraceContext *raw) {
  auto *token = static_cast<SharedInvocationToken *>(opaque);
  if (!token || !token->enabled || !raw)
    return;
  const int saved_errno = errno;
  ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &token->cancel_state);
  token->cancel_disabled = true;
  copy_shared_value(token->raw, *raw);
  log_dispatch(*token->probe, token->raw);
  token->raw.stack_capture = &shared_capture_stack;
  ++callback_depth;
  token->named.clear();
  --callback_depth;
  token->raw.named_storage = &token->named;
  token->raw.named_values = nullptr;
  token->raw.named_count = 0;
  token->raw.append_named = &tracer::detail::append_named_value;
  for (unsigned i = 0; i < token->raw.argument_count; ++i) {
    token->raw.argument_names[i] =
        i < token->probe->argument_names.size() ? token->probe->argument_names[i].c_str() : nullptr;
  }
  ++callback_depth;
  const auto entry = raw->phase == tracer::TracePhase::Pre
                         ? token->probe->shared_generation->pre_entry()
                         : token->probe->shared_generation->post_entry();
  const bool hook_succeeded = invoke_shared(*token, entry);
  if (token->probe->shared_generation->has_hook(raw->phase))
    log_hook(*token->probe, token->raw, hook_succeeded);
  if (!hook_succeeded) {
    token->enabled = false;
    token->probe->callback_errors.fetch_add(1, std::memory_order_relaxed);
    tracer::context_execution::runtime_diagnostics().record(
        tracer::context_execution::Severity::error,
        tracer::context_execution::Reason::hook_failure,
        raw->phase == tracer::TracePhase::Pre
            ? tracer::context_execution::Phase::pre
            : tracer::context_execution::Phase::post,
        tracer::context_execution::Action::callback_skipped, raw->call_id);
  }
  --callback_depth;
  if (!hook_succeeded)
    tracer_event_dispatch_impl(token->probe->request.probe_id,
                               &token->raw, 0, true,
                               "shared interceptor callback failed");
  copy_shared_value(raw->stack, token->raw.stack);
  errno = saved_errno;
  tracer_shared_resume(token);
}

extern "C" void tracer_shared_original_enter(void *opaque) {
  auto *token = static_cast<SharedInvocationToken *>(opaque);
  if (!token || !token->invocation || !token->invocation->entered())
    return;
  token->original.emplace();
  if (!token->original->entered()) {
    token->original.reset();
    throw tracer::context_engine::Error(
        tracer::context_engine::ErrorCode::active_access);
  }
}
extern "C" void tracer_shared_original_leave(void *opaque) noexcept {
  if (auto *token = static_cast<SharedInvocationToken *>(opaque))
    token->original.reset();
}
extern "C" void tracer_shared_leave(void *opaque) noexcept {
  auto *token = static_cast<SharedInvocationToken *>(opaque);
  if (!token || !token->occupied.load(std::memory_order_acquire) ||
      token->finished)
    return;
  const int saved_errno = errno;
  if (!token->cancel_disabled) {
    ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &token->cancel_state);
    token->cancel_disabled = true;
  }
  token->original.reset();
  ++callback_depth;
  if (token->constructed) {
    tracer::context_runtime::LifecycleBoundary lifecycle;
    if (!lifecycle.entered() ||
        !invoke_shared(*token, token->probe->shared_generation->destroy_entry(),
                       true))
      tracer::context_execution::runtime_diagnostics().record(
          tracer::context_execution::Severity::critical,
          tracer::context_execution::Reason::invalid_ownership,
          tracer::context_execution::Phase::lifecycle,
          tracer::context_execution::Action::lifecycle_ignored,
          token->raw.call_id);
  }
  token->constructed = false;
  token->enabled = false;
  token->invocation.reset();
  token->support.reset();
  --callback_depth;
  token->probe = nullptr;
  token->finished = true;
  errno = saved_errno;
}

extern "C" void tracer_shared_release(void *opaque) noexcept {
  auto *token = static_cast<SharedInvocationToken *>(opaque);
  if (token && token->finished) {
    const int saved_errno = errno;
    tracer::release_native_stack(token->entry_context);
    token->entry_context.stack = {};
    std::vector<tracer::StackFrame>().swap(token->filtered_frames);
    token->occupied.store(false, std::memory_order_release);
    errno = saved_errno;
  }
}

template <class Address>
auto address_value(Address address) -> decltype(address.getValue()) {
  return address.getValue();
}

std::uint64_t address_value(std::uint64_t address) {
  return address;
}

void set_error(tracer::Request *request, const char *text) noexcept {
  if (request) {
    std::snprintf(request->error, sizeof(request->error), "%s", text);
  }
}

bool valid_type(tracer::ValueType type, bool result);

bool read_probe_spec(tracer::Request& request, std::vector<tracer::ValueType>& arguments,
                     clangtracer::v1::ProbeSpec& spec, std::string& error) {
  std::ifstream input(request.source_path, std::ios::binary);
  if (!input || !spec.ParseFromIstream(&input)) {
    error = "cannot parse serialized probe specification";
    return false;
  }
  if (spec.symbol() != request.symbol || !spec.has_signature() ||
      spec.object_watch_id() != request.object_watch_id ||
      static_cast<unsigned>(spec.signature().kind()) != static_cast<unsigned>(request.kind) ||
      static_cast<unsigned>(spec.signature().result()) != static_cast<unsigned>(request.result) ||
      spec.signature().arguments_size() != static_cast<int>(request.argument_count)) {
    error = "serialized probe specification does not match the prepared request";
    return false;
  }
  if (request.object_watch_id &&
      (request.kind != tracer::CallableKind::Method || !spec.has_debug_info() ||
       spec.debug_info().record_name().empty() ||
       !tracer::prepared::is_typed_version(spec.prepared().format_version()))) {
    error = "object-watch method requires verified receiver type metadata";
    return false;
  }
  arguments.clear();
  for (const auto type : spec.signature().arguments()) {
    if (!valid_type(static_cast<tracer::ValueType>(type), false)) {
      error = "unsupported argument type";
      return false;
    }
    arguments.push_back(static_cast<tracer::ValueType>(type));
  }
  request.arguments = arguments.data();
  if (spec.has_receiver()) {
    const auto &receiver = spec.receiver();
    if (receiver.translation_unit().empty() || receiver.working_directory().empty() ||
        receiver.compiler_arguments().empty() || receiver.record_name().empty()) {
      error = "receiver compilation context is incomplete";
      return false;
    }
  }
  return true;
}

bool supported_receiver_field(clang::QualType type) {
  type = type.getCanonicalType();
  if (const auto *array =
          llvm::dyn_cast<clang::ConstantArrayType>(type.getTypePtr())) {
    return supported_receiver_field(array->getElementType());
  }
  if (type->isPointerType()) {
    const auto pointee = type->getPointeeType().getCanonicalType();
    return pointee->isVoidType() || pointee->isBuiltinType();
  }
  return type->isBuiltinType() && type->isScalarType();
}

bool matches_receiver_value_type(clang::QualType type,
                                 clang::ASTContext &context,
                                 tracer::ValueType expected, bool result) {
  if (type->isReferenceType()) {
    return false;
  }
  type = type.getCanonicalType().getUnqualifiedType();
  if (type->isVoidType()) {
    return result && expected == tracer::ValueType::Void;
  }
  if (type->isBooleanType())
    return expected == tracer::ValueType::Bool;
  if (type->isPointerType()) {
    return expected == tracer::ValueType::Pointer &&
           context.getTypeSize(type) == 64;
  }
  if (type->isCharType() || type->isBitIntType()) {
    return false;
  }
  if (type->isFloatingType()) {
    return (context.getTypeSize(type) == 32 &&
            expected == tracer::ValueType::Float32) ||
           (context.getTypeSize(type) == 64 &&
            expected == tracer::ValueType::Float64);
  }
  if (type->isIntegerType() || type->isEnumeralType()) {
    const auto bits = context.getTypeSize(type);
    const bool is_signed = type->isSignedIntegerOrEnumerationType();
    return (bits == 32 && is_signed && expected == tracer::ValueType::Int32) ||
           (bits == 64 && is_signed && expected == tracer::ValueType::Int64) ||
           (bits == 64 && !is_signed && expected == tracer::ValueType::UInt64);
  }
  return false;
}

const char *field_access_name(clang::AccessSpecifier access, bool is_class) {
  switch (access) {
  case clang::AS_public:
    return "public";
  case clang::AS_protected:
    return "protected";
  case clang::AS_private:
    return "private";
  case clang::AS_none:
    return is_class ? "private" : "public";
  }
  return "";
}

std::string record_layout_identity(clang::ASTContext &context,
                                   const clang::CXXRecordDecl *record);

// The identity of one member type: arrays by extent and element, records by their own structural
// identity, everything else by its ABI identity and canonical spelling.
std::string layout_type_identity(clang::ASTContext &context, clang::QualType type) {
  type = type.getCanonicalType();
  std::string qualifiers;
  if (type.isConstQualified())
    qualifiers += "const ";
  if (type.isVolatileQualified())
    qualifiers += "volatile ";
  if (const auto *array = context.getAsConstantArrayType(type))
    return qualifiers + "array[" + std::to_string(array->getSize().getZExtValue()) + "]" +
           layout_type_identity(context, array->getElementType());
  if (!type->isDependentType())
    if (const auto *record = type->getAsCXXRecordDecl()) {
      // An unnamed record has no spelling that survives an AST import (it names its location).
      const auto name = record->getDeclName().isEmpty() ? std::string()
                                                         : record->getQualifiedNameAsString();
      return qualifiers + "record[" + name + "]" + record_layout_identity(context, record);
    }
  clang::PrintingPolicy policy(context.getLangOpts());
  return qualifiers + tracer::receiver_abi::type_identity(type, context) + "|" +
         type.getAsString(policy);
}

// The structural identity of a record that has bases, unions, record, array or bit-field members:
// everything that decides where each member lives and what it is, recursively. Two records with
// the same identity are laid out the same way, so a record rebuilt or imported later can be
// compared with the one that was verified against DWARF. Unnamed bit-fields are padding: they
// are left out, and the offsets of their neighbours carry the effect.
std::string record_layout_identity(clang::ASTContext &context,
                                   const clang::CXXRecordDecl *record) {
  record = record ? record->getDefinition() : nullptr;
  if (!record)
    return "incomplete";
  const auto &layout = context.getASTRecordLayout(record);
  std::string identity = std::string(record->isUnion() ? "union" : "class") +
                         "{size=" + std::to_string(layout.getSize().getQuantity()) +
                         ";align=" + std::to_string(layout.getAlignment().getQuantity()) +
                         (record->isDynamicClass() ? ";dynamic" : "");
  for (const auto &base : record->bases()) {
    const auto *base_record = base.getType()->getAsCXXRecordDecl();
    identity += ";base[" + std::string(field_access_name(base.getAccessSpecifier(), false));
    if (base.isVirtual() || !base_record) {
      identity += ";virtual]";
      continue;
    }
    identity += ";offset=" + std::to_string(layout.getBaseClassOffset(base_record).getQuantity()) +
                ";name=" + base_record->getQualifiedNameAsString() + ";" +
                record_layout_identity(context, base_record) + "]";
  }
  for (const auto *field : record->fields()) {
    if (field->isUnnamedBitField())
      continue;
    identity += ";field[" + field->getNameAsString() + ";" +
                field_access_name(field->getAccess(), record->isClass()) +
                (field->isMutable() ? ";mutable" : "") +
                ";offset=" + std::to_string(layout.getFieldOffset(field->getFieldIndex())) +
                ";width=" + std::to_string(field->isBitField() ? field->getBitWidthValue() : 0) +
                ";" + layout_type_identity(context, field->getType()) + "]";
  }
  return identity + "}";
}

bool validate_receiver_layout(clang::ASTContext &context,
                              clang::CXXRecordDecl *record,
                              const clangtracer::v1::ReceiverContext &receiver,
                              std::string &error) {
  if (!receiver.layout_identity().empty()) {
    // A record with bases, unions or record, array and bit-field members is compared by its
    // structural identity instead of a flat field list.
    if (!record || !record->isCompleteDefinition() || record->isDependentType() ||
        record->getDescribedClassTemplate() ||
        llvm::isa<clang::ClassTemplateSpecializationDecl>(record) || record->isDynamicClass() ||
        record->getNumVBases() != 0) {
      error = "receiver class has an unsupported or changed layout";
      return false;
    }
    const auto &layout = context.getASTRecordLayout(record);
    if (static_cast<std::uint64_t>(layout.getSize().getQuantity()) != receiver.size() ||
        static_cast<std::uint64_t>(layout.getAlignment().getQuantity()) != receiver.alignment()) {
      error = "receiver size or alignment changed since source selection";
      return false;
    }
    if (record_layout_identity(context, record) != receiver.layout_identity()) {
      error = "receiver fields changed since source selection";
      return false;
    }
    return true;
  }
  if (!record || !record->isCompleteDefinition() || record->isUnion() ||
      record->isDependentType() || record->getDescribedClassTemplate() ||
      llvm::isa<clang::ClassTemplateSpecializationDecl>(record) ||
      record->isDynamicClass() || record->getNumBases() != 0 ||
      std::distance(record->fields().begin(), record->fields().end()) !=
          static_cast<std::ptrdiff_t>(receiver.fields_size())) {
    error = "receiver class has an unsupported or changed layout";
    return false;
  }
  const auto &layout = context.getASTRecordLayout(record);
  const auto record_size = static_cast<std::uint64_t>(
      context
          .getTypeSizeInChars(context.getTypeDeclType(
              static_cast<const clang::TypeDecl *>(record)))
          .getQuantity());
  const auto record_alignment =
      static_cast<std::uint64_t>(layout.getAlignment().getQuantity());
  if (record_size != receiver.size() ||
      record_alignment != receiver.alignment()) {
    error = "receiver size or alignment changed since source selection";
    return false;
  }
  clang::PrintingPolicy policy(context.getLangOpts());
  unsigned index = 0;
  for (clang::FieldDecl *field : record->fields()) {
    const auto &expected = receiver.fields(static_cast<int>(index));
    const auto type = field->getType();
    const auto bit_width = field->isBitField() ? field->getBitWidthValue() : 0;
    if (!supported_receiver_field(type) ||
        field->getNameAsString() != expected.name() ||
        type.getCanonicalType().getAsString(policy) !=
            expected.canonical_type() ||
        layout.getFieldOffset(index) != expected.bit_offset() ||
        bit_width != expected.bit_width() ||
        context.getTypeSize(type) != expected.bit_size() ||
        context.getTypeAlign(type) != expected.bit_alignment() ||
        tracer::receiver_abi::type_identity(type, context) !=
            expected.abi_identity() ||
        field->isMutable() != expected.is_mutable() ||
        field_access_name(field->getAccess(), record->isClass()) !=
            expected.access()) {
      error = "receiver fields changed since source selection";
      return false;
    }
    ++index;
  }
  return true;
}

std::string mangle_function(const clang::FunctionDecl *declaration,
                            clang::ASTContext &context) {
  auto mangle =
      std::unique_ptr<clang::MangleContext>(context.createMangleContext());
  if (!mangle->shouldMangleDeclName(declaration)) {
    return declaration->getNameAsString();
  }
  std::string symbol;
  llvm::raw_string_ostream output(symbol);
  mangle->mangleName(clang::GlobalDecl(declaration), output);
  output.flush();
  return symbol;
}

bool debug_identifier(llvm::StringRef text) {
  if (text.empty() || text.size() > 63)
    return false;
  for (std::size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
          (i && c >= '0' && c <= '9')))
      return false;
  }
  return true;
}

std::string normalized_cpp_type(llvm::StringRef text) {
  return tracer::normalized_cpp_type_spelling(
      std::string_view(text.data(), text.size()));
}

// Pointer levels of a record type's own declarator in a DWARF type spelling. A `*` inside the
// record's template arguments (std::map<int, int (*)(int)>&, std::vector<Node*>) belongs to an
// argument, not to the pointer chain that leads to the record.
std::size_t record_pointer_levels(std::string_view type) {
  std::size_t depth = 0, levels = 0;
  for (const char c : type) {
    if (c == '<')
      ++depth;
    else if (c == '>' && depth)
      --depth;
    else if (c == '*' && depth == 0)
      ++levels;
  }
  return levels;
}

bool standard_library_record_name(llvm::StringRef name) {
  return ::tracer::compat::starts_with(name, "std::") ||
         ::tracer::compat::starts_with(name, "__gnu_cxx::") ||
         ::tracer::compat::starts_with(name, "__cxxabiv1::");
}

// Headers chosen from DWARF are placed at the start of the callback source as ordinary
// #include lines, each tagged with this trailing comment. The rest of the agent therefore
// treats them exactly like headers the caller wrote in the callback, while code that must tell
// the two apart (records still recovered from DWARF, ABI evidence that only a user-supplied
// definition can replace) can do so.
constexpr std::string_view kAutomaticHeaderTag = " // clang-tracer: automatic standard header";

std::string without_automatic_headers(const std::string &text) {
  std::string result;
  std::size_t begin = 0;
  while (begin < text.size()) {
    const auto newline = text.find('\n', begin);
    const auto end = newline == std::string::npos ? text.size() : newline + 1;
    const std::string_view line(text.data() + begin, end - begin);
    if (line.find(kAutomaticHeaderTag) == std::string_view::npos)
      result.append(line);
    begin = end;
  }
  return result;
}

// True when `includes` holds headers the caller supplied, ignoring the automatic ones.
bool has_user_includes(const std::string &includes) {
  return !without_automatic_headers(includes).empty();
}

bool has_automatic_headers(const std::string &includes) {
  return includes.find(kAutomaticHeaderTag) != std::string::npos;
}

// Every standard-library record the callable's description names, with the role it plays.
// Template arguments are followed through their structured DWARF description, so the header of
// a record reached only as an argument (std::chrono::duration inside a std::vector) is found
// without parsing a spelling.
struct StandardRecordUse {
  std::string name;
  std::string role;
  bool nested = false;  // Reached only as a template argument of another standard record.
  bool enumeration = false;  // An enumeration the signature names, not a record.
};

void collect_standard_records(const clangtracer::v1::DebugInfo &info, const std::string &role,
                              std::vector<StandardRecordUse> &uses, bool nested = false);

void collect_standard_records(const clangtracer::v1::DebugTemplateType &type,
                              const std::string &role, std::vector<StandardRecordUse> &uses) {
  if (type.kind() == clangtracer::v1::DebugTemplateType::RECORD) {
    if (type.has_record())
      collect_standard_records(type.record(), role, uses, true);
    else
      uses.push_back({type.name(), role, true});
  }
  if (type.has_element())
    collect_standard_records(type.element(), role, uses);
  for (const auto &parameter : type.parameters())
    collect_standard_records(parameter, role, uses);
}

void collect_standard_records(const clangtracer::v1::DebugTemplateArgument &argument,
                              const std::string &role, std::vector<StandardRecordUse> &uses) {
  if (argument.has_type())
    collect_standard_records(argument.type(), role, uses);
  for (const auto &element : argument.pack_elements())
    collect_standard_records(element, role, uses);
}

void collect_standard_records(const clangtracer::v1::DebugInfo &info, const std::string &role,
                              std::vector<StandardRecordUse> &uses, bool nested) {
  if (!info.record_name().empty())
    uses.push_back({info.record_name(), role, nested});
  // Only a standard-library record brings its arguments' headers with it. A program template
  // that merely has a standard type as an argument keeps the headerless recovery it always had.
  if (::tracer::standard_headers::standard_library_scope(info.record_name()))
    for (const auto &argument : info.template_arguments())
      collect_standard_records(argument, nested ? role : role + " template argument", uses);
}

// Every standard-library record the callable names: receiver, arguments, result and, through
// their structured template arguments, the records those name.
std::vector<StandardRecordUse> callable_standard_records(const clangtracer::v1::ProbeSpec &spec) {
  const auto &info = spec.debug_info();
  std::vector<StandardRecordUse> uses;
  collect_standard_records(info, "receiver", uses);
  for (const auto &argument : info.arguments())
    if (argument.has_object_type())
      collect_standard_records(argument.object_type(),
                               "argument " + std::to_string(argument.index()), uses);
  if (info.has_result_object_type())
    collect_standard_records(info.result_object_type(), "result", uses);
  // A standard-library enumeration is named by the signature itself; its header is not
  // recovered from DWARF (an opaque redeclaration inside std is not an option).
  const auto enumeration = [&](const clangtracer::v1::DebugTemplateType &type,
                               const std::string &role) {
    if (type.kind() == clangtracer::v1::DebugTemplateType::ENUM &&
        ::tracer::standard_headers::standard_library_scope(type.name()))
      uses.push_back({type.name(), role, false, true});
  };
  for (const auto &argument : info.arguments())
    if (argument.has_enum_type())
      enumeration(argument.enum_type(), "argument " + std::to_string(argument.index()));
  if (info.has_result_enum_type())
    enumeration(info.result_enum_type(), "result");
  return uses;
}

bool parse_standard_header_entries(const clangtracer::v1::ProbeSpec &spec,
                                   std::vector<::tracer::standard_headers::Entry> &entries,
                                   std::string &error) {
  for (const auto &text : spec.standard_headers()) {
    ::tracer::standard_headers::Entry entry;
    if (const auto problem = ::tracer::standard_headers::parse_entry(text, entry);
        !problem.empty()) {
      error = "invalid standard header mapping '" + text + "': " + problem;
      return false;
    }
    entries.push_back(std::move(entry));
  }
  return true;
}

// Standard-library records that are only template arguments and have no header mapping. They
// do not stop a probe: the header of the record that names them normally defines them
// (<chrono> defines the clock a time_point names). They are listed when the type cannot be
// built after all, with the entry that would map each.
std::string unmapped_argument_records(const clangtracer::v1::ProbeSpec &spec) {
  std::vector<::tracer::standard_headers::Entry> user_entries;
  std::string ignored;
  if (!parse_standard_header_entries(spec, user_entries, ignored))
    return {};
  std::set<std::string> keys;
  for (const auto &use : callable_standard_records(spec)) {
    std::string key;
    if (use.nested && ::tracer::standard_headers::standard_library_scope(use.name) &&
        !::tracer::standard_headers::find_entry(use.name, user_entries, key))
      keys.insert(key);
  }
  std::string result;
  for (const auto &key : keys)
    result += (result.empty() ? "" : ", ") + key + " (--std-header " + key + "=<header>)";
  return result;
}

// The real standard headers the callable's records need, as a block of tagged #include lines;
// empty when it uses no standard-library record. A record the callable names directly must
// have a mapping, and one without is reported with the exact entry that would map it. A caller
// extends or overrides the table per probe (ProbeSpec.standard_headers). `standard` receives
// the newest C++ standard any of the records needs (0 when none is newer than C++20).
bool automatic_standard_headers(const clangtracer::v1::ProbeSpec &spec, std::string &block,
                                int &standard, std::string &error) {
  block.clear();
  standard = 0;
  std::vector<::tracer::standard_headers::Entry> user_entries;
  if (!parse_standard_header_entries(spec, user_entries, error))
    return false;
  std::set<std::string> headers;
  for (const auto &use : callable_standard_records(spec)) {
    if (!::tracer::standard_headers::standard_library_scope(use.name))
      continue;
    std::string key;
    const auto *entry = ::tracer::standard_headers::find_entry(use.name, user_entries, key);
    if (!entry) {
      // Left to the header of the record that names it; see unmapped_argument_records().
      if (use.nested)
        continue;
      error = std::string("standard-library ") + (use.enumeration ? "enumeration " : "record ") +
              key + " (" + use.role +
              ") has no header mapping: map it with --std-header " + key +
              "=<header>, or supply its real header with --include or an #include in the "
              "callback";
      return false;
    }
    headers.insert(::tracer::standard_headers::include_line(entry->header));
    standard = std::max(standard, entry->standard);
  }
  for (const auto &line : headers) {
    block += line.substr(0, line.size() - 1);  // drop the newline, then tag the line
    block += kAutomaticHeaderTag;
    block += "\n";
  }
  return true;
}

// The libstdc++ dual ABI the target's debug data shows: 1 for the C++11 ABI
// (std::__cxx11::basic_string), 0 for the old one (std::basic_string), -1 when it shows none or
// both.
int target_cxx11_abi(const clangtracer::v1::ProbeSpec &spec) {
  bool seen_new = spec.symbol().find("__cxx11") != std::string::npos, seen_old = false;
  for (const auto &use : callable_standard_records(spec))
    ::tracer::standard_headers::note_dual_abi_names(use.name, seen_new, seen_old);
  return seen_new == seen_old ? -1 : (seen_new ? 1 : 0);
}

bool split_debug_template_arguments(llvm::StringRef spelling,
                                    std::string &primary,
                                    std::vector<std::string> &arguments) {
  const auto open = spelling.find('<');
  if (open == llvm::StringRef::npos || spelling.empty() ||
      spelling.back() != '>')
    return false;
  primary = normalized_cpp_type(spelling.take_front(open));
  llvm::StringRef remaining = spelling.drop_front(open + 1).drop_back();
  unsigned depth = 0;
  std::size_t begin = 0;
  for (std::size_t i = 0; i < remaining.size(); ++i) {
    if (remaining[i] == '<')
      ++depth;
    else if (remaining[i] == '>') {
      if (!depth)
        return false;
      --depth;
    } else if (remaining[i] == ',' && depth == 0) {
      arguments.push_back(normalized_cpp_type(remaining.slice(begin, i)));
      begin = i + 1;
    }
  }
  if (depth != 0)
    return false;
  arguments.push_back(normalized_cpp_type(remaining.drop_front(begin)));
  return !arguments.empty();
}

// Recovered declarations are parsed in the synthetic main file or have no source
// location. Only declarations from included files belong to real headers.
bool debug_record_from_header(const clang::CXXRecordDecl *record) {
  const auto &sources = record->getASTContext().getSourceManager();
  const auto location = sources.getExpansionLoc(record->getLocation());
  return location.isValid() && !sources.isWrittenInMainFile(location);
}

// Rebuild the declaration scope of a recovered record from the DWARF scope kinds:
// namespaces become namespaces and enclosing classes and unions keep their kind. An enclosing
// record found in the AST (for example from a real header) is reused; a missing one is a bare
// stand-in that says nothing about the original enclosing type's layout or ABI.
// `real_enclosing` is set to the first enclosing record that came from a header.
clang::DeclContext *recovered_debug_scope(clang::ASTContext &context,
                                          const clangtracer::v1::DebugInfo &info,
                                          llvm::StringRef &name, const char *error,
                                          const clang::CXXRecordDecl **real_enclosing = nullptr,
                                          bool create_missing = true) {
  clang::DeclContext *scope = context.getTranslationUnitDecl();
  llvm::StringRef remaining = info.record_name();
  int depth = 0;
  while (::tracer::compat::contains(remaining, "::")) {
    auto part = remaining.split("::");
    if (!debug_identifier(part.first))
      throw std::runtime_error(error);
    const auto kind = depth < info.record_scopes_size()
                          ? info.record_scopes(depth)
                          : clangtracer::v1::RECORD_SCOPE_NAMESPACE;
    const bool is_record = kind != clangtracer::v1::RECORD_SCOPE_NAMESPACE;
    ++depth;
    auto &id = context.Idents.get(part.first);
    clang::DeclContext *next = nullptr;
    for (auto *named : scope->lookup(&id)) {
      if (is_record) {
        if (auto *enclosing = llvm::dyn_cast<clang::CXXRecordDecl>(named)) {
          next = enclosing;
          if (real_enclosing && !*real_enclosing && debug_record_from_header(enclosing))
            *real_enclosing = enclosing;
        }
      } else if (auto *ns = llvm::dyn_cast<clang::NamespaceDecl>(named)) {
        next = ns;
      }
      if (next)
        break;
    }
    if (!next && !create_missing)
      return nullptr;
    if (!next && is_record) {
      auto *enclosing = clang::CXXRecordDecl::Create(
          context,
          kind == clangtracer::v1::RECORD_SCOPE_UNION ? clang::TagTypeKind::Union
                                                      : clang::TagTypeKind::Struct,
          scope, {}, {}, &id);
      if (scope->isRecord())
        enclosing->setAccess(clang::AS_public);
      scope->addDecl(enclosing);
      enclosing->startDefinition();
      enclosing->completeDefinition();
      next = enclosing;
    } else if (!next) {
      auto *ns = clang::NamespaceDecl::Create(context, scope, false, {}, {}, &id,
                                              nullptr, false);
      scope->addDecl(ns);
      next = ns;
    }
    scope = next;
    remaining = part.second;
  }
  name = remaining;
  return scope;
}

// A real enclosing class from a header cannot receive a synthesized nested definition: importing
// it would drag the real parent along and redefine it when the header is parsed again.
void reject_real_enclosing(const clang::CXXRecordDecl *real_enclosing,
                           const clangtracer::v1::DebugInfo &info) {
  if (real_enclosing)
    throw std::runtime_error(
        "header class " + real_enclosing->getQualifiedNameAsString() +
        " does not define its nested record in " + info.record_name() +
        "; the header must define that nested type");
}

bool debug_record_identity_matches(clang::ASTContext &context,
                                   const clang::CXXRecordDecl *record,
                                   llvm::StringRef expected,
                                   std::string *mismatch = nullptr) {
  clang::PrintingPolicy policy(context.getLangOpts());
  policy.SuppressTagKeyword = true;
  policy.SuppressDefaultTemplateArgs = false;
  policy.SuppressInlineNamespace = false;
  // Compare semantic template arguments, not typedef spellings such as string
  // inside hash/equal_to. Preserve ABI-bearing inline namespaces throughout.
  policy.PrintAsCanonical = true;
  policy.UsePreferredNames = false;
  const auto actual = tracer::prepared::record_type(context, record).getAsString(policy);
  if (normalized_cpp_type(actual) == normalized_cpp_type(expected))
    return true;
  if (!standard_library_record_name(expected))
    return false;

  const auto *specialization =
      llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record);
  if (!specialization) {
    if (mismatch)
      *mismatch = "candidate=" + actual + " is not a class-template "
                  "specialization";
    return false;
  }
  std::string expected_primary;
  std::vector<std::string> expected_arguments;
  if (!split_debug_template_arguments(expected, expected_primary,
                                      expected_arguments)) {
    if (mismatch)
      *mismatch = "cannot parse DWARF template identity: " +
                  normalized_cpp_type(expected);
    return false;
  }
  // getQualifiedNameAsString uses a default policy that can hide ABI-bearing
  // inline namespaces such as std::__cxx11. Use the same explicit policy as
  // the complete record and its template arguments.
  std::string primary_name;
  llvm::raw_string_ostream primary_output(primary_name);
  specialization->getSpecializedTemplate()->printQualifiedName(primary_output,
                                                               policy);
  primary_output.flush();
  const auto actual_primary = normalized_cpp_type(primary_name);
  if (actual_primary != expected_primary) {
    if (mismatch)
      *mismatch = "candidate-primary=" + actual_primary +
                  " expected-primary=" + expected_primary + " type=" + actual;
    return false;
  }
  const auto actual_arguments = specialization->getTemplateArgs().asArray();
  if (actual_arguments.size() != expected_arguments.size()) {
    if (mismatch)
      *mismatch = "candidate-template-arity=" +
                  std::to_string(actual_arguments.size()) +
                  " expected-template-arity=" +
                  std::to_string(expected_arguments.size()) + " type=" + actual;
    return false;
  }
  for (std::size_t i = 0; i < actual_arguments.size(); ++i) {
    std::string argument;
    llvm::raw_string_ostream output(argument);
    actual_arguments[i].print(policy, output, true);
    output.flush();
    if (normalized_cpp_type(argument) != expected_arguments[i]) {
      if (mismatch)
        *mismatch = "candidate-template-argument[" + std::to_string(i) +
                    "]=" + normalized_cpp_type(argument) +
                    " expected=" + expected_arguments[i] + " type=" + actual;
      return false;
    }
  }
  return true;
}

bool debug_record_identity_matches(clang::ASTContext &context,
                                   const clang::CXXRecordDecl *record,
                                   const clangtracer::v1::DebugInfo &debug,
                                   std::string *mismatch = nullptr);

// Whether a compiler type is the type DWARF describes. Qualifiers, pointer depth, array
// bounds, function signatures and (through the nested record description) record identity are
// compared as structure; builtin types by size and encoding class, which DWARF names
// differently across compilers.
bool debug_template_type_matches(clang::ASTContext &context, const clang::PrintingPolicy &policy,
                                 clang::QualType type,
                                 const clangtracer::v1::DebugTemplateType &expected,
                                 std::string &why, bool array_const = false,
                                 bool array_volatile = false) {
  using Kind = clangtracer::v1::DebugTemplateType;
  type = context.getCanonicalType(type);
  const auto fail = [&](const std::string &reason) {
    why = reason + " (compiler type " + type.getAsString(policy) + ")";
    return false;
  };
  if (expected.kind() == Kind::VOID)
    return type->isVoidType() ? true : fail("DWARF type is void");
  // The qualifiers of an array belong to its elements: DWARF may put them on either, and the
  // compiler's canonical type keeps them on the element. `array_*` carries them inward.
  const bool expected_const = expected.is_const() || array_const;
  const bool expected_volatile = expected.is_volatile() || array_volatile;
  if (expected.kind() == Kind::ARRAY) {
    const clang::ArrayType *array = nullptr;
    if (expected.has_array_count()) {
      const auto *constant = context.getAsConstantArrayType(type);
      if (!constant || constant->getSize().getZExtValue() != expected.array_count())
        return fail("DWARF type is an array of " + std::to_string(expected.array_count()));
      array = constant;
    } else {
      // No bound in DWARF: an array of unknown bound (T[]).
      array = context.getAsIncompleteArrayType(type);
      if (!array)
        return fail("DWARF type is an array of unknown bound");
    }
    return debug_template_type_matches(context, policy, array->getElementType(),
                                       expected.element(), why, expected_const,
                                       expected_volatile);
  }
  if (type.isLocalConstQualified() != expected_const ||
      type.isLocalVolatileQualified() != expected_volatile)
    return fail("cv-qualification differs from DWARF");
  switch (expected.kind()) {
  case Kind::BUILTIN: {
    if (!type->isBuiltinType() || context.getTypeSize(type) / 8 != expected.byte_size())
      return fail("DWARF builtin " + expected.name() + " has a different size or is not builtin");
    const auto *builtin = type->castAs<clang::BuiltinType>();
    switch (expected.dwarf_encoding()) {
    case llvm::dwarf::DW_ATE_boolean:
      return type->isBooleanType() ? true : fail("DWARF builtin " + expected.name() + " is not bool");
    case llvm::dwarf::DW_ATE_float:
      return type->isFloatingType() ? true : fail("DWARF builtin " + expected.name() + " is not floating");
    case llvm::dwarf::DW_ATE_signed:
    case llvm::dwarf::DW_ATE_signed_char:
    case llvm::dwarf::DW_ATE_unsigned:
    case llvm::dwarf::DW_ATE_unsigned_char: {
      const bool is_signed = expected.dwarf_encoding() == llvm::dwarf::DW_ATE_signed ||
                             expected.dwarf_encoding() == llvm::dwarf::DW_ATE_signed_char;
      if (type->isBooleanType() || (is_signed ? !type->isSignedIntegerType()
                                              : !type->isUnsignedIntegerType()))
        return fail("DWARF builtin " + expected.name() + " has a different signedness");
      // char, signed char and unsigned char share encodings with other types of their size.
      if (expected.name() == "char" && builtin->getKind() != clang::BuiltinType::Char_S &&
          builtin->getKind() != clang::BuiltinType::Char_U)
        return fail("DWARF builtin is plain char");
      if (expected.name() == "signed char" && builtin->getKind() != clang::BuiltinType::SChar)
        return fail("DWARF builtin is signed char");
      if (expected.name() == "unsigned char" && builtin->getKind() != clang::BuiltinType::UChar)
        return fail("DWARF builtin is unsigned char");
      if (expected.name() == "wchar_t" && builtin->getKind() != clang::BuiltinType::WChar_S &&
          builtin->getKind() != clang::BuiltinType::WChar_U)
        return fail("DWARF builtin is wchar_t");
      return true;
    }
    default:
      // char8/16/32_t and other encodings: the builtin's spelling must agree.
      return normalized_cpp_type(type.getAsString(policy)) == normalized_cpp_type(expected.name())
                 ? true
                 : fail("DWARF builtin " + expected.name() + " differs");
    }
  }
  case Kind::ENUM: {
    const auto *enumeration = type->getAs<clang::EnumType>();
    if (!enumeration)
      return fail("DWARF type is enum " + expected.name());
    return normalized_cpp_type(enumeration->getDecl()->getQualifiedNameAsString()) ==
                   normalized_cpp_type(expected.name())
               ? true
               : fail("DWARF enum " + expected.name() + " differs");
  }
  case Kind::POINTER:
    if (!type->isPointerType() || context.getTypeSize(type) / 8 != expected.byte_size())
      return fail("DWARF type is a pointer");
    return debug_template_type_matches(context, policy, type->getPointeeType(), expected.element(),
                                       why);
  case Kind::LVALUE_REFERENCE:
    if (!type->isLValueReferenceType())
      return fail("DWARF type is an lvalue reference");
    return debug_template_type_matches(context, policy, type->getPointeeType(), expected.element(),
                                       why);
  case Kind::RVALUE_REFERENCE:
    if (!type->isRValueReferenceType())
      return fail("DWARF type is an rvalue reference");
    return debug_template_type_matches(context, policy, type->getPointeeType(), expected.element(),
                                       why);
  case Kind::FUNCTION: {
    const auto *function = type->getAs<clang::FunctionProtoType>();
    if (!function || function->getNumParams() != static_cast<unsigned>(expected.parameters_size()) ||
        function->isVariadic() != expected.function_variadic())
      return fail("DWARF type is a function type with different parameters");
    if (!debug_template_type_matches(context, policy, function->getReturnType(), expected.element(),
                                     why))
      return false;
    for (unsigned i = 0; i < function->getNumParams(); ++i)
      if (!debug_template_type_matches(context, policy, function->getParamType(i),
                                       expected.parameters(static_cast<int>(i)), why))
        return false;
    return true;
  }
  case Kind::RECORD: {
    const auto *declaration = type->getAsCXXRecordDecl();
    if (!declaration)
      return fail("DWARF type is record " + expected.name());
    if (expected.has_record())
      return debug_record_identity_matches(context, declaration, expected.record(), &why)
                 ? true
                 : (why.empty() ? fail("DWARF record " + expected.name() + " differs") : false);
    return normalized_cpp_type(type.getAsString(policy)) == normalized_cpp_type(expected.name())
               ? true
               : fail("DWARF record " + expected.name() + " differs");
  }
  default:
    return fail("DWARF template argument type is not described");
  }
}

bool debug_template_arguments_match(clang::ASTContext &context, const clang::PrintingPolicy &policy,
                                    llvm::ArrayRef<clang::TemplateArgument> actual,
                                    const google::protobuf::RepeatedPtrField<
                                        clangtracer::v1::DebugTemplateArgument> &expected,
                                    std::string &why) {
  using Kind = clangtracer::v1::DebugTemplateArgument;
  if (actual.size() != static_cast<std::size_t>(expected.size())) {
    why = "template-arity " + std::to_string(actual.size()) + " differs from DWARF " +
          std::to_string(expected.size());
    return false;
  }
  for (std::size_t i = 0; i < actual.size(); ++i) {
    const auto &argument = actual[i];
    const auto &wanted = expected[static_cast<int>(i)];
    const auto label = "template-argument[" + std::to_string(i) + "]";
    switch (wanted.kind()) {
    case Kind::TYPE:
      if (argument.getKind() != clang::TemplateArgument::Type) {
        why = label + " is not a type argument but DWARF has a type";
        return false;
      }
      if (!debug_template_type_matches(context, policy, argument.getAsType(), wanted.type(), why)) {
        why = label + ": " + why;
        return false;
      }
      break;
    case Kind::VALUE: {
      if (argument.getKind() == clang::TemplateArgument::Integral) {
        if (!wanted.has_constant_value()) {
          why = label + " is a constant but DWARF recorded no integral value";
          return false;
        }
        if (!debug_template_type_matches(context, policy, argument.getIntegralType(), wanted.type(),
                                         why)) {
          why = label + ": " + why;
          return false;
        }
        llvm::SmallString<32> text;
        argument.getAsIntegral().toString(text, 10);
        if (text.str() != wanted.constant_value()) {
          why = label + "=" + text.str().str() + " differs from DWARF value " +
                wanted.constant_value();
          return false;
        }
      } else if (argument.getKind() != clang::TemplateArgument::Declaration &&
                 argument.getKind() != clang::TemplateArgument::NullPtr) {
        why = label + " is not a non-type argument but DWARF has a value";
        return false;
      }
      break;
    }
    case Kind::TEMPLATE: {
      if (argument.getKind() != clang::TemplateArgument::Template &&
          argument.getKind() != clang::TemplateArgument::TemplateExpansion) {
        why = label + " is not a template argument but DWARF has a template";
        return false;
      }
      std::string printed;
      llvm::raw_string_ostream output(printed);
      argument.getAsTemplateOrTemplatePattern().print(output, policy);
      output.flush();
      if (normalized_cpp_type(printed) != normalized_cpp_type(wanted.template_name())) {
        why = label + "=" + normalized_cpp_type(printed) + " differs from DWARF template " +
              normalized_cpp_type(wanted.template_name());
        return false;
      }
      break;
    }
    case Kind::PACK:
      if (argument.getKind() != clang::TemplateArgument::Pack) {
        why = label + " is not a parameter pack but DWARF has a pack";
        return false;
      }
      if (!debug_template_arguments_match(context, policy, argument.pack_elements(),
                                          wanted.pack_elements(), why)) {
        why = label + " pack: " + why;
        return false;
      }
      break;
    default:
      why = label + " has no described kind in DWARF";
      return false;
    }
  }
  return true;
}

// A record identity check that uses the DWARF template parameters when the target's debug data
// provided them, and falls back to the display name otherwise. With parameters, the primary
// template and every argument are compared by kind and structure; the display name is not
// re-parsed.
bool debug_record_identity_matches(clang::ASTContext &context,
                                   const clang::CXXRecordDecl *record,
                                   const clangtracer::v1::DebugInfo &debug,
                                   std::string *mismatch) {
  if (debug.template_arguments_size() == 0 || !standard_library_record_name(debug.record_name()))
    return debug_record_identity_matches(context, record, debug.record_name(), mismatch);
  const auto *specialization = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record);
  if (!specialization) {
    if (mismatch)
      *mismatch = "candidate is not a class-template specialization but DWARF record " +
                  debug.record_name() + " has template arguments";
    return false;
  }
  clang::PrintingPolicy policy(context.getLangOpts());
  policy.SuppressTagKeyword = true;
  policy.SuppressDefaultTemplateArgs = false;
  policy.SuppressInlineNamespace = false;
  policy.PrintAsCanonical = true;
  policy.UsePreferredNames = false;
  // The primary template is the display name before its argument list; that part carries no
  // arguments, so it is compared as a name.
  std::string primary;
  llvm::raw_string_ostream primary_output(primary);
  specialization->getSpecializedTemplate()->printQualifiedName(primary_output, policy);
  primary_output.flush();
  const auto open = debug.record_name().find('<');
  const auto expected_primary =
      open == std::string::npos ? debug.record_name() : debug.record_name().substr(0, open);
  if (normalized_cpp_type(primary) != normalized_cpp_type(expected_primary)) {
    if (mismatch)
      *mismatch = "candidate-primary=" + normalized_cpp_type(primary) +
                  " expected-primary=" + normalized_cpp_type(expected_primary);
    return false;
  }
  std::string why;
  if (!debug_template_arguments_match(context, policy, specialization->getTemplateArgs().asArray(),
                                      debug.template_arguments(), why)) {
    if (mismatch)
      *mismatch = "candidate=" + primary + ": " + why;
    return false;
  }
  return true;
}

bool debug_path_identifier(llvm::StringRef text) {
  if (text.size() > 63 || text.empty())
    return false;
  while (true) {
    auto part = text.split('.');
    if (!debug_identifier(part.first))
      return false;
    if (part.second.empty())
      return !::tracer::compat::ends_with(text, ".");
    text = part.second;
  }
}

clang::QualType debug_builtin(clang::ASTContext &context,
                              llvm::StringRef type) {
  if (::tracer::compat::ends_with(type, " const")) {
    auto t = debug_builtin(context, type.drop_back(6));
    t.addConst();
    return t;
  }
  if (::tracer::compat::ends_with(type, " volatile")) {
    auto t = debug_builtin(context, type.drop_back(9));
    t.addVolatile();
    return t;
  }
  if (::tracer::compat::ends_with(type, "*")) {
    return context.getPointerType(debug_builtin(context, type.drop_back()));
  }
  if (::tracer::compat::starts_with(type, "const ")) {
    auto t = debug_builtin(context, type.drop_front(6));
    t.addConst();
    return t;
  }
  if (type == "void")
    return context.VoidTy;
  if (type == "bool")
    return context.BoolTy;
  if (type == "char")
    return context.CharTy;
  if (type == "signed char")
    return context.SignedCharTy;
  if (type == "unsigned char")
    return context.UnsignedCharTy;
  if (type == "wchar_t")
    return context.WCharTy;
  if (type == "char16_t")
    return context.Char16Ty;
  if (type == "char32_t")
    return context.Char32Ty;
  if (type == "short" || type == "short int")
    return context.ShortTy;
  if (type == "unsigned short" || type == "unsigned short int")
    return context.UnsignedShortTy;
  if (type == "int")
    return context.IntTy;
  if (type == "unsigned int")
    return context.UnsignedIntTy;
  if (type == "long" || type == "long int")
    return context.LongTy;
  if (type == "unsigned long" || type == "unsigned long int" ||
      type == "long unsigned int")
    return context.UnsignedLongTy;
  if (type == "long long" || type == "long long int")
    return context.LongLongTy;
  if (type == "unsigned long long" || type == "unsigned long long int" ||
      type == "long long unsigned int")
    return context.UnsignedLongLongTy;
  if (type == "float")
    return context.FloatTy;
  if (type == "double")
    return context.DoubleTy;
  throw std::runtime_error("unsupported DWARF builtin: " + type.str());
}

bool matches_debug_type_chain(clang::QualType type, clang::ASTContext &context,
                              const clangtracer::v1::DebugField &field) {
  if (field.type_chain_size() < 1 || field.type_chain_size() > 33)
    return false;
  for (int i = 0; i < field.type_chain_size(); ++i) {
    const auto &level = field.type_chain(i);
    type = type.getCanonicalType();
    if (type.isConstQualified() != level.is_const() ||
        type.isVolatileQualified() != level.is_volatile() ||
        type.getAddressSpace() != clang::LangAS::Default)
      return false;
    if (type->isPointerType()) {
      if (level.kind() != clangtracer::v1::DebugTypeLevel::POINTER ||
          context.getTypeSize(type) / 8 != level.byte_size() ||
          level.dwarf_encoding())
        return false;
      type = type->getPointeeType();
      continue;
    }
    if (i + 1 != field.type_chain_size())
      return false;
    if (type->isVoidType())
      return level.kind() == clangtracer::v1::DebugTypeLevel::VOID &&
             !level.byte_size() && !level.dwarf_encoding();
    if (!type->isBuiltinType() ||
        level.kind() != clangtracer::v1::DebugTypeLevel::BUILTIN ||
        !level.byte_size() ||
        context.getTypeSize(type) / 8 != level.byte_size())
      return false;
    switch (level.dwarf_encoding()) {
    case llvm::dwarf::DW_ATE_boolean:
      return type->isBooleanType();
    case llvm::dwarf::DW_ATE_float:
      return type->isFloatingType();
    case llvm::dwarf::DW_ATE_signed:
    case llvm::dwarf::DW_ATE_signed_char:
      return type->isSignedIntegerType();
    case llvm::dwarf::DW_ATE_unsigned:
    case llvm::dwarf::DW_ATE_unsigned_char:
      return type->isUnsignedIntegerType() && !type->isBooleanType();
    case llvm::dwarf::DW_ATE_UTF:
      return type->isChar16Type() || type->isChar32Type();
    default:
      return false;
    }
  }
  return false; // A pointer chain without its terminal type is incomplete.
}

// Checks a record that Clang laid out against the DWARF description of the record, recursively
// through bases and by-value members. Every difference throws and names the record, the member
// and both values, because a rebuilt layout is only trusted when DWARF agrees with it entirely.
void verify_debug_layout(clang::ASTContext &context, const clang::CXXRecordDecl *declared,
                         const clangtracer::v1::DebugInfo &expected, const std::string &path) {
  const auto mismatch = [&](const std::string &what) {
    throw std::runtime_error("Clang layout of " + path + " does not match DWARF: " + what);
  };
  const auto *record = declared ? declared->getDefinition() : nullptr;
  if (!record || record->isDependentType())
    mismatch("it is not a complete class in the rebuilt declarations");
  if (record->isUnion() != expected.record_is_union())
    mismatch(std::string("it is ") + (record->isUnion() ? "a union" : "not a union") +
             " in Clang and " + (expected.record_is_union() ? "a union" : "not a union") +
             " in DWARF");
  if (record->isDynamicClass() || record->getNumVBases() != 0)
    mismatch("it is a dynamic class or has virtual bases in Clang");
  const auto &layout = context.getASTRecordLayout(record);
  const auto size = static_cast<std::uint64_t>(layout.getSize().getQuantity());
  if (size != expected.record_size())
    mismatch("its size is " + std::to_string(size) + " bytes in Clang and " +
             std::to_string(expected.record_size()) + " in DWARF");
  const auto alignment = static_cast<std::uint64_t>(layout.getAlignment().getQuantity());
  if (expected.record_alignment() && alignment != expected.record_alignment())
    mismatch("its alignment is " + std::to_string(alignment) + " in Clang and " +
             std::to_string(expected.record_alignment()) + " in DWARF");
  if (record->getNumBases() != static_cast<unsigned>(expected.bases_size()))
    mismatch("it has " + std::to_string(record->getNumBases()) + " base classes in Clang and " +
             std::to_string(expected.bases_size()) + " in DWARF");
  int base_index = 0;
  for (const auto &base : record->bases()) {
    const auto &wanted = expected.bases(base_index++);
    const auto *base_record = base.getType()->getAsCXXRecordDecl();
    if (!base_record || base_record->getQualifiedNameAsString() != wanted.record().record_name())
      mismatch("base class " + wanted.record().record_name() + " is " +
               (base_record ? base_record->getQualifiedNameAsString() : std::string("not a class")) +
               " in Clang");
    const auto offset = layout.getBaseClassOffset(base_record).getQuantity();
    if (static_cast<std::uint64_t>(offset) != wanted.byte_offset())
      mismatch("base class " + wanted.record().record_name() + " is at offset " +
               std::to_string(offset) + " in Clang and " + std::to_string(wanted.byte_offset()) +
               " in DWARF");
    if (field_access_name(base.getAccessSpecifier(), false) != wanted.access())
      mismatch("base class " + wanted.record().record_name() + " is " +
               field_access_name(base.getAccessSpecifier(), false) + " in Clang and " +
               wanted.access() + " in DWARF");
    verify_debug_layout(context, base_record, wanted.record(),
                        path + " base " + wanted.record().record_name());
  }
  std::vector<const clang::FieldDecl *> fields;
  for (const auto *field : record->fields())
    if (!field->isUnnamedBitField())
      fields.push_back(field);
  if (fields.size() != static_cast<std::size_t>(expected.fields_size()))
    mismatch("it has " + std::to_string(fields.size()) + " data members in Clang and " +
             std::to_string(expected.fields_size()) + " in DWARF");
  for (std::size_t i = 0; i < fields.size(); ++i) {
    const auto *field = fields[i];
    const auto &wanted = expected.fields(static_cast<int>(i));
    const auto where = "member " + (wanted.name().empty() ? std::string("(anonymous)") : wanted.name());
    if (field->getNameAsString() != wanted.name())
      mismatch(where + " is named '" + field->getNameAsString() + "' in Clang");
    if (field_access_name(field->getAccess(), record->isClass()) != wanted.access())
      mismatch(where + " is " + field_access_name(field->getAccess(), record->isClass()) +
               " in Clang and " + wanted.access() + " in DWARF");
    const auto offset = layout.getFieldOffset(field->getFieldIndex());
    const auto wanted_offset = wanted.bit_size() ? wanted.bit_offset() : wanted.byte_offset() * 8;
    if (offset != wanted_offset)
      mismatch(where + " is at bit offset " + std::to_string(offset) + " in Clang and " +
               std::to_string(wanted_offset) + " in DWARF");
    if (field->isBitField() != (wanted.bit_size() != 0) ||
        (field->isBitField() && field->getBitWidthValue() != wanted.bit_size()))
      mismatch(where + " has a different bit-field width in Clang (" +
               std::to_string(field->isBitField() ? field->getBitWidthValue() : 0) +
               ") and DWARF (" + std::to_string(wanted.bit_size()) + ")");
    auto type = field->getType();
    for (const auto extent : wanted.array_extents()) {
      const auto *array = context.getAsConstantArrayType(type);
      if (!array || array->getSize().getZExtValue() != extent)
        mismatch(where + " is not an array of " + std::to_string(extent) + " elements in Clang");
      type = array->getElementType();
    }
    if (context.getAsArrayType(type))
      mismatch(where + " has more array dimensions in Clang than in DWARF");
    if (wanted.has_record()) {
      const auto *element = type->getAsCXXRecordDecl();
      const auto &described = wanted.record();
      if (!element ||
          (described.record_name().empty()
               ? !element->getDeclName().isEmpty()
               : element->getQualifiedNameAsString() != described.record_name()))
        mismatch(where + " has a different class type in Clang than " +
                 (described.record_name().empty() ? std::string("DWARF's unnamed class")
                                                  : described.record_name()));
      verify_debug_layout(context, element, described, path + "::" + wanted.name());
    } else {
      if (!matches_debug_type_chain(type, context, wanted))
        mismatch(where + " has a type whose ABI differs from DWARF");
      if (!context.hasSameType(type, debug_builtin(context, wanted.builtin_type())))
        mismatch(where + " has the type " + type.getAsString() + " in Clang and " +
                 wanted.builtin_type() + " in DWARF");
    }
    const auto bits = context.getTypeSize(field->getType());
    if (bits != wanted.byte_size() * 8)
      mismatch(where + " is " + std::to_string(bits) + " bits in Clang and " +
               std::to_string(wanted.byte_size() * 8) + " in DWARF");
  }
}

DebugRecord construct_debug_record(clang::ASTUnit &unit,
                                   const clangtracer::v1::DebugInfo &info) {
  auto &context = unit.getASTContext();
  class Finder : public clang::RecursiveASTVisitor<Finder> {
  public:
    clang::ASTContext *context = nullptr;
    const clangtracer::v1::DebugInfo *debug = nullptr;
    std::string selected;
    clang::CXXRecordDecl *found = nullptr;
    std::vector<clang::CXXRecordDecl *> incomplete;
    std::string candidate_mismatch;
    bool shouldVisitTemplateInstantiations() const { return true; }
    bool VisitCXXRecordDecl(clang::CXXRecordDecl *d) {
      if (d->isDependentType())
        return true;
      if (!d->isCompleteDefinition()) {
        std::string mismatch;
        if (standard_library_record_name(selected) &&
            debug_record_identity_matches(*context, d, *debug, &mismatch))
          incomplete.push_back(d);
        else if (!mismatch.empty() && ::tracer::compat::starts_with(
                     selected, d->getQualifiedNameAsString() + "<"))
          candidate_mismatch = std::move(mismatch);
        return true;
      }
      if (d->getQualifiedNameAsString() == selected &&
          !llvm::isa<clang::ClassTemplateSpecializationDecl>(d)) {
        found = d;
        return true;
      }
      std::string mismatch;
      if (debug_record_identity_matches(*context, d, *debug, &mismatch))
        found = d;
      else if (!mismatch.empty() &&
               (::tracer::compat::starts_with(
                    selected, d->getQualifiedNameAsString() + "<") ||
                (candidate_mismatch.empty() &&
                 ::tracer::compat::contains(d->getQualifiedNameAsString(),
                                            "basic_string"))))
        candidate_mismatch = std::move(mismatch);
      return true;
    }
  } finder;
  finder.context = &context;
  finder.debug = &info;
  finder.selected = info.record_name();
  finder.TraverseDecl(context.getTranslationUnitDecl());
  // A reference-only declaration need not instantiate an STL specialization.
  // Complete the matching compiler-owned type after traversal, without parsing
  // DWARF strings as C++ or relaxing the identity/layout checks below.
  if (!finder.found && unit.hasSema()) {
    for (auto *candidate : finder.incomplete) {
      if (unit.getSema().isCompleteType(candidate->getLocation(),
                                      tracer::prepared::record_type(context, candidate))) {
        auto *definition = candidate->getDefinition();
        if (definition && debug_record_identity_matches(context, definition, info)) {
          finder.found = definition;
          break;
        }
      }
    }
  }
  auto *record = finder.found;
  const bool standard_library_type =
      ::tracer::compat::starts_with(info.record_name(), "std::") ||
      ::tracer::compat::starts_with(info.record_name(), "__gnu_cxx::") ||
      ::tracer::compat::starts_with(info.record_name(), "__cxxabiv1::");
  if (!record && standard_library_type)
    throw std::runtime_error(
        "standard-library ABI mismatch: target DWARF type " +
        info.record_name() +
        " is not the type selected by callback headers and compiler options" +
        (finder.candidate_mismatch.empty()
             ? std::string()
             : "; candidate mismatch: " + finder.candidate_mismatch));
  // A standard-library specialization that a real header defines is the compiler's type: its
  // members are the library's business and are not rebuilt or compared field by field, even
  // when DWARF could describe them (std::chrono::duration holds one integer). Its identity was
  // matched above; its size and alignment are compared with the target's here.
  const bool header_standard_record =
      record && standard_library_type && debug_record_from_header(record) &&
      llvm::isa<clang::ClassTemplateSpecializationDecl>(record);
  // A record that a real header defines was opaque before DWARF described bases, unions and
  // record, array and bit-field members, and it is accepted the same way now: by size and
  // declared alignment. Its members are the header's business.
  const bool header_recovered_layout =
      record && !info.record_is_opaque() && tracer::client::uses_recovered_layout(info) &&
      debug_record_from_header(record);
  if (record && (info.record_is_opaque() || header_standard_record || header_recovered_layout)) {
    if (!record->isCompleteDefinition())
      throw std::runtime_error("header record is incomplete for opaque DWARF type");
    // An opaque record's alignment is the one its DIE declares or none. A described one also
    // has a computed alignment, which is compared only when it is the declared one.
    const std::uint64_t compared_alignment =
        header_recovered_layout && !info.record_alignment_declared() ? 0
                                                                     : info.record_alignment();
    const auto &layout = context.getASTRecordLayout(record);
    if ((info.record_size() &&
         static_cast<std::uint64_t>(layout.getSize().getQuantity()) !=
             info.record_size()) ||
        (compared_alignment &&
         static_cast<std::uint64_t>(layout.getAlignment().getQuantity()) != compared_alignment))
      throw std::runtime_error(
          "header record size/alignment does not match opaque DWARF type");
    clangtracer::v1::ReceiverContext metadata;
    metadata.set_record_name(info.record_name());
    metadata.set_size(info.record_size());
    metadata.set_alignment(compared_alignment);
    metadata.set_is_volatile(info.receiver_volatile());
    return {record, std::move(metadata), true};
  }
  if (!record && info.record_is_opaque()) {
    llvm::StringRef remaining;
    const clang::CXXRecordDecl *real_enclosing = nullptr;
    clang::DeclContext *scope = recovered_debug_scope(
        context, info, remaining, "unsupported debug record scope", &real_enclosing);
    reject_real_enclosing(real_enclosing, info);
    if (!debug_identifier(remaining))
      throw std::runtime_error("unsupported debug record name");
    record = clang::CXXRecordDecl::Create(
        context, clang::TagTypeKind::Struct, scope, {}, {},
        &context.Idents.get(remaining));
    if (scope->isRecord())
      record->setAccess(clang::AS_public);
    scope->addDecl(record);
    clangtracer::v1::ReceiverContext metadata;
    metadata.set_record_name(info.record_name());
    metadata.set_size(info.record_size());
    metadata.set_alignment(info.record_alignment());
    metadata.set_is_volatile(info.receiver_volatile());
    return {record, std::move(metadata), true};
  }
  const bool recovered_layout = tracer::client::uses_recovered_layout(info);
  if (!record && recovered_layout)
    // The members of such a record come from the declarations built out of DWARF.
    throw std::runtime_error("receiver " + info.record_name() +
                             " was not declared from debug data");
  if (!record) {
    llvm::StringRef remaining;
    const clang::CXXRecordDecl *real_enclosing = nullptr;
    clang::DeclContext *scope = recovered_debug_scope(
        context, info, remaining, "unsupported debug receiver scope", &real_enclosing);
    reject_real_enclosing(real_enclosing, info);
    if (!debug_identifier(remaining))
      throw std::runtime_error("unsupported debug receiver name");
    record =
        clang::CXXRecordDecl::Create(context, clang::TagTypeKind::Class, scope,
                                     {}, {}, &context.Idents.get(remaining));
    if (scope->isRecord())
      record->setAccess(clang::AS_public);
    scope->addDecl(record);
    record->startDefinition();
    for (const auto &field : info.fields()) {
      if (!debug_identifier(field.name()))
        throw std::runtime_error("unsupported debug field name");
      auto type = debug_builtin(context, field.builtin_type());
      auto *decl = clang::FieldDecl::Create(
          context, record, {}, {}, &context.Idents.get(field.name()), type,
          context.getTrivialTypeSourceInfo(type), nullptr, false,
          clang::ICIS_NoInit);
      decl->setAccess(field.access() == "private"     ? clang::AS_private
                      : field.access() == "protected" ? clang::AS_protected
                                                      : clang::AS_public);
      record->addDecl(decl);
    }
    record->completeDefinition();
    record->setArgPassingRestrictions(
        info.record_calling_convention() ==
                clangtracer::v1::RECORD_PASS_BY_REFERENCE
            ? clang::RecordArgPassingKind::CannotPassInRegs
            : clang::RecordArgPassingKind::CanPassInRegs);
  }
  if (recovered_layout) {
    verify_debug_layout(context, record, info, info.record_name());
    const auto &rich_layout = context.getASTRecordLayout(record);
    clangtracer::v1::ReceiverContext rich;
    rich.set_record_name(info.record_name());
    rich.set_size(info.record_size());
    rich.set_alignment(rich_layout.getAlignment().getQuantity());
    rich.set_is_volatile(info.receiver_volatile());
    rich.set_layout_identity(record_layout_identity(context, record));
    std::string rich_error;
    if (!validate_receiver_layout(context, record, rich, rich_error))
      throw std::runtime_error(rich_error);
    return {record, std::move(rich), false};
  }
  if (record->isUnion() || record->isDynamicClass() || record->getNumBases() ||
      std::distance(record->field_begin(), record->field_end()) !=
          info.fields_size())
    throw std::runtime_error("unsupported DWARF receiver layout");
  const auto &layout = context.getASTRecordLayout(record);
  if (static_cast<std::uint64_t>(layout.getSize().getQuantity()) !=
      info.record_size())
    throw std::runtime_error("Clang receiver size does not match DWARF");
  clangtracer::v1::ReceiverContext metadata;
  auto *receiver = &metadata;
  receiver->Clear();
  receiver->set_record_name(info.record_name());
  receiver->set_size(info.record_size());
  receiver->set_alignment(layout.getAlignment().getQuantity());
  receiver->set_is_volatile(info.receiver_volatile());
  unsigned i = 0;
  clang::PrintingPolicy policy(context.getLangOpts());
  for (auto *field : record->fields()) {
    const auto &expected = info.fields(i);
    if (!matches_debug_type_chain(field->getType(), context, expected))
      throw std::runtime_error("receiver field type ABI does not match DWARF");
    if (field->getNameAsString() != expected.name() || field->isBitField() ||
        !context.hasSameType(field->getType(),
                             debug_builtin(context, expected.builtin_type())) ||
        layout.getFieldOffset(i) != expected.byte_offset() * 8 ||
        context.getTypeSize(field->getType()) != expected.byte_size() * 8 ||
        field_access_name(field->getAccess(), record->isClass()) !=
            expected.access())
      throw std::runtime_error(
          "Clang field layout/access does not match DWARF");
    auto *f = receiver->add_fields();
    f->set_name(expected.name());
    f->set_canonical_type(
        field->getType().getCanonicalType().getAsString(policy));
    f->set_bit_offset(layout.getFieldOffset(i++));
    f->set_bit_size(context.getTypeSize(field->getType()));
    f->set_bit_alignment(context.getTypeAlign(field->getType()));
    f->set_access(expected.access());
    f->set_is_mutable(field->isMutable());
    f->set_abi_identity(
        tracer::receiver_abi::type_identity(field->getType(), context));
  }

  std::string error;
  if (!validate_receiver_layout(context, record, metadata, error))
    throw std::runtime_error(error);
  return {record, std::move(metadata), false};
}

// DWARF describes the concrete layout, but cannot recreate the primary C++
// template. Give each recovered specialization a private compiler name and
// keep the original selector and linkage symbol for entry verification.
std::optional<clangtracer::v1::ProbeSpec> template_recovery_spec(
    const clangtracer::v1::ProbeSpec &target) {
  const auto &debug = target.debug_info();
  std::unordered_map<std::string, std::string> names;
  std::unordered_map<std::string, std::string> identities;
  const auto digest_key = [](const std::string &text) {
    const auto digest = llvm::SHA256::hash(llvm::ArrayRef<std::uint8_t>(
        reinterpret_cast<const std::uint8_t *>(text.data()), text.size()));
    // Keep the generated identifier within the recovered identifier limit.
    return llvm::toHex(llvm::ArrayRef<std::uint8_t>(digest).take_front(16), true);
  };
  const auto add = [&](const clangtracer::v1::DebugInfo &record) {
    const auto &name = record.record_name();
    if (!record.record_identity().empty()) {
      // A record in an anonymous namespace has no spelling that names it. Derive its private
      // name from the mapped debug image and the unit and DIE that describe it, so two
      // same-named anonymous types never share a callback type.
      const auto previous = identities.try_emplace(name, record.record_identity());
      if (!previous.second && previous.first->second != record.record_identity())
        throw std::runtime_error("two anonymous-namespace records share the display name " + name);
      names.try_emplace(
          name, "tracer_dwarf_types::anonymous_" +
                    digest_key(debug.module() + '\n' + std::to_string(debug.device()) + ':' +
                               std::to_string(debug.inode()) + ':' +
                               std::to_string(debug.file_size()) + '\n' +
                               record.record_identity()));
      return;
    }
    if (name.find('<') == std::string::npos || standard_library_record_name(name))
      return;
    names.try_emplace(name, "tracer_dwarf_types::record_" + digest_key(name));
  };
  add(debug);
  for (const auto &argument : debug.arguments())
    if (argument.has_object_type())
      add(argument.object_type());
  if (debug.has_result_object_type())
    add(debug.result_object_type());
  if (names.empty())
    return std::nullopt;

  const auto rename_type = [&](std::string &type) {
    // Recovered spellings consist of a base type and CV/pointer/reference
    // modifiers. Match the base as a whole, including any nested template
    // arguments, so two specializations cannot be confused.
    const auto base = std::string_view(type);
    const auto unqualified = ::tracer::compat::starts_with(base, "const ") ? base.substr(6) : base;
    for (const auto &[original, recovered] : names) {
      if (!::tracer::compat::starts_with(unqualified, original))
        continue;
      const auto suffix = unqualified.substr(original.size());
      if (!suffix.empty() && suffix.front() != ' ' &&
          suffix.front() != '*' && suffix.front() != '&')
        continue;
      type.replace(base.size() - unqualified.size(), original.size(), recovered);
      break;
    }
  };
  const auto rename_record = [&](clangtracer::v1::DebugInfo &record) {
    if (const auto found = names.find(record.record_name()); found != names.end()) {
      record.set_record_name(found->second);
      // The private name lives in a namespace, whatever scopes the original had.
      record.clear_record_scopes();
    }
    for (auto &method : *record.mutable_methods()) {
      rename_type(*method.mutable_cpp_result_type());
      for (auto &parameter : *method.mutable_cpp_parameter_types())
        rename_type(parameter);
    }
  };
  auto recovered = target;
  auto &info = *recovered.mutable_debug_info();
  rename_record(info);
  for (auto &argument : *info.mutable_arguments()) {
    rename_type(*argument.mutable_cpp_type());
    if (argument.has_object_type())
      rename_record(*argument.mutable_object_type());
  }
  rename_type(*info.mutable_result_cpp_type());
  if (info.has_result_object_type())
    rename_record(*info.mutable_result_object_type());
  return recovered;
}

// C++ spelling of a record or type that DWARF described structurally, for asking the compiler
// for the real standard-library type. Every name is checked before it reaches source text, and
// a constant is emitted as a cast of its decimal value; nothing is taken from a display name
// except the name of the primary template. The result only locates the type: the compiler's
// type must then match the DWARF description (debug_record_identity_matches) before it is used.
std::string debug_record_spelling(const clangtracer::v1::DebugInfo &info);

std::string debug_scope_spelling(llvm::StringRef name) {
  std::string result;
  while (true) {
    auto part = name.split("::");
    if (!debug_identifier(part.first))
      throw std::runtime_error("cannot spell DWARF name " + name.str() + " safely");
    result += (result.empty() ? "" : "::") + part.first.str();
    if (part.second.empty())
      return result;
    name = part.second;
  }
}

// `declarator` is what the type is wrapped around, built from the outside in, so that a
// pointer or reference to an array or function gets its parentheses: int (*)(int), int (&)[4],
// never int(int)*. The qualifiers of an array are spelled on its element.
std::string debug_template_type_spelling(const clangtracer::v1::DebugTemplateType &type,
                                         std::string declarator = {}, bool array_const = false,
                                         bool array_volatile = false) {
  using Kind = clangtracer::v1::DebugTemplateType;
  const bool is_const = type.is_const() || array_const;
  const bool is_volatile = type.is_volatile() || array_volatile;
  const auto qualifiers = [&] {
    return std::string(is_const ? " const" : "") + (is_volatile ? " volatile" : "");
  };
  std::string base;
  switch (type.kind()) {
  case Kind::VOID:
    base = "void";
    break;
  case Kind::BUILTIN: {
    // DWARF builtin names are C++ spellings (long unsigned int, short int, ...) of one to
    // four words; nothing else may reach the source.
    for (const char c : type.name())
      if (!((c >= 'a' && c <= 'z') || c == ' ' || (c >= '0' && c <= '9') || c == '_'))
        throw std::runtime_error("cannot spell DWARF builtin " + type.name() + " safely");
    if (type.name().empty())
      throw std::runtime_error("unnamed DWARF builtin template argument");
    base = type.name();
    break;
  }
  case Kind::ENUM:
    base = "::" + debug_scope_spelling(type.name());
    break;
  case Kind::RECORD:
    base = type.has_record() ? debug_record_spelling(type.record())
                             : "::" + debug_scope_spelling(type.name());
    break;
  case Kind::POINTER:
  case Kind::LVALUE_REFERENCE:
  case Kind::RVALUE_REFERENCE: {
    declarator = (type.kind() == Kind::POINTER ? "*"
                  : type.kind() == Kind::LVALUE_REFERENCE ? "&" : "&&") +
                 qualifiers() + declarator;
    if (type.element().kind() == Kind::ARRAY || type.element().kind() == Kind::FUNCTION)
      declarator = "(" + declarator + ")";
    return debug_template_type_spelling(type.element(), std::move(declarator));
  }
  case Kind::ARRAY:
    declarator += type.has_array_count() ? "[" + std::to_string(type.array_count()) + "]" : "[]";
    return debug_template_type_spelling(type.element(), std::move(declarator), is_const,
                                        is_volatile);
  case Kind::FUNCTION: {
    declarator += "(";
    for (int i = 0; i < type.parameters_size(); ++i)
      declarator += (i ? ", " : "") + debug_template_type_spelling(type.parameters(i));
    if (type.function_variadic())
      declarator += type.parameters_size() ? ", ..." : "...";
    declarator += ")";
    return debug_template_type_spelling(type.element(), std::move(declarator));
  }
  default:
    throw std::runtime_error("template argument type is not described");
  }
  return base + qualifiers() + (declarator.empty() ? "" : " " + declarator);
}

std::string debug_template_argument_spelling(const clangtracer::v1::DebugTemplateArgument &argument) {
  using Kind = clangtracer::v1::DebugTemplateArgument;
  switch (argument.kind()) {
  case Kind::TYPE:
    return debug_template_type_spelling(argument.type());
  case Kind::VALUE: {
    const auto &text = argument.constant_value();
    const auto digits = !text.empty() && text.front() == '-' ? text.substr(1) : text;
    if (!argument.has_constant_value() || digits.empty() || digits.size() > 20 ||
        !std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; }))
      throw std::runtime_error("template value argument has no integral constant");
    return "static_cast<" + debug_template_type_spelling(argument.type()) + ">(" + text + ")";
  }
  case Kind::TEMPLATE:
    return "::" + debug_scope_spelling(argument.template_name());
  case Kind::PACK: {
    std::string result;
    for (int i = 0; i < argument.pack_elements_size(); ++i)
      result += (i ? ", " : "") + debug_template_argument_spelling(argument.pack_elements(i));
    return result;
  }
  default:
    throw std::runtime_error("template argument has no described kind");
  }
}

std::string debug_record_spelling(const clangtracer::v1::DebugInfo &info) {
  const auto open = info.record_name().find('<');
  const auto primary = info.record_name().substr(0, open);
  std::string result = "::" + debug_scope_spelling(primary);
  if (info.template_arguments_size() == 0) {
    if (open == std::string::npos)
      return result;
    // DWARF may describe an instantiation only by a declaration, which has no template
    // parameter children. Its display name is then the only description, so it is used as a
    // request alone, after every character of it has been checked; identity is still decided
    // by comparing the compiler's type with the enclosing structure.
    // Only what a type-id is made of may pass: names, template brackets, declarator
    // punctuation and integer constants. No quote, brace, semicolon or assignment can occur,
    // so the text cannot end the alias it is placed in.
    std::vector<char> open_brackets;
    for (const char c : info.record_name()) {
      const bool word = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_';
      if (c == '<' || c == '(' || c == '[') {
        open_brackets.push_back(c == '<' ? '>' : c == '(' ? ')' : ']');
      } else if (c == '>' || c == ')' || c == ']') {
        if (open_brackets.empty() || open_brackets.back() != c)
          throw std::runtime_error("cannot spell DWARF name " + info.record_name() + " safely");
        open_brackets.pop_back();
      } else if (!word && c != ':' && c != ',' && c != ' ' && c != '*' && c != '&' && c != '-') {
        throw std::runtime_error("cannot spell DWARF name " + info.record_name() + " safely");
      }
    }
    if (!open_brackets.empty())
      throw std::runtime_error("cannot spell DWARF name " + info.record_name() + " safely");
    return "::" + info.record_name();
  }
  result += "<";
  for (int i = 0; i < info.template_arguments_size(); ++i) {
    // A pack of no elements spells nothing, and no comma either.
    const auto text = debug_template_argument_spelling(info.template_arguments(i));
    if (text.empty())
      continue;
    result += (result.back() == '<' ? "" : ", ") + text;
  }
  // `>>` would close two lists in C++98; keep a space so any language mode parses it.
  return result + " >";
}

// Records that only a template argument reaches and that the real headers cannot supply: the
// program's own types (a std::vector<Person> needs Person). They are recovered from DWARF like
// any other record the program defines; standard-library records are never rebuilt.
void collect_argument_records(const clangtracer::v1::DebugTemplateType &type,
                              std::vector<const clangtracer::v1::DebugInfo *> &records);

void collect_argument_records(const clangtracer::v1::DebugInfo &info,
                              std::vector<const clangtracer::v1::DebugInfo *> &records) {
  // Records named only as arguments of a standard-library record (std::vector<Person>); a
  // program template's own arguments are recovered the way they always were.
  if (!standard_library_record_name(info.record_name()))
    return;
  const auto visit = [&](const auto &self, const clangtracer::v1::DebugTemplateArgument &argument)
      -> void {
    if (argument.has_type())
      collect_argument_records(argument.type(), records);
    for (const auto &element : argument.pack_elements())
      self(self, element);
  };
  for (const auto &argument : info.template_arguments())
    visit(visit, argument);
}

void collect_argument_records(const clangtracer::v1::DebugTemplateType &type,
                              std::vector<const clangtracer::v1::DebugInfo *> &records) {
  if (type.kind() == clangtracer::v1::DebugTemplateType::RECORD && type.has_record()) {
    const auto &record = type.record();
    if (!standard_library_record_name(record.record_name()) &&
        record.record_name().find('<') == std::string::npos)
      records.push_back(&record);
    collect_argument_records(record, records);
  }
  if (type.has_element())
    collect_argument_records(type.element(), records);
  for (const auto &parameter : type.parameters())
    collect_argument_records(parameter, records);
}

// The program's own types that a standard-library record names as template arguments, sorted
// by what the headerless route can do with each. The real headers supply the standard record
// but not these, so each is either declared from DWARF or reported by name.
struct ArgumentProgramTypes {
  struct Named {
    std::string owner;   // The standard-library record whose argument it is.
    std::string name;
    std::string reason;  // Why it cannot be declared; empty otherwise.
  };
  std::vector<const clangtracer::v1::DebugTemplateType *> enums;  // Declared from DWARF.
  std::vector<Named> undeclarable;  // Cannot be declared at all.
  std::vector<Named> incomplete;    // Records DWARF describes only as opaque: declared, not defined.
};

bool declarable_argument_enum(const clangtracer::v1::DebugTemplateType &type) {
  if (type.scopes_size() != 0 || type.name().find('(') != std::string::npos ||
      type.name().find('<') != std::string::npos)
    return false;
  llvm::StringRef name = type.name();
  while (!name.empty()) {
    const auto part = name.split("::");
    if (!debug_identifier(part.first))
      return false;
    name = part.second;
  }
  return type.byte_size() == 1 || type.byte_size() == 2 || type.byte_size() == 4 ||
         type.byte_size() == 8;
}

void collect_argument_program_types(const clangtracer::v1::DebugInfo &info,
                                    ArgumentProgramTypes &types);

void collect_argument_program_types(const clangtracer::v1::DebugTemplateType &type,
                                    const std::string &owner, ArgumentProgramTypes &types) {
  using Kind = clangtracer::v1::DebugTemplateType;
  if (type.kind() == Kind::ENUM && !standard_library_record_name(type.name())) {
    if (declarable_argument_enum(type))
      types.enums.push_back(&type);
    else
      types.undeclarable.push_back(
          {owner, type.name(),
           type.scopes_size() != 0 ? "an enumeration declared inside a class or function"
                                   : "an enumeration without a declarable name"});
  } else if (type.kind() == Kind::RECORD) {
    if (standard_library_record_name(type.name())) {
      if (type.has_record())
        collect_argument_program_types(type.record(), types);
    } else if (type.name().find('<') != std::string::npos) {
      types.undeclarable.push_back({owner, type.name(), "a class template specialization"});
    } else if (type.name().find('(') != std::string::npos) {
      types.undeclarable.push_back({owner, type.name(), "a type in an anonymous namespace"});
    } else if (!type.has_record()) {
      types.undeclarable.push_back(
          {owner, type.name(), "a record whose debug description is not supported"});
    } else if (type.record().record_is_opaque()) {
      types.incomplete.push_back({owner, type.name(), {}});
    }
  }
  if (type.has_element() && type.kind() != Kind::ENUM)
    collect_argument_program_types(type.element(), owner, types);
  for (const auto &parameter : type.parameters())
    collect_argument_program_types(parameter, owner, types);
}

void collect_argument_program_types(const clangtracer::v1::DebugInfo &info,
                                    ArgumentProgramTypes &types) {
  if (!standard_library_record_name(info.record_name()))
    return;
  const auto visit = [&](const auto &self, const clangtracer::v1::DebugTemplateArgument &argument)
      -> void {
    if (argument.has_type())
      collect_argument_program_types(argument.type(), info.record_name(), types);
    for (const auto &element : argument.pack_elements())
      self(self, element);
  };
  for (const auto &argument : info.template_arguments())
    visit(visit, argument);
}

ArgumentProgramTypes argument_program_types(const clangtracer::v1::ProbeSpec &spec) {
  ArgumentProgramTypes types;
  const auto &info = spec.debug_info();
  collect_argument_program_types(info, types);
  for (const auto &argument : info.arguments())
    if (argument.has_object_type())
      collect_argument_program_types(argument.object_type(), types);
  if (info.has_result_object_type())
    collect_argument_program_types(info.result_object_type(), types);
  return types;
}

// The underlying type of an enumeration declared from DWARF. The base type's own name is used
// when it is plain C++ spelling (`long unsigned int`); otherwise the recorded size and
// signedness give an equivalent one, so the declaration never changes the enumeration's ABI.
std::string enum_underlying_spelling(const clangtracer::v1::DebugTemplateType &type) {
  if (type.has_element() && type.element().byte_size() == type.byte_size()) {
    const auto &name = type.element().name();
    if (!name.empty() &&
        std::all_of(name.begin(), name.end(), [](char c) {
          return (c >= 'a' && c <= 'z') || c == ' ' || (c >= '0' && c <= '9') || c == '_';
        }))
      return name;
  }
  // DW_ATE_signed (0x05) and DW_ATE_signed_char (0x06); anything else is unsigned.
  const bool is_signed = type.has_element() && (type.element().dwarf_encoding() == 0x05 ||
                                                type.element().dwarf_encoding() == 0x06);
  switch (type.byte_size()) {
  case 1: return is_signed ? "signed char" : "unsigned char";
  case 2: return is_signed ? "short" : "unsigned short";
  case 4: return is_signed ? "int" : "unsigned int";
  default: return is_signed ? "long" : "unsigned long";
  }
}

// The scope names of an enumeration, outermost first, and its own name last.
std::vector<std::string> enum_name_parts(const clangtracer::v1::DebugTemplateType &type) {
  std::vector<std::string> parts;
  for (llvm::StringRef name = type.name(); !name.empty();) {
    const auto part = name.split("::");
    if (!debug_identifier(part.first))
      throw std::runtime_error("unsupported recovered enumeration name " + type.name());
    parts.push_back(part.first.str());
    name = part.second;
  }
  if (parts.empty())
    throw std::runtime_error("unsupported recovered enumeration name");
  return parts;
}

// The opaque declaration of an enumeration: its name, whether it is scoped and its underlying
// type, without enumerators. That is all a callback needs to name the type.
std::string opaque_enum_declaration(const clangtracer::v1::DebugTemplateType &type,
                                    const std::string &own_name) {
  return std::string("enum ") + (type.enum_scoped() ? "class " : "") + own_name + " : " +
         enum_underlying_spelling(type) + "; ";
}

// Opaque declarations of the program's enumerations that standard-library records name as
// template arguments (std::vector<Status>). The enumeration's name, whether it is scoped and
// its underlying type are all the standard record's identity and layout depend on; its
// enumerators are not declared.
std::string argument_enum_declarations(const clangtracer::v1::ProbeSpec &spec) {
  const auto types = argument_program_types(spec);
  std::string result;
  std::unordered_set<std::string> seen;
  for (const auto *type : types.enums) {
    if (!seen.insert(type->name()).second)
      continue;
    const auto parts = enum_name_parts(*type);
    for (std::size_t i = 0; i + 1 < parts.size(); ++i)
      result += "namespace " + parts[i] + " { ";
    result += opaque_enum_declaration(*type, parts.back());
    for (std::size_t i = 0; i + 1 < parts.size(); ++i)
      result += "} ";
  }
  return result;
}

// The enumerations the callable's arguments and result name, once each. A standard-library
// enumeration is not rebuilt (`include_standard` lists it anyway): its real header supplies it,
// as for standard-library records.
std::vector<const clangtracer::v1::DebugTemplateType *> callable_enums(
    const clangtracer::v1::ProbeSpec &spec, bool include_standard = false) {
  std::vector<const clangtracer::v1::DebugTemplateType *> enums;
  std::unordered_set<std::string> seen;
  const auto add = [&](const clangtracer::v1::DebugTemplateType &type) {
    if (type.kind() == clangtracer::v1::DebugTemplateType::ENUM && !type.name().empty() &&
        (include_standard || !standard_library_record_name(type.name())) &&
        seen.insert(type.name()).second)
      enums.push_back(&type);
  };
  const auto &info = spec.debug_info();
  for (const auto &argument : info.arguments())
    if (argument.has_enum_type())
      add(argument.enum_type());
  if (info.has_result_enum_type())
    add(info.result_enum_type());
  return enums;
}

// The first program type that the automatic route cannot declare for a standard-library record,
// as a diagnostic that names it; empty when there is none.
std::string undeclarable_argument_type(const clangtracer::v1::ProbeSpec &spec) {
  const auto types = argument_program_types(spec);
  if (types.undeclarable.empty())
    return {};
  const auto &first = types.undeclarable.front();
  return "standard-library type " + first.owner + " has the template argument " + first.name +
         ", " + first.reason + ", which cannot be declared from the target's debug data; supply "
         "its real header with --include and --compdb, or with an #include in the callback";
}

struct RecoveredScopePart {
  std::string name;
  int kind;  // RecordScopeKind
};

// Split a recovered record name into its enclosing scopes (with their kinds) and its own name.
std::vector<RecoveredScopePart> recovered_record_scopes(
    const clangtracer::v1::DebugInfo &info, llvm::StringRef &name) {
  std::vector<RecoveredScopePart> scopes;
  name = info.record_name();
  while (::tracer::compat::contains(name, "::")) {
    auto part = name.split("::");
    if (!debug_identifier(part.first))
      throw std::runtime_error("unsupported recovered record namespace");
    const auto depth = static_cast<int>(scopes.size());
    scopes.push_back({part.first.str(), depth < info.record_scopes_size()
                                            ? static_cast<int>(info.record_scopes(depth))
                                            : static_cast<int>(clangtracer::v1::RECORD_SCOPE_NAMESPACE)});
    name = part.second;
  }
  if (!debug_identifier(name))
    throw std::runtime_error("unsupported recovered record name");
  return scopes;
}

// The qualified spelling of a recovered record named by another record's base or member,
// with every part validated like the record's own name.
std::string recovered_record_reference(const clangtracer::v1::DebugInfo &info) {
  llvm::StringRef name;
  recovered_record_scopes(info, name);
  return "::" + info.record_name();
}

std::string recovered_fields(const clangtracer::v1::DebugInfo &info, bool unnamed,
                             std::string &access);

// An unnamed struct or union that is the type of a member, with its members inline. They are
// public, which is what DWARF records for them.
std::string recovered_unnamed_record(const clangtracer::v1::DebugInfo &record) {
  std::string access = "public";
  return std::string(record.record_is_union() ? "union" : "struct") + " { " +
         recovered_fields(record, true, access) + "}";
}

// The data members of a recovered record in DWARF order. `access` is the access the text is
// currently in; `unnamed` is a record whose members need no access labels.
std::string recovered_fields(const clangtracer::v1::DebugInfo &info, bool unnamed,
                             std::string &access) {
  std::string result;
  // Where the previous member ended, in bits: an unnamed bit-field is not in DWARF, so a bit-field
  // that starts later than that is preceded by the padding the source had there.
  std::uint64_t end_bit = 0;
  for (const auto &base : info.bases())
    end_bit = std::max<std::uint64_t>(
        end_bit, (base.byte_offset() + base.record().record_size()) * 8);
  for (const auto &field : info.fields()) {
    if (!field.name().empty() && !debug_identifier(field.name()))
      throw std::runtime_error("unsupported recovered field name");
    if (field.name().empty() && !field.has_record())
      throw std::runtime_error("unsupported recovered field name");
    if (!unnamed && field.access() != access) {
      if (field.access() != "public" && field.access() != "protected" &&
          field.access() != "private")
        throw std::runtime_error("unsupported recovered field access");
      access = field.access();
      result += access + ": ";
    }
    std::string extents;
    for (const auto extent : field.array_extents())
      extents += "[" + std::to_string(extent) + "]";
    const bool anonymous = field.name().empty();
    if (field.alignment() && !anonymous)
      result += "alignas(" + std::to_string(field.alignment()) + ") ";
    if (field.bit_size()) {
      if (!info.record_is_union()) {
        while (field.bit_offset() > end_bit) {
          // Never across a byte boundary: a char-sized bit-field is never split by its type.
          const auto to_boundary = 8 - end_bit % 8;
          const auto piece = std::min<std::uint64_t>(field.bit_offset() - end_bit, to_boundary);
          result += "unsigned char : " + std::to_string(piece) + "; ";
          end_bit += piece;
        }
      }
      result += field.builtin_type() + " " + field.name() + " : " +
                std::to_string(field.bit_size()) + "; ";
      end_bit = field.bit_offset() + field.bit_size();
      continue;
    }
    if (field.has_record()) {
      const auto &record = field.record();
      if (!record.record_name().empty()) {
        result += recovered_record_reference(record) + " " + field.name() + extents + "; ";
      } else if (anonymous) {
        // An anonymous union is standard C++; an anonymous struct is an extension.
        result += (record.record_is_union() ? "" : "__extension__ ") +
                  recovered_unnamed_record(record) + "; ";
      } else {
        result += recovered_unnamed_record(record) + " " + field.name() + extents + "; ";
      }
    } else {
      result += field.builtin_type() + " " + field.name() + extents + "; ";
    }
    end_bit = (field.byte_offset() + field.byte_size()) * 8;
  }
  return result;
}

// The record's own declaration, without its enclosing scopes. `nested` is the text of records
// declared inside it.
std::string recovered_record_body(const clangtracer::v1::DebugInfo &info,
                                  llvm::StringRef name, const std::string &nested = {}) {
  std::string result;
  // A union keeps its tag everywhere it is declared, so every declaration of it agrees.
  const std::string keyword = info.record_is_union() ? "union" : "class";
  if (info.record_is_opaque()) {
    if (nested.empty()) {
      result += keyword + " " + name.str() + "; ";
    } else {
      // The record is only named by pointer, but it must be complete to hold the nested
      // declarations. Give the stand-in the size and alignment DWARF recorded so that the
      // layout check against the opaque type holds; nothing is claimed about its members.
      result += keyword + " " + name.str() + " { public: " + nested;
      if (info.record_size() != 0) {
        const auto alignment = info.record_alignment() > 1 ? info.record_alignment() : 1;
        result += "alignas(" + std::to_string(alignment) + ") unsigned char tracer_opaque_storage[" +
                  std::to_string(info.record_size()) + "]; ";
      }
      result += "}; ";
    }
  } else {
    result += keyword;
    if (info.record_alignment() > 1)
      result += " alignas(" + std::to_string(info.record_alignment()) + ")";
    result += " " + name.str();
    for (int i = 0; i < info.bases_size(); ++i) {
      const auto &base = info.bases(i);
      if (base.access() != "public" && base.access() != "protected" &&
          base.access() != "private")
        throw std::runtime_error("unsupported recovered base access");
      result += (i ? ", " : " : ") + base.access() + " " +
                recovered_record_reference(base.record());
    }
    result += " { ";
    if (!nested.empty())
      result += "public: " + nested;
    std::string access = nested.empty() ? std::string() : "public";
    result += recovered_fields(info, false, access);
    if (info.record_calling_convention() ==
        clangtracer::v1::RECORD_PASS_BY_REFERENCE) {
      result += "public: " + name.str() + "(const " + name.str() + "&); ";
      result += name.str() + "(" + name.str() + "&&); ~" + name.str() +
                "(); ";
    }
    if (!info.methods().empty()) {
      if (access != "public")
        result += "public: ";
      for (const auto &method : info.methods()) {
        if (!debug_identifier(method.name()) || method.is_static() ||
            method.is_virtual())
          continue;
        if (method.linkage_symbol().empty() ||
            !std::all_of(method.linkage_symbol().begin(),
                         method.linkage_symbol().end(), [](unsigned char c) {
                           return (c >= 'a' && c <= 'z') ||
                                  (c >= 'A' && c <= 'Z') ||
                                  (c >= '0' && c <= '9') || c == '_' ||
                                  c == '.' || c == '$';
                         }))
          continue;
        result += method.cpp_result_type() + " " + method.name() + "(";
        for (int i = 0; i < method.cpp_parameter_types_size(); ++i) {
          if (i)
            result += ", ";
          result += method.cpp_parameter_types(i);
        }
        result += ")";
        if (method.is_const())
          result += " const";
        if (method.is_volatile())
          result += " volatile";
        result += method.ref_qualifier();
        result += " __asm__(\"" + method.linkage_symbol() + "\"); ";
      }
    }
    result += "}; ";
  }
  return result;
}

// One name declared inside an enclosing class or union: a scope that only holds other
// declarations, a recovered record, or both.
struct RecoveredScopeNode {
  std::string name;
  int kind = clangtracer::v1::RECORD_SCOPE_CLASS;
  const clangtracer::v1::DebugInfo *record = nullptr;
  std::vector<RecoveredScopeNode> children;
  std::vector<std::string> enums;  // Opaque declarations of enumerations nested in the scope.
};

std::string emit_recovered_scope(const RecoveredScopeNode &node) {
  // Enumerations first: they are complete as soon as they are declared, and nothing else in
  // the scope depends on them being declared later. They land inside the enclosing class body.
  std::string inner;
  for (const auto &declaration : node.enums)
    inner += declaration;
  // Forward declarations next: a method may name a sibling declared later, or a cycle.
  for (const auto &child : node.children) {
    const char *keyword = child.record ? (child.record->record_is_union() ? "union" : "class")
                          : child.kind == clangtracer::v1::RECORD_SCOPE_UNION ? "union" : "struct";
    inner += std::string(keyword) + " " + child.name + "; ";
  }
  // A forward declaration cannot supply `Sibling::Inner`: a sibling whose nested name is used
  // by another must be defined first. Keep the original order, except for that dependency.
  std::vector<std::string> texts;
  for (const auto &child : node.children)
    texts.push_back(emit_recovered_scope(child));
  std::vector<bool> emitted(texts.size(), false);
  const auto blocked = [&](std::size_t index) {
    for (std::size_t other = 0; other < texts.size(); ++other)
      if (other != index && !emitted[other] &&
          texts[index].find("::" + node.children[other].name + "::") != std::string::npos)
        return true;
    return false;
  };
  for (std::size_t count = 0; count < texts.size(); ++count) {
    std::size_t pick = texts.size();
    for (std::size_t i = 0; i < texts.size() && pick == texts.size(); ++i)
      if (!emitted[i] && !blocked(i))
        pick = i;
    if (pick == texts.size())  // A dependency cycle: fall back to the original order.
      pick = static_cast<std::size_t>(std::find(emitted.begin(), emitted.end(), false) -
                                      emitted.begin());
    emitted[pick] = true;
    inner += texts[pick];
  }
  if (node.record)
    return recovered_record_body(*node.record, node.name, inner);
  // A struct keeps the nested records public, matching the member access DWARF recorded.
  const char *keyword = node.kind == clangtracer::v1::RECORD_SCOPE_UNION ? "union" : "struct";
  return std::string(keyword) + " " + node.name + " { " + inner + "}; ";
}

// Declarations for a set of recovered records. Records that share an enclosing class, or are
// one, are emitted as one declaration tree, because a class cannot be reopened.
std::string recovered_record_group(
    const std::vector<const clangtracer::v1::DebugInfo *> &records,
    const std::vector<const clangtracer::v1::DebugTemplateType *> &enums = {}) {
  struct Parsed {
    const clangtracer::v1::DebugInfo *info;
    std::vector<RecoveredScopePart> scopes;
    std::string name;
  };
  std::vector<Parsed> parsed;
  std::unordered_set<std::string> enclosing;  // Qualified names that enclose another record.
  for (const auto *info : records) {
    llvm::StringRef name;
    auto scopes = recovered_record_scopes(*info, name);
    std::string path;
    for (const auto &part : scopes) {
      path += (path.empty() ? "" : "::") + part.name;
      if (part.kind != clangtracer::v1::RECORD_SCOPE_NAMESPACE)
        enclosing.insert(path);
    }
    parsed.push_back({info, std::move(scopes), name.str()});
  }
  // The scopes of an enumeration, with kinds; an enumeration inside a class encloses nothing
  // itself, but its class must be one declaration with the enumeration inside it.
  const auto enum_scopes = [](const clangtracer::v1::DebugTemplateType &type) {
    auto parts = enum_name_parts(type);
    std::vector<RecoveredScopePart> scopes;
    for (std::size_t i = 0; i + 1 < parts.size(); ++i)
      scopes.push_back({parts[i], static_cast<int>(i) < type.scopes_size()
                                      ? static_cast<int>(type.scopes(static_cast<int>(i)))
                                      : static_cast<int>(clangtracer::v1::RECORD_SCOPE_NAMESPACE)});
    return std::make_pair(std::move(scopes), parts.back());
  };
  for (const auto *type : enums) {
    std::string path;
    for (const auto &part : enum_scopes(*type).first) {
      path += (path.empty() ? "" : "::") + part.name;
      if (part.kind != clangtracer::v1::RECORD_SCOPE_NAMESPACE)
        enclosing.insert(path);
    }
  }
  struct Item {
    std::string key;
    std::vector<std::string> namespaces;
    RecoveredScopeNode root;
    std::string plain;
  };
  std::vector<Item> items;
  for (const auto &entry : parsed) {
    const auto &scopes = entry.scopes;
    std::string full;
    for (const auto &part : scopes)
      full += part.name + "::";
    full += entry.name;
    // Components from the first enclosing record onward form the tree; the record itself is
    // one more node when it is nested or itself encloses another record.
    auto first_record = std::find_if(scopes.begin(), scopes.end(), [](const auto &part) {
      return part.kind != clangtracer::v1::RECORD_SCOPE_NAMESPACE;
    });
    const bool encloses = enclosing.count(full) != 0;
    if (first_record == scopes.end() && !encloses) {
      std::string text;
      for (const auto &part : scopes)
        text += "namespace " + part.name + " { ";
      text += recovered_record_body(*entry.info, entry.name);
      for (std::size_t i = 0; i < scopes.size(); ++i)
        text += "} ";
      items.push_back({{}, {}, {}, std::move(text)});
      continue;
    }
    std::vector<RecoveredScopePart> chain(first_record, scopes.end());
    chain.push_back({entry.name, clangtracer::v1::RECORD_SCOPE_CLASS});
    std::vector<std::string> namespaces;
    std::string key;
    for (auto it = scopes.begin(); it != first_record; ++it) {
      namespaces.push_back(it->name);
      key += it->name + "::";
    }
    key += chain.front().name;
    auto item = std::find_if(items.begin(), items.end(),
                             [&](const Item &candidate) { return candidate.key == key; });
    if (item == items.end()) {
      items.push_back({key, std::move(namespaces), {chain.front().name, chain.front().kind, nullptr, {}, {}}, {}});
      item = std::prev(items.end());
    }
    RecoveredScopeNode *node = &item->root;
    for (std::size_t i = 1; i < chain.size(); ++i) {
      auto child = std::find_if(node->children.begin(), node->children.end(),
                                [&](const auto &candidate) { return candidate.name == chain[i].name; });
      if (child == node->children.end()) {
        node->children.push_back({chain[i].name, chain[i].kind, nullptr, {}, {}});
        child = std::prev(node->children.end());
      }
      node = &*child;
    }
    node->record = entry.info;
  }
  for (const auto *type : enums) {
    const auto [scopes, own_name] = enum_scopes(*type);
    const auto declaration = opaque_enum_declaration(*type, own_name);
    const auto first_class = std::find_if(scopes.begin(), scopes.end(), [](const auto &part) {
      return part.kind != clangtracer::v1::RECORD_SCOPE_NAMESPACE;
    });
    if (first_class == scopes.end()) {
      std::string text;
      for (const auto &part : scopes)
        text += "namespace " + part.name + " { ";
      text += declaration;
      for (std::size_t i = 0; i < scopes.size(); ++i)
        text += "} ";
      items.push_back({{}, {}, {}, std::move(text)});
      continue;
    }
    // The same tree as a nested record: an enclosing class that is not itself recovered is a
    // stand-in struct that holds only the enumeration.
    std::vector<std::string> namespaces;
    std::string key;
    for (auto it = scopes.begin(); it != first_class; ++it) {
      namespaces.push_back(it->name);
      key += it->name + "::";
    }
    key += first_class->name;
    auto item = std::find_if(items.begin(), items.end(),
                             [&](const Item &candidate) { return candidate.key == key; });
    if (item == items.end()) {
      items.push_back({key, std::move(namespaces), {first_class->name, first_class->kind, nullptr, {}, {}}, {}});
      item = std::prev(items.end());
    }
    RecoveredScopeNode *node = &item->root;
    for (auto it = std::next(first_class); it != scopes.end(); ++it) {
      auto child = std::find_if(node->children.begin(), node->children.end(),
                                [&](const auto &candidate) { return candidate.name == it->name; });
      if (child == node->children.end()) {
        node->children.push_back({it->name, it->kind, nullptr, {}, {}});
        child = std::prev(node->children.end());
      }
      node = &*child;
    }
    if (std::find(node->enums.begin(), node->enums.end(), declaration) == node->enums.end())
      node->enums.push_back(declaration);
  }
  std::string result;
  for (const auto &item : items) {
    if (item.key.empty()) {
      result += item.plain;
      continue;
    }
    for (const auto &part : item.namespaces)
      result += "namespace " + part + " { ";
    result += emit_recovered_scope(item.root);
    for (std::size_t i = 0; i < item.namespaces.size(); ++i)
      result += "} ";
  }
  return result;
}

// Calls `visit` for each named record that `record` holds by value or derives from, looking
// through unnamed member types, which have no declaration of their own.
void for_each_recovered_dependency(
    const clangtracer::v1::DebugInfo &record,
    const std::function<void(const clangtracer::v1::DebugInfo &)> &visit) {
  if (record.record_is_opaque())
    return;
  const auto visit_record = [&](const clangtracer::v1::DebugInfo &dependency, auto &&self) -> void {
    if (!dependency.record_name().empty()) {
      visit(dependency);
      return;
    }
    for (const auto &base : dependency.bases())
      self(base.record(), self);
    for (const auto &field : dependency.fields())
      if (field.has_record())
        self(field.record(), self);
  };
  for (const auto &base : record.bases())
    visit_record(base.record(), visit_record);
  for (const auto &field : record.fields())
    if (field.has_record())
      visit_record(field.record(), visit_record);
}

// Adds `record` to `records` after every record it needs complete, each once and by name:
// a base or by-value member must be declared before the class that uses it. A record for which
// `declared_elsewhere` holds (a header already defines it) and one the standard library
// supplies are left out, and so are their dependencies.
void append_recovered_record(
    const clangtracer::v1::DebugInfo &record,
    std::vector<const clangtracer::v1::DebugInfo *> &records,
    std::unordered_set<std::string> &seen,
    const std::function<bool(const clangtracer::v1::DebugInfo &)> &declared_elsewhere) {
  if (record.record_name().empty() || standard_library_record_name(record.record_name()) ||
      seen.count(record.record_name()))
    return;
  if (declared_elsewhere && declared_elsewhere(record)) {
    seen.insert(record.record_name());
    return;
  }
  for_each_recovered_dependency(record, [&](const clangtracer::v1::DebugInfo &dependency) {
    append_recovered_record(dependency, records, seen, declared_elsewhere);
  });
  if (seen.insert(record.record_name()).second)
    records.push_back(&record);
}

std::string recovered_record_declarations(
    const clangtracer::v1::ProbeSpec &spec) {
  const auto &info = spec.debug_info();
  std::vector<const clangtracer::v1::DebugInfo *> records;
  std::unordered_set<std::string> seen;
  // A standard-library record is never rebuilt as a handwritten class: its real header
  // supplies it, selected automatically or named by the caller.
  auto append = [&](const clangtracer::v1::DebugInfo &record) {
    append_recovered_record(record, records, seen, {});
  };
  if (!info.record_name().empty())
    append(info);
  for (const auto &argument : info.arguments())
    if (argument.has_object_type())
      append(argument.object_type());
  if (info.has_result_object_type())
    append(info.result_object_type());
  // The program's own records named only as template arguments of a standard-library record.
  std::vector<const clangtracer::v1::DebugInfo *> nested;
  collect_argument_records(info, nested);
  for (const auto &argument : info.arguments())
    if (argument.has_object_type())
      collect_argument_records(argument.object_type(), nested);
  if (info.has_result_object_type())
    collect_argument_records(info.result_object_type(), nested);
  for (const auto *record : nested)
    append(*record);
  return argument_enum_declarations(spec) + recovered_record_group(records, callable_enums(spec));
}

// One alias per standard-library record the callable uses, each spelled from DWARF's
// structured arguments, so the compiler instantiates the real specialization even when nothing
// in the included headers names it (std::array<int, 4> or std::vector<Person>).
std::string standard_type_requests(const clangtracer::v1::ProbeSpec &spec) {
  const auto &info = spec.debug_info();
  std::string requests;
  std::unordered_set<std::string> seen;
  const auto request = [&](const clangtracer::v1::DebugInfo &record) {
    if (record.record_name().empty() || !standard_library_record_name(record.record_name()) ||
        !seen.insert(record.record_name()).second)
      return;
    requests += "using ClangTracerDwarfType" + std::to_string(seen.size()) + " = " +
                debug_record_spelling(record) + ";\nstatic_assert(sizeof(ClangTracerDwarfType" +
                std::to_string(seen.size()) + ") != 0, \"\");\n";
  };
  request(info);
  for (const auto &argument : info.arguments())
    if (argument.has_object_type())
      request(argument.object_type());
  if (info.has_result_object_type())
    request(info.result_object_type());
  return requests;
}

bool ast_has_debug_record(clang::ASTContext &context,
                          const clangtracer::v1::DebugInfo &info) {
  class Finder : public clang::RecursiveASTVisitor<Finder> {
  public:
    Finder(clang::ASTContext &context, const clangtracer::v1::DebugInfo &expected)
        : context(context), expected(expected) {}
    bool VisitCXXRecordDecl(clang::CXXRecordDecl *record) {
      if (debug_record_identity_matches(context, record, expected))
        found = true;
      return !found;
    }
    clang::ASTContext &context;
    const clangtracer::v1::DebugInfo &expected;
    bool found = false;
  } finder(context, info);
  finder.TraverseDecl(context.getTranslationUnitDecl());
  return finder.found;
}

// The qualified names of the enumerations and the records an AST declares. A standard-library
// name is kept without its inline ABI namespaces (libc++ spells std::__1::errc).
struct DeclaredNames {
  std::unordered_set<std::string> enums, records;
};

std::string declared_name(const std::string &name) {
  return ::tracer::standard_headers::standard_library_scope(name)
             ? ::tracer::standard_headers::primary_name(name)
             : name;
}

DeclaredNames declared_names(clang::ASTContext &context) {
  class Finder : public clang::RecursiveASTVisitor<Finder> {
  public:
    bool VisitEnumDecl(clang::EnumDecl *declaration) {
      names.enums.insert(declared_name(declaration->getQualifiedNameAsString()));
      return true;
    }
    bool VisitCXXRecordDecl(clang::CXXRecordDecl *declaration) {
      names.records.insert(declared_name(declaration->getQualifiedNameAsString()));
      return true;
    }
    DeclaredNames names;
  } finder;
  finder.TraverseDecl(context.getTranslationUnitDecl());
  return std::move(finder.names);
}

// The enumerations of the callable that the headers in the AST do not declare. An enumeration
// nested in a class cannot be added to a class that the headers already define, so that case is
// reported instead of declaring a second class with the same name.
std::vector<const clangtracer::v1::DebugTemplateType *> missing_callable_enums(
    clang::ASTContext &context, const clangtracer::v1::ProbeSpec &spec) {
  const auto enums = callable_enums(spec);
  if (enums.empty())
    return {};
  const auto declared = declared_names(context);
  std::vector<const clangtracer::v1::DebugTemplateType *> missing;
  for (const auto *type : enums) {
    if (declared.enums.count(type->name()))
      continue;
    // The outermost enclosing class, qualified; nothing nested can exist without it.
    std::string scope;
    const auto parts = enum_name_parts(*type);
    for (int i = 0; i < type->scopes_size() && static_cast<std::size_t>(i) + 1 < parts.size();
         ++i) {
      scope += (scope.empty() ? "" : "::") + parts[static_cast<std::size_t>(i)];
      if (type->scopes(i) != clangtracer::v1::RECORD_SCOPE_NAMESPACE) {
        if (declared.records.count(scope))
          throw std::runtime_error("enumeration " + type->name() + " is not declared by the "
                                   "headers, but its enclosing class " + scope + " is; "
                                   "declare the enumeration in that class");
        break;
      }
    }
    missing.push_back(type);
  }
  return missing;
}

std::string missing_recovered_record_declarations(
    clang::ASTContext &context,
    const clangtracer::v1::ProbeSpec &spec, bool check_enclosing = false) {
  std::vector<const clangtracer::v1::DebugInfo *> records;
  std::unordered_set<std::string> seen;
  const auto declared = [&](const clangtracer::v1::DebugInfo &record) {
    return ast_has_debug_record(context, record);
  };
  const auto append = [&](const clangtracer::v1::DebugInfo &record) {
    append_recovered_record(record, records, seen, declared);
  };
  const auto &info = spec.debug_info();
  if (!info.record_name().empty())
    append(info);
  for (const auto &argument : info.arguments())
    if (argument.has_object_type())
      append(argument.object_type());
  if (info.has_result_object_type())
    append(info.result_object_type());
  // The program's own records named only as template arguments of a standard-library record.
  std::vector<const clangtracer::v1::DebugInfo *> nested;
  collect_argument_records(info, nested);
  for (const auto &argument : info.arguments())
    if (argument.has_object_type())
      collect_argument_records(argument.object_type(), nested);
  if (info.has_result_object_type())
    collect_argument_records(info.result_object_type(), nested);
  for (const auto *record : nested)
    append(*record);
  if (check_enclosing)
    for (const auto *record : records) {
      llvm::StringRef name;
      const clang::CXXRecordDecl *real_enclosing = nullptr;
      recovered_debug_scope(context, *record, name, "unsupported debug record scope",
                            &real_enclosing, false);
      reject_real_enclosing(real_enclosing, *record);
    }
  return recovered_record_group(records, missing_callable_enums(context, spec));
}

std::uint64_t validate_debug_entry(const clangtracer::v1::ProbeSpec &spec) {
  if (const auto *error = tracer::scalar_override_error(spec))
    throw std::invalid_argument(error);
  const auto &info = spec.debug_info();
  tracer::client::DebugLookupOptions lookup;
  lookup.module = info.module();
  lookup.require_override_abi = tracer::has_scalar_overrides(spec);
  const auto live_patch = [](std::uint64_t address) -> tracer::client::PreparedCodePatch {
    const auto *current = agent.load(std::memory_order_acquire);
    if (current) {
      for (const auto &probe : current->probes) {
        if (!probe || !probe->active.load() || probe->request.patch_address != address ||
            probe->request.original_entry != address)
          continue;
        const auto &patch = probe->request;
        std::array<unsigned char, tracer::kMaxPatchBytes> live{};
        iovec local{live.data(), patch.patch_size};
        iovec remote{reinterpret_cast<void *>(address), patch.patch_size};
        if (tracer::linux_api::process_vm_readv(::getpid(), &local, 1, &remote, 1, 0) ==
                static_cast<ssize_t>(patch.patch_size) &&
            std::memcmp(live.data(), patch.installed, patch.patch_size) == 0)
          return {{reinterpret_cast<const char *>(patch.original), patch.patch_size},
                  {reinterpret_cast<const char *>(patch.installed), patch.patch_size}};
      }
    }
    return {};
  };
  const auto checked = spec.has_prepared()
      ? tracer::client::bind_prepared_callable(::getpid(), spec.prepared(), lookup, live_patch)
      : tracer::client::resolve_debug_callable(
            ::getpid(),
            // Bind by linkage name when the entry has one: an alias shares the primary's
            // selector but is installed under its own symbol.
            spec.symbol().compare(0, 2, "_Z") == 0 ? spec.symbol() : info.selector(), lookup);
  if (!info.runtime_entry() || !info.entry_size() ||
      checked.spec.debug_info().SerializeAsString() !=
          info.SerializeAsString() ||
      checked.spec.symbol() != spec.symbol() ||
      checked.spec.signature().SerializeAsString() !=
          spec.signature().SerializeAsString() ||
      checked.spec.argument_names().size() != spec.argument_names().size() ||
      !std::equal(spec.argument_names().begin(), spec.argument_names().end(),
                  checked.spec.argument_names().begin())) {
    throw std::runtime_error("mapped debug image, entry or callable metadata "
                             "changed before injection");
  }
  return checked.spec.debug_info().runtime_entry();
}

// Says which standard headers were chosen automatically, from which toolchain, and for which
// target when a recovered standard-library type cannot be used. The three situations the
// caller can act on differ: a header the agent's toolchain does not have (a missing
// definition), a header whose type is not the target's (an incompatible definition), and a
// libstdc++ string ABI that disagrees with the target's.
constexpr std::string_view kAutomaticCallbackFailure =
    "cannot compile the callback with the standard headers chosen automatically";

std::string automatic_headers_failure(const std::string &error,
                                      const clangtracer::v1::ProbeSpec &spec,
                                      const std::string &includes) {
  const bool missing_header = error.find("file not found") != std::string::npos;
  const bool incompatible = ::tracer::compat::starts_with(error, "standard-library ABI mismatch") ||
                            ::tracer::compat::starts_with(error, "libstdc++ ABI mismatch") ||
                            error.find("size/alignment does not match") != std::string::npos;
  std::string selected;
  std::string_view lines(includes);
  while (!lines.empty()) {
    const auto newline = lines.find('\n');
    const auto line = lines.substr(0, newline);
    if (const auto tag = line.find(kAutomaticHeaderTag);
        tag != std::string_view::npos && ::tracer::compat::starts_with(line, "#include "))
      selected += (selected.empty() ? "" : " ") + std::string(line.substr(9, tag - 9));
    lines.remove_prefix(newline == std::string_view::npos ? lines.size() : newline + 1);
  }
  if (!missing_header && !incompatible) {
    // The requested type did not compile. Two causes can be named from the debug data instead
    // of leaving the caller with compiler output about generated code: a program type that is
    // needed complete but is only described as opaque, and a standard-library argument record
    // that has no mapping and that the chosen headers turned out not to define.
    if (!::tracer::compat::starts_with(error, "cannot create debug type AST") &&
        !::tracer::compat::starts_with(error, kAutomaticCallbackFailure))
      return error;
    const auto types = argument_program_types(spec);
    if (!types.incomplete.empty() && error.find("incomplete type") != std::string::npos) {
      const auto &first = types.incomplete.front();
      return "standard-library type " + first.owner + " needs the complete definition of its "
             "template argument " + first.name + ", which the target's debug data describes "
             "only as an opaque type; supply its real header with --include and --compdb, or "
             "with an #include in the callback (compiler output: " + error + ")";
    }
    if (const auto unmapped = unmapped_argument_records(spec); !unmapped.empty())
      return "the headers chosen automatically (" + selected + ") do not define every "
             "standard-library record the type names as a template argument; without a header "
             "mapping: " + unmapped + "; map it, or supply the real header with --include or "
             "an #include in the callback (compiler output: " + error + ")";
    return error;
  }
  tracer::AgentRuntimeProfile profile;
  const bool have_profile =
      tracer_agent_runtime_profile_v1(&profile, sizeof(profile)) == 0;
  std::string result = missing_header
      ? "standard header selected for a recovered standard-library type is not available in the "
        "agent's toolchain: "
      : "the agent's standard headers define a different type than the target's: ";
  result += error + "; headers chosen automatically: " + selected;
  if (have_profile) {
    result += "; agent toolchain: libstdc++ release " +
              std::to_string(profile.libstdcxx_header_release) +
              ", _GLIBCXX_USE_CXX11_ABI=" + std::to_string(profile.libstdcxx_default_cxx11_abi) +
              ", runtime " + profile.cxx_runtime_path;
  }
  if (!spec.debug_info().producer().empty())
    result += "; target built by '" + spec.debug_info().producer() + "'";
  result += "; to use the target's own headers pass --include with --compdb, or change the "
            "mapping with --std-header RECORD=HEADER";
  return result;
}

bool build_debug_receiver_once(clangtracer::v1::ProbeSpec &spec,
                               std::unique_ptr<clang::ASTUnit> &output,
                               clang::CXXRecordDecl *&record, std::string &error,
                               DebugRecords &records,
                               const std::string &callback_includes,
                               const VerifiedDebugEntry *verified_entry) {
  try {
    const auto &info = spec.debug_info();
    // tracer_agent_start performed the full serialized metadata comparison
    // and entry lookup. Reuse only that operation's result; never cache across
    // requests or sessions. The spec address guards against passing a token
    // verified for a different request.
    if (!verified_entry || verified_entry->spec != &spec ||
        verified_entry->runtime_entry != info.runtime_entry())
      throw std::runtime_error(
          "debug entry verification does not match probe");
    bool has_record = !info.record_name().empty();
    for (const auto &argument : info.arguments())
      has_record |= argument.has_object_type();
    has_record |= info.has_result_object_type();
    if (!has_record)
      return true;
    const bool user_headers = spec.has_header_context() || spec.include_headers_size() != 0 ||
                              has_user_includes(callback_includes);
    const bool headers_available = user_headers || has_automatic_headers(callback_includes);
    const auto standard_library_record = [](const auto &record) {
      return ::tracer::compat::starts_with(record.record_name(), "std::") ||
             ::tracer::compat::starts_with(record.record_name(),
                                           "__gnu_cxx::") ||
             ::tracer::compat::starts_with(record.record_name(),
                                           "__cxxabiv1::");
    };
    if (!headers_available && standard_library_record(info)) {
      error = "standard-library record types require their real headers";
      return false;
    }
    if (!headers_available)
      for (const auto &argument : info.arguments())
        if (argument.has_object_type() &&
            standard_library_record(argument.object_type())) {
          error = "standard-library record types require their real headers";
          return false;
        }
    if (!headers_available && info.has_result_object_type() &&
        standard_library_record(info.result_object_type())) {
      error = "standard-library record types require their real headers";
      return false;
    }
    std::string headers;
    for (const auto &header : spec.include_headers()) {
      if (header.empty() || header.size() > 4095 || header.front() != '/' ||
          header.find_first_of("\"\r\n") != std::string::npos ||
          header.find('\0') != std::string::npos)
        throw std::runtime_error("invalid optional include path");
      headers += "#include \"" + header + "\"\n";
    }
    if (!user_headers)
      headers += recovered_record_declarations(spec);
    // The records come from real headers; ask the compiler for exactly the specializations
    // DWARF names so they exist to be matched against it.
    if (!user_headers && has_automatic_headers(callback_includes))
      headers += standard_type_requests(spec);
    if (!callback_includes.empty())
      headers = callback_includes + headers;
    const auto parse_types = [&] {
      auto capture = std::make_unique<DiagnosticCapture>(nullptr);
      if (spec.has_header_context()) {
        const auto &header_context = spec.header_context();
        const auto &directory = header_context.working_directory();
        if (spec.include_headers().empty() || directory.empty() ||
            directory.front() != '/' || directory.size() > 4095 ||
            directory.find('\0') != std::string::npos ||
            header_context.ByteSizeLong() > 64 * 1024 ||
            header_context.compiler_arguments_size() > 512)
          throw std::runtime_error("invalid header compilation context");
        auto arguments = tracer::runtime::jit_arguments();
        arguments.insert(arguments.end(),
                         header_context.compiler_arguments().begin(),
                         header_context.compiler_arguments().end());
        arguments.insert(arguments.end(), spec.extra_compile_arguments().begin(),
                         spec.extra_compile_arguments().end());
        bool resource_dir = false;
        for (const auto &argument : arguments) {
          if (argument.find('\0') != std::string::npos)
            throw std::runtime_error("invalid header compiler argument");
          resource_dir |= argument == "-resource-dir" ||
                          ::tracer::compat::starts_with(argument, "-resource-dir=");
        }
        if (!resource_dir)
          arguments.push_back(std::string("-resource-dir=") +
                              tracer::runtime::clang_resource_dir());
        arguments.insert(arguments.end(), {"-x", "c++"});
        auto physical_fs = llvm::vfs::createPhysicalFileSystem();
        llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> private_fs(
            physical_fs.release());
        if (auto ec = private_fs->setCurrentWorkingDirectory(directory))
          throw std::runtime_error("cannot use header working directory: " +
                                   ec.message());
        // Map a synthetic TU, never the compilation database's donor .cpp.
        // The private filesystem keeps the target's process-wide cwd unchanged.
        const auto input =
            (std::filesystem::path(directory) / "clang-tracer-debug-types.cc")
                .string();
        output = clang::tooling::buildASTFromCodeWithArgs(
            headers, arguments, input, "clang-tracer",
            std::make_shared<clang::PCHContainerOperations>(),
            clang::tooling::getClangStripDependencyFileAdjuster(), {},
            capture.get(), std::move(private_fs));
      } else {
        auto arguments = tracer::runtime::jit_arguments();
        arguments.insert(arguments.end(),
                         {"-std=c++20", "-x", "c++",
                          std::string("-resource-dir=") +
                              tracer::runtime::clang_resource_dir()});
        arguments.insert(arguments.end(), spec.compile_options().begin(),
                         spec.compile_options().end());
        arguments.insert(arguments.end(), spec.extra_compile_arguments().begin(),
                         spec.extra_compile_arguments().end());
        output = clang::tooling::buildASTFromCodeWithArgs(
            headers, arguments, "clang-tracer-debug-types.cc", "clang-tracer",
            std::make_shared<clang::PCHContainerOperations>(),
            clang::tooling::getClangStripOutputAdjuster(), {}, capture.get());
      }
      auto *diagnostics = capture.get();
      // AST import can report diagnostics after this function returns. Clang's
      // tooling APIs borrow the consumer, so give the retained AST ownership.
      if (output)
        output->getDiagnostics().setClient(capture.release(), true);
      if (!output || diagnostics->had_error())
        throw std::runtime_error("cannot create debug type AST: " +
                                 diagnostics->message());
    };
    parse_types();
    if (headers_available) {
      // Parse real headers first, then recover all missing records together so
      // enclosing layouts and dependencies between nested types remain intact.
      const auto missing =
          missing_recovered_record_declarations(output->getASTContext(), spec, true);
      if (!missing.empty()) {
        headers += missing;
        parse_types();
      }
    }
    auto &context = output->getASTContext();
    const auto &triple = context.getTargetInfo().getTriple();
    if (triple.getArch() != llvm::Triple::x86_64 || !triple.isOSLinux() ||
        context.getTypeSize(context.VoidPtrTy) != 64)
      throw std::runtime_error("header target does not match Linux x86_64");
    // The string ABI the headers selected must be the one the target's dual-ABI classes show.
    if (const int target_abi = target_cxx11_abi(spec); target_abi >= 0) {
      const bool target_uses_cxx11_string_abi = target_abi == 1;
      auto &preprocessor = output->getPreprocessor();
      auto &identifier = preprocessor.getIdentifierTable().get(
          "_GLIBCXX_USE_CXX11_ABI");
      if (const auto *macro = preprocessor.getMacroInfo(&identifier);
          macro && macro->getNumTokens() == 1) {
        const auto value = clang::Lexer::getSpelling(
            macro->tokens().front(), output->getSourceManager(),
            context.getLangOpts());
        if ((value == "0" && target_uses_cxx11_string_abi) ||
            (value == "1" && !target_uses_cxx11_string_abi))
          throw std::runtime_error(
              std::string("libstdc++ ABI mismatch: the target uses the ") +
              (target_uses_cxx11_string_abi ? "C++11 (std::__cxx11)" : "pre-C++11") +
              " std::string ABI according to its DWARF, but the headers and options select "
              "_GLIBCXX_USE_CXX11_ABI=" + value + "; pass --compile-option "
              "-D_GLIBCXX_USE_CXX11_ABI=" + (target_uses_cxx11_string_abi ? "1" : "0") +
              ", or remove the option that sets it");
      }
    }
    if (!info.record_name().empty())
      records.push_back(construct_debug_record(*output, info));
    for (const auto &argument : info.arguments()) {
      if (!argument.has_object_type())
        continue;
      const auto &object = argument.object_type();
      bool duplicate = false;
      for (const auto &previous : records)
        if (previous.layout.record_name() == object.record_name())
          duplicate = true;
      auto checked_record = construct_debug_record(*output, object);
      if (!duplicate)
        records.push_back(std::move(checked_record));
    }
    if (info.has_result_object_type()) {
      const auto &object = info.result_object_type();
      bool duplicate = false;
      for (const auto &previous : records)
        if (previous.layout.record_name() == object.record_name())
          duplicate = true;
      auto checked_record = construct_debug_record(*output, object);
      if (!duplicate)
        records.push_back(std::move(checked_record));
    }
    record = nullptr;
    if (!info.record_name().empty()) {
      const auto found = std::find_if(
          records.begin(), records.end(), [&](const DebugRecord &entry) {
            return entry.layout.record_name() == info.record_name();
          });
      if (found == records.end())
        throw std::runtime_error("selected receiver type was not recovered");
      record = found->decl;
      *spec.mutable_receiver() = found->layout;
    }
    return true;
  } catch (const std::exception &e) {
    error = e.what();
    if (has_automatic_headers(callback_includes))
      error = automatic_headers_failure(error, spec, callback_includes);
    return false;
  }
}

// Gives up the described layout of every record the agent rebuilt from debug data (those that
// the header route accepts by size and alignment only are not touched unless their layout is
// described beyond flat members), keeping the class as an opaque one with the reason. Returns
// false when a demoted record is passed or returned by value: that needs the verified layout.
bool demote_recovered_records(clangtracer::v1::DebugInfo &info, bool recovered_from_debug_data,
                              const std::string &cause) {
  bool by_value = false;
  const auto demote = [&](clangtracer::v1::DebugInfo &record, bool value) {
    if (record.record_is_opaque() || record.record_name().empty() ||
        standard_library_record_name(record.record_name()) ||
        (!recovered_from_debug_data && !tracer::client::uses_recovered_layout(record)))
      return;
    record.clear_fields();
    record.clear_bases();
    record.set_record_is_opaque(true);
    // The alignment was derived together with the layout that did not hold.
    record.set_record_alignment(0);
    record.set_record_opaque_reason(record.record_name() +
                                    ": its layout could not be rebuilt from debug data: " + cause);
    by_value |= value;
  };
  demote(info, false);
  for (auto &argument : *info.mutable_arguments())
    if (argument.has_object_type())
      demote(*argument.mutable_object_type(),
             argument.is_object_value() && !argument.is_reference());
  if (info.has_result_object_type())
    demote(*info.mutable_result_object_type(),
           info.result_is_object_value() && !info.result_is_reference());
  return !by_value;
}

// Builds the receiver's declarations and verifies them. A layout that DWARF describes but Clang
// does not reproduce (packed members, reused tail padding, ...) must not turn a receiver that can
// be observed into one that cannot: those records are demoted to opaque ones and the build is
// repeated once, so the probe still installs and only a callback that reads members is refused.
bool build_debug_receiver(clangtracer::v1::ProbeSpec &spec,
                          std::unique_ptr<clang::ASTUnit> &output,
                          clang::CXXRecordDecl *&record, std::string &error,
                          DebugRecords &records,
                          const std::string &callback_includes,
                          const VerifiedDebugEntry *verified_entry) {
  if (build_debug_receiver_once(spec, output, record, error, records, callback_includes,
                                verified_entry))
    return true;
  const bool recovered_from_debug_data =
      !(spec.has_header_context() || spec.include_headers_size() != 0 ||
        has_user_includes(callback_includes));
  // Work on a copy: a demotion that does not allow a retry leaves the specification as it was.
  auto demoted = spec.debug_info();
  const std::string original = error;
  if (!demote_recovered_records(demoted, recovered_from_debug_data, original) ||
      demoted.SerializeAsString() == spec.debug_info().SerializeAsString())
    return false;
  const auto previous = spec.debug_info();
  *spec.mutable_debug_info() = std::move(demoted);
  records.clear();
  output.reset();
  record = nullptr;
  std::string retry_error;
  if (build_debug_receiver_once(spec, output, record, retry_error, records, callback_includes,
                                verified_entry))
    return true;
  // The original failure is the cause worth reporting.
  *spec.mutable_debug_info() = previous;
  records.clear();
  output.reset();
  record = nullptr;
  error = original;
  return false;
}

bool build_and_validate_receiver_ast(clangtracer::v1::ProbeSpec &spec,
                                     const tracer::Request &request,
                                     std::unique_ptr<clang::ASTUnit> &output,
                                     clang::CXXRecordDecl *&output_record,
                                     std::string &error,
                                     DebugRecords &records,
                                     const std::string &callback_includes,
                                     const VerifiedDebugEntry *verified_entry) {
  if (spec.has_debug_info())
    return build_debug_receiver(spec, output, output_record, error, records,
                                callback_includes, verified_entry);
  if (!spec.has_receiver()) {
    return true;
  }
  if (request.kind != tracer::CallableKind::Method) {
    error = "receiver metadata is valid only for a non-static method";
    return false;
  }
  const auto &receiver = spec.receiver();
  if (receiver.clang_version() != CLANG_VERSION_STRING) {
    error = "receiver Clang version does not match the tracing agent";
    return false;
  }
  auto arguments = tracer::runtime::jit_arguments();
  arguments.reserve(arguments.size() + static_cast<std::size_t>(
                                           receiver.compiler_arguments_size()));
  const auto source_path =
      std::filesystem::path(receiver.translation_unit()).is_absolute()
          ? std::filesystem::path(receiver.translation_unit())
          : std::filesystem::path(receiver.working_directory()) /
                receiver.translation_unit();
  const auto normalized_source = source_path.lexically_normal();
  bool first = true;
  for (const auto &argument : receiver.compiler_arguments()) {
    if (first) {
      first = false; // The frozen invocation includes argv[0].
      continue;
    }
    const std::filesystem::path candidate(argument);
    const auto normalized_candidate =
        (candidate.is_absolute()
             ? candidate
             : std::filesystem::path(receiver.working_directory()) / candidate)
            .lexically_normal();
    if (!argument.empty() && argument.front() != '-' &&
        normalized_candidate == normalized_source) {
      continue;
    }
    arguments.push_back(argument);
  }
  arguments.insert(arguments.end(), spec.extra_compile_arguments().begin(),
                   spec.extra_compile_arguments().end());
  if (arguments.empty()) {
    error = "receiver compilation context has no compiler options";
    return false;
  }
  clang::tooling::FixedCompilationDatabase database(
      receiver.working_directory(), arguments);
  auto physical_fs = llvm::vfs::createPhysicalFileSystem();
  llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> private_fs(
      physical_fs.release());
  clang::tooling::ClangTool tool(
      database, {receiver.translation_unit()},
      std::make_shared<clang::PCHContainerOperations>(), private_fs);
  auto diagnostics = std::make_unique<DiagnosticCapture>(nullptr);
  tool.setDiagnosticConsumer(diagnostics.get());
  tool.appendArgumentsAdjuster(clang::tooling::getClangStripOutputAdjuster());
  std::vector<std::unique_ptr<clang::ASTUnit>> units;
  if (tool.buildASTs(units) != 0 || diagnostics->had_error() ||
      units.size() != 1) {
    error = diagnostics->message();
    if (error.empty()) {
      error = "cannot parse receiver translation unit with its frozen compiler "
              "context";
    }
    return false;
  }
  units.front()->getDiagnostics().setClient(diagnostics.release(), true);
  auto &context = units.front()->getASTContext();
  const auto triple = context.getTargetInfo().getTriple();
  if (triple.getArch() != llvm::Triple::x86_64 || !triple.isOSLinux() ||
      context.getTypeSize(context.VoidPtrTy) != 64 ||
      triple.str() != receiver.target_triple()) {
    error =
        "receiver target triple does not match the Linux x86_64 tracing agent";
    return false;
  }

  class TargetFinder final : public clang::RecursiveASTVisitor<TargetFinder> {
  public:
    TargetFinder(clang::ASTContext &context, llvm::StringRef symbol,
                 llvm::StringRef record_name)
        : context_(context), symbol_(symbol), record_name_(record_name) {}

    bool VisitFunctionDecl(clang::FunctionDecl *function) {
      auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(function);
      if (!method || llvm::isa<clang::CXXConstructorDecl>(method) ||
          llvm::isa<clang::CXXDestructorDecl>(method) ||
          method->getParent()->getQualifiedNameAsString() != record_name_ ||
          function->getType()->isDependentType()) {
        return true;
      }
      if (mangle_function(function, context_) == symbol_) {
        target = method;
        record = method->getParent()->getDefinition();
      }
      return true;
    }

    clang::CXXRecordDecl *record = nullptr;
    clang::CXXMethodDecl *target = nullptr;

  private:
    clang::ASTContext &context_;
    std::string symbol_;
    std::string record_name_;
  };

  TargetFinder finder(context, request.symbol, receiver.record_name());
  finder.TraverseDecl(context.getTranslationUnitDecl());
  auto *record = finder.record;
  auto *target = finder.target;
  if (!record || !target || target->isStatic() ||
      target->getParent()->getCanonicalDecl() != record->getCanonicalDecl()) {
    error = "frozen source does not contain the selected non-static method and "
            "receiver";
    return false;
  }
  if (!validate_receiver_layout(context, record, receiver, error)) {
    return false;
  }
  auto *method = target;
  if (method->isVolatile() != receiver.is_volatile() ||
      !matches_receiver_value_type(method->getReturnType(), context,
                                   request.result, true) ||
      method->getNumParams() != request.argument_count) {
    error = "selected method signature changed since source selection";
    return false;
  }
  for (unsigned i = 0; i < request.argument_count; ++i) {
    if (!matches_receiver_value_type(method->getParamDecl(i)->getType(),
                                     context, request.arguments[i], false)) {
      error = "selected method arguments do not match the prepared request";
      return false;
    }
  }
  output_record = record;
  output = std::move(units.front());
  return true;
}

bool validate_object_abi_evidence(
    const clangtracer::v1::ProbeSpec &spec,
    const std::string &callback_includes,
    const std::string &forced_include_source, std::string &error) {
  const bool header_available = spec.has_header_context() ||
                                spec.include_headers_size() != 0 ||
                                has_user_includes(callback_includes) ||
                                !forced_include_source.empty();
  // Automatically selected standard headers give Clang the real type of a standard-library
  // record, so its ABI comes from the compiler. A record the headers do not cover still needs
  // DWARF evidence or a real definition.
  const bool standard_headers = has_automatic_headers(callback_includes);
  if (!spec.has_debug_info())
    return true;
  const auto validate = [&](const clangtracer::v1::DebugInfo &record,
                            const char *role,
                            bool allow_nontrivial_result) {
    const auto convention = record.record_calling_convention();
    if (header_available ||
        (standard_headers && standard_library_record_name(record.record_name())))
      return true;
    if (convention ==
            clangtracer::v1::RECORD_CALLING_CONVENTION_UNSPECIFIED ||
        record.record_is_opaque() || record.record_size() == 0 ||
        record.record_alignment() == 0) {
      error = std::string(role) +
              " record ABI is not fully described by DWARF; include its "
              "definition to validate the compiler ABI";
      return false;
    }
    if (convention == clangtracer::v1::RECORD_PASS_BY_REFERENCE) {
      if (allow_nontrivial_result)
        return true;
      error = "nontrivial by-value argument requires its real type definition "
              "to preserve copy semantics";
      return false;
    }
    return convention == clangtracer::v1::RECORD_PASS_BY_VALUE;
  };
  for (const auto &argument : spec.debug_info().arguments()) {
    if (argument.is_object_value() && argument.has_object_type() &&
        !validate(argument.object_type(), "by-value argument", false))
      return false;
  }
  if (spec.debug_info().result_is_object_value() &&
      spec.debug_info().has_result_object_type() &&
      !validate(spec.debug_info().result_object_type(), "by-value result",
                true))
    return false;
  return true;
}

bool determine_object_result_sret(const clangtracer::v1::ProbeSpec &spec,
                                  const DebugRecords &records,
                                  bool &result_sret, std::string &error) {
  result_sret = false;
  if (!spec.has_debug_info() ||
      !spec.debug_info().result_is_object_value())
    return true;
  if (!spec.debug_info().has_result_object_type()) {
    error = "by-value result has no recovered record ABI";
    return false;
  }
  const auto &name = spec.debug_info().result_object_type().record_name();
  const auto found = std::find_if(
      records.begin(), records.end(), [&](const DebugRecord &record) {
        return record.layout.record_name() == name;
      });
  if (found == records.end() || !found->decl ||
      !found->decl->isCompleteDefinition()) {
    error = "by-value result ABI cannot be determined without a complete "
            "record definition";
    return false;
  }
  // On the supported Linux x86-64 SysV target, Clang's record argument
  // classification matches the indirect-result decision for C++ records.
  result_sret = spec.debug_info().result_object_type().record_size() > 16 ||
                !found->decl->canPassInRegisters();
  return true;
}

bool valid_type(tracer::ValueType type, bool result) {
  switch (type) {
  case tracer::ValueType::Void:
    return result;
  case tracer::ValueType::Int32:
  case tracer::ValueType::Bool:
  case tracer::ValueType::Int64:
  case tracer::ValueType::UInt64:
  case tracer::ValueType::Float32:
  case tracer::ValueType::Float64:
  case tracer::ValueType::Pointer:
    return true;
  case tracer::ValueType::Object:
    return true;
  default:
    return false;
  }
}

const char *type_name(tracer::ValueType type) {
  switch (type) {
  case tracer::ValueType::Void:
    return "void";
  case tracer::ValueType::Int32:
    return "int32_t";
  case tracer::ValueType::Bool:
    return "bool";
  case tracer::ValueType::Int64:
    return "int64_t";
  case tracer::ValueType::UInt64:
    return "uint64_t";
  case tracer::ValueType::Float32:
    return "float";
  case tracer::ValueType::Float64:
    return "double";
  case tracer::ValueType::Pointer:
    return "void*";
  default:
    return "void";
  }
}

// The C++ type a DWARF-described integer has in the generated wrapper, or an empty string when the
// carrier's own type (`type_name`) already is the declared type. Narrow integers travel as int32 or
// uint64, but the wrapper must declare them as the callee does: a `short` or `unsigned int`
// parameter is passed and returned at its own width, and widening the prototype would change the
// ABI. `char` and `wchar_t` take their signedness from the carrier, so a target built with
// -funsigned-char still gets a matching wrapper.
std::string narrow_integer_type(tracer::ValueType carrier, std::string cpp_type) {
  for (bool stripped = true; stripped;) {
    stripped = false;
    for (const char *qualifier : {"const ", "volatile "}) {
      const std::string prefix = qualifier;
      if (::tracer::compat::starts_with(cpp_type, prefix)) {
        cpp_type.erase(0, prefix.size());
        stripped = true;
      }
    }
    for (const char *qualifier : {" const", " volatile"}) {
      const std::string suffix = qualifier;
      if (::tracer::compat::ends_with(cpp_type, suffix)) {
        cpp_type.erase(cpp_type.size() - suffix.size());
        stripped = true;
      }
    }
  }
  struct Narrow {
    const char *name;
    std::size_t size;
    bool is_signed;
    bool plain; // The signedness is the target's choice.
  };
  static const Narrow narrow[] = {
      {"char", 1, std::is_signed<char>::value, true},
      {"signed char", 1, true, false},
      {"unsigned char", 1, false, false},
      {"short", 2, true, false},
      {"unsigned short", 2, false, false},
      {"unsigned int", 4, false, false},
      {"char16_t", 2, false, false},
      {"char32_t", 4, false, false},
      {"wchar_t", sizeof(wchar_t), std::is_signed<wchar_t>::value, true}};
  for (const auto &entry : narrow) {
    if (cpp_type != entry.name)
      continue;
    if (carrier != tracer::ValueType::Int32 && carrier != tracer::ValueType::UInt64)
      throw std::runtime_error("debug type '" + cpp_type +
                               "' does not match its trace value type");
    const bool carrier_signed = carrier == tracer::ValueType::Int32;
    if (carrier_signed == entry.is_signed)
      return entry.name;
    if (!entry.plain)
      throw std::runtime_error("debug type '" + cpp_type +
                               "' does not match its trace value type");
    static const char *const signed_names[] = {"signed char", "short", "int"};
    static const char *const unsigned_names[] = {"unsigned char", "unsigned short", "unsigned int"};
    const auto width = entry.size == 1 ? 0 : entry.size == 2 ? 1 : 2;
    return carrier_signed ? signed_names[width] : unsigned_names[width];
  }
  return {};
}

// The C++ type an enumeration taken or returned by value has in the generated wrapper: the
// integer type of its underlying type, at its own width and signedness, for the same reason as
// narrow_integer_type (an enumeration over `unsigned char` is passed in a byte, not in a uint64).
// The underlying type is taken from its size and DWARF encoding, not from its name: GCC spells
// `unsigned short` as `short unsigned int`. The carrier must be the one that type travels as.
std::string enum_integer_type(tracer::ValueType carrier,
                              const clangtracer::v1::DebugTemplateType &enumeration) {
  const auto size = enumeration.byte_size();
  const auto encoding = enumeration.has_element() ? enumeration.element().dwarf_encoding() : 0;
  const bool boolean = encoding == 0x02;                         // DW_ATE_boolean
  const bool is_signed = encoding == 0x05 || encoding == 0x06;   // DW_ATE_signed, signed_char
  const auto mismatch = [&]() {
    return std::runtime_error("enumeration " + enumeration.name() +
                              " does not match its trace value type");
  };
  if (enumeration.has_element() && enumeration.element().byte_size() != size)
    throw mismatch();
  if (boolean) {
    if (carrier != tracer::ValueType::Bool || size != 1)
      throw mismatch();
    return "bool";
  }
  const bool carrier_ok =
      is_signed ? (size == 8 ? carrier == tracer::ValueType::Int64
                             : carrier == tracer::ValueType::Int32 && size <= 4)
                : carrier == tracer::ValueType::UInt64;
  if (!carrier_ok)
    throw mismatch();
  switch (size) {
  case 1: return is_signed ? "signed char" : "unsigned char";
  case 2: return is_signed ? "short" : "unsigned short";
  case 4: return is_signed ? "int" : "unsigned int";
  case 8: return type_name(carrier);
  default: throw mismatch();
  }
}

void fill_value(std::string &code, unsigned i, tracer::ValueType type,
                bool indirect_object) {
  code += "ctx.arguments[" + std::to_string(i) +
          "].type=static_cast<tracer::ValueType>(" +
          std::to_string(static_cast<unsigned>(type)) + ");\n";
  switch (type) {
  case tracer::ValueType::Int32:
  case tracer::ValueType::Int64:
    code += "ctx.arguments[" + std::to_string(i) +
            "].signed_value=static_cast<int64_t>(a" + std::to_string(i) +
            ");\n";
    break;
  case tracer::ValueType::Bool:
    code += "ctx.arguments[" + std::to_string(i) +
            "].signed_value=static_cast<int64_t>(a" + std::to_string(i) +
            ");\n";
    break;
  case tracer::ValueType::UInt64:
    code += "ctx.arguments[" + std::to_string(i) +
            "].unsigned_value=static_cast<uint64_t>(a" + std::to_string(i) +
            ");\n";
    break;
  case tracer::ValueType::Float32:
  case tracer::ValueType::Float64:
    code += "ctx.arguments[" + std::to_string(i) +
            "].real_value=static_cast<double>(a" + std::to_string(i) + ");\n";
    break;
  case tracer::ValueType::Pointer:
    code += "ctx.arguments[" + std::to_string(i) +
            "].unsigned_value=reinterpret_cast<uintptr_t>(a" +
            std::to_string(i) + ");\n";
    break;
  case tracer::ValueType::Object: {
    const auto argument = "a" + std::to_string(i);
    const auto address = indirect_object ? argument : "__builtin_addressof(" + argument + ")";
    code += "ctx.arguments[" + std::to_string(i) +
            "].unsigned_value=reinterpret_cast<uintptr_t>(" + address + ");\n";
    break;
  }
  default:
    break;
  }
}

class DiagnosticCaptureScope {
public:
  explicit DiagnosticCaptureScope(clang::DiagnosticsEngine &diagnostics)
      : diagnostics_(diagnostics), old_owner_(diagnostics_.takeClient()),
        consumer_(old_owner_.get()) {
    diagnostics_.setClient(&consumer_, false);
  }

  ~DiagnosticCaptureScope() {
    diagnostics_.setClient(old_owner_.release(), true);
  }

  const std::string &message() const { return consumer_.message(); }

  bool had_error() const { return consumer_.had_error(); }

private:
  clang::DiagnosticsEngine &diagnostics_;
  std::unique_ptr<clang::DiagnosticConsumer> old_owner_;
  DiagnosticCapture consumer_;
};

bool add_jump(std::uint8_t *at, const void *destination) {
  at[0] = 0xff;
  at[1] = 0x25;
  std::memset(at + 2, 0, 4);
  const auto address = reinterpret_cast<std::uintptr_t>(destination);
  std::memcpy(at + 6, &address, sizeof(address));
  return true;
}

bool mapping(const void *address, std::uintptr_t &begin, std::uintptr_t &end,
             int &protection) {
  std::ifstream maps("/proc/self/maps");
  std::string line;
  const auto needle = reinterpret_cast<std::uintptr_t>(address);
  while (std::getline(maps, line)) {
    unsigned long long lo = 0, hi = 0;
    char perms[5]{};
    if (std::sscanf(line.c_str(), "%llx-%llx %4s", &lo, &hi, perms) == 3 &&
        needle >= lo && needle < hi) {
      begin = lo;
      end = hi;
      protection = 0;
      if (perms[0] == 'r') {
        protection |= PROT_READ;
      }
      if (perms[1] == 'w') {
        protection |= PROT_WRITE;
      }
      if (perms[2] == 'x') {
        protection |= PROT_EXEC;
      }
      return true;
    }
  }
  return false;
}

bool decode_patch(const std::uint8_t *entry, tracer::Request &req,
                  tracer::native_detour::Patch &patch, std::string &error,
                  std::uint64_t function_size = 0) {
  std::uintptr_t lo = 0, hi = 0;
  int prot = 0;
  if (!mapping(entry, lo, hi, prot) || (prot & PROT_READ) == 0) {
    error = "target entry is not in a readable mapping";
    return false;
  }
  auto available = std::min<std::size_t>(
      tracer::native_detour::kMaxDecodedBytes,
      hi - reinterpret_cast<std::uintptr_t>(entry));
  if (function_size)
    available = std::min<std::uint64_t>(available, function_size);
  const auto address = reinterpret_cast<std::uintptr_t>(entry);
  if (!tracer::native_detour::decode({entry, available}, address, patch, error)) {
    // Only use the short relay when the resolver verified the function range.
    if (!function_size ||
        !tracer::native_detour::decode({entry, available}, address, patch, error,
                                      tracer::native_detour::kRelativeJumpBytes))
      return false;
    error.clear();
  }
  req.patch_size = static_cast<std::uint32_t>(patch.size);
  std::memcpy(req.original, patch.original.data(), patch.size);
  return true;
}

struct DirectCaller {
  std::uint64_t pc;
  std::string bytes;
};

std::optional<DirectCaller> verified_direct_caller(
    const clangtracer::v1::ProbeSpec &spec, std::uint64_t callee) {
  tracer::client::DebugLookupOptions lookup;
  lookup.module = spec.debug_info().module();
  const auto resolved = tracer::client::resolve_debug_callable(
      ::getpid(), spec.direct_caller(), lookup);
  const auto &caller = resolved.spec.debug_info();
  const auto code = tracer::client::verified_function_code(::getpid(), caller, 64 * 1024);
  const auto decoded = tracer::native_entry::scan(caller.runtime_entry(), code);
  if (decoded.decode_failed || !decoded.diagnostic.empty())
    return std::nullopt;
  std::optional<DirectCaller> result;
  std::uint64_t last_end = 0;
  for (const auto &inst : decoded.instructions) {
    if (inst.address < last_end)
      return std::nullopt;
    last_end = inst.address + inst.size;
    const auto offset = inst.address - caller.runtime_entry();
    if (inst.flow == tracer::native_entry::Flow::call && inst.target == callee &&
        inst.size == 5 && code[offset] == '\xe8') {
      if (result)
        return std::nullopt;
      result = DirectCaller{inst.address, code.substr(offset, inst.size)};
    }
  }
  return result;
}

bool prepare_getter_fallback(Probe &probe, const clangtracer::v1::ProbeSpec &spec) {
  namespace api = clangtracer::v1;
  auto &request = probe.request;
  if (!spec.has_debug_info() || spec.signature().kind() != api::METHOD ||
      spec.signature().result() != api::INT32 || spec.signature().arguments_size() ||
      spec.debug_info().entry_size() > tracer::kMaxPatchBytes)
    return false;
  std::string code, error;
  try {
    code = tracer::client::verified_function_code(::getpid(), spec.debug_info(), tracer::kMaxPatchBytes);
  } catch (const std::exception &) {
    return false;
  }
  std::size_t trap_offset = 0;
  if (!tracer::native_detour::decode_getter(code, trap_offset, error))
    return false;
  request.verified_size = code.size();
  std::memcpy(request.verified_bytes, code.data(), code.size());
  std::optional<DirectCaller> caller;
  const char *reason = trap_offset ? "landing_pad_requires_trap" : "no_verified_caller";
  if (!trap_offset && !spec.direct_caller().empty()) {
    try {
      caller = verified_direct_caller(spec, request.original_entry);
    } catch (const std::exception &) {
      // The explicit caller could not be proved. A verified entry trap can
      // still cover the getter and reports why the call-site path was skipped.
    }
    reason = caller ? "no_reachable_relay" : "caller_not_verified";
  }
  if (caller && tracer::native_detour::create_getter_copy(
                    code, caller->pc, probe.trampoline, probe.mapping_size, error)) {
    request.patch_backend = tracer::NativePatch::CallSite;
    request.patch_address = caller->pc;
    request.patch_size = caller->bytes.size();
    std::memcpy(request.original, caller->bytes.data(), caller->bytes.size());
    std::strcpy(request.patch_reason, "entry_jump_unavailable");
    return true;
  }
  if (!tracer::native_detour::create_getter_copy(
          code, 0, probe.trampoline, probe.mapping_size, error))
    return false;
  request.patch_backend = tracer::NativePatch::Trap;
  request.patch_address = request.original_entry + trap_offset;
  request.patch_size = 1;
  request.original[0] = static_cast<std::uint8_t>(code[trap_offset]);
  request.installed[0] = 0xcc;
  std::strcpy(request.patch_reason, reason);
  return true;
}

using PthreadCreate = tracer::context_scheduler::PthreadCreate;
template <class> struct PthreadCreateSignature;
template <class Result, class... Arguments>
struct PthreadCreateSignature<Result (*)(Arguments...)> {
  using result_type = Result;
  using argument_types = std::tuple<Arguments...>;
};
template <class Result, class... Arguments>
struct PthreadCreateSignature<Result (*)(Arguments...) noexcept> {
  using result_type = Result;
  using argument_types = std::tuple<Arguments...>;
};
using NativePthreadCreate = decltype(&::pthread_create);
using ExpectedPthreadArguments =
    std::tuple<pthread_t *, const pthread_attr_t *, void *(*)(void *), void *>;
static_assert(std::is_convertible_v<NativePthreadCreate, PthreadCreate>,
              "pthread_create pointer must safely convert to detour ABI type");
static_assert(
    std::is_same_v<
        typename PthreadCreateSignature<NativePthreadCreate>::result_type,
        int> &&
        std::is_same_v<typename PthreadCreateSignature<
                           NativePthreadCreate>::argument_types,
                       ExpectedPthreadArguments>,
    "pthread_create native argument and return ABI changed");

bool copy_pthread_patch(const tracer::Request &source,
                        tracer::Request &destination) noexcept {
  destination.pthread_create_entry = source.pthread_create_entry;
  destination.pthread_create_trampoline = source.pthread_create_trampoline;
  destination.pthread_create_wrapper = source.pthread_create_wrapper;
  destination.pthread_create_patch_size = source.pthread_create_patch_size;
  std::memcpy(destination.pthread_create_original,
              source.pthread_create_original,
              sizeof(destination.pthread_create_original));
  std::memcpy(destination.pthread_create_installed,
              source.pthread_create_installed,
              sizeof(destination.pthread_create_installed));
  return destination.pthread_create_patch_size != 0;
}

bool native_cet_disabled(std::string &error) {
  try {
    bool checked = false;
    for (const auto &task :
         std::filesystem::directory_iterator("/proc/self/task")) {
      std::ifstream status(task.path() / "status");
      if (!status) {
        if (!std::filesystem::exists(task.path()))
          continue;
        error = "cannot verify pthread_create control-flow protection state";
        return false;
      }
      bool found = false;
      for (std::string line; std::getline(status, line);) {
        constexpr std::string_view prefix = "x86_Thread_features:";
        if (!::tracer::compat::starts_with(line, prefix)) {
          continue;
        }
        found = true;
        if (line.find_first_not_of(" \t\r", prefix.size()) !=
            std::string::npos) {
          error = "pthread_create adapter does not support active CET thread "
                  "features";
          return false;
        }
      }
      if (!found) {
        error = "kernel does not expose pthread_create CET admission state";
        return false;
      }
      checked = true;
    }
    return checked;
  } catch (const std::exception &exception) {
    error = exception.what();
    return false;
  }
}

bool prepare_context_pthread(Agent &owner, Probe &probe, std::string &error) {
  if (!probe.request.context_required)
    return true;
  if (!native_cet_disabled(error))
    return false;
  if (owner.pthread_detour) {
    if (owner.pthread_trampoline == MAP_FAILED ||
        !owner.pthread_detour->enable_propagation() ||
        !copy_pthread_patch(owner.pthread_patch, probe.request)) {
      error = "previous pthread_create detour is not reusable";
      return false;
    }
    return true;
  }

  ::dlerror();
  void *symbol = ::dlsym(RTLD_DEFAULT, "pthread_create");
  if (const char *reason = ::dlerror()) {
    error = std::string("pthread_create symbol lookup failed: ") + reason;
    return false;
  }
  if (!symbol) {
    error = "pthread_create symbol lookup returned null";
    return false;
  }
  Dl_info module{};
  if (::dladdr(symbol, &module) == 0 || !module.dli_fname ||
      !module.dli_fbase) {
    error = "pthread_create module identity is unavailable";
    return false;
  }
  if (!tracer::runtime::has_soname(symbol, "libc.so.6") &&
      !tracer::runtime::has_soname(symbol, "libpthread.so.0")) {
    error = "pthread_create is interposed by an unsupported runtime module";
    return false;
  }
  struct stat module_status{};
  if (::stat(module.dli_fname, &module_status) != 0 ||
      !S_ISREG(module_status.st_mode)) {
    error = "pthread_create ELF module identity could not be verified";
    return false;
  }
  std::uintptr_t map_begin = 0, map_end = 0;
  int protection = 0;
  if (!mapping(symbol, map_begin, map_end, protection) ||
      (protection & PROT_EXEC) == 0) {
    error = "pthread_create is not in an executable mapping";
    return false;
  }
  tracer::Request patch{};
  auto *patch_entry = static_cast<const std::uint8_t *>(symbol);
  if (patch_entry[0] == 0xf3 && patch_entry[1] == 0x0f &&
      patch_entry[2] == 0x1e && patch_entry[3] == 0xfa) {
    // Preserve libc's landing pad. With CET disabled its four bytes are a
    // no-op; the relocated prologue begins immediately after it.
    patch_entry += 4;
  }
  patch.pthread_create_entry = reinterpret_cast<std::uintptr_t>(patch_entry);
  tracer::native_detour::Patch scheduler_patch;
  if (!decode_patch(patch_entry, patch, scheduler_patch, error))
    return false;
  patch.pthread_create_patch_size = patch.patch_size;
  std::memcpy(patch.pthread_create_original, patch.original, patch.patch_size);
  const auto probe_begin = probe.request.patch_address;
  const auto probe_end = probe_begin + probe.request.patch_size;
  const auto scheduler_end = patch.pthread_create_entry + patch.patch_size;
  if (probe_begin < scheduler_end && patch.pthread_create_entry < probe_end) {
    error = "pthread_create patch overlaps the target probe";
    return false;
  }
  for (std::size_t i = 0; i < owner.probes.size(); ++i) {
    const auto *existing = owner.probes[i].get();
    if (!existing || !existing->active.load(std::memory_order_acquire))
      continue;
    const auto existing_begin = existing->request.patch_address;
    const auto existing_end = existing_begin + existing->request.patch_size;
    if (existing_begin < scheduler_end &&
        patch.pthread_create_entry < existing_end) {
      error = "pthread_create patch overlaps an active target probe";
      return false;
    }
  }

  std::size_t trampoline_size = 0;
  void *trampoline = MAP_FAILED;
  if (!tracer::native_detour::create_trampoline(
          scheduler_patch, trampoline, trampoline_size, error))
    return false;

  patch.pthread_create_trampoline =
      reinterpret_cast<std::uintptr_t>(trampoline);
  patch.pthread_create_wrapper = reinterpret_cast<std::uintptr_t>(
      &tracer::context_scheduler::tracer_context_pthread_create_detour);
  patch.pthread_create_patch_size = patch.patch_size;
  std::array<std::uint8_t, tracer::kMaxPatchBytes> installed{};
  add_jump(installed.data(),
           reinterpret_cast<void *>(patch.pthread_create_wrapper));
  std::fill(installed.begin() + kJumpBytes,
            installed.begin() + patch.pthread_create_patch_size, 0x90);
  std::memcpy(patch.pthread_create_installed, installed.data(),
              patch.pthread_create_patch_size);

  auto ledger = std::make_shared<tracer::context_engine::PropagationLedger>(
      tracer::context_engine::PropagationLimits{64, 4096, 3 * 1024 * 1024, 64});
  auto detour =
      std::make_unique<tracer::context_scheduler::ContextPthreadDetour>(
          ledger, &tracer::context_runtime::active_context,
          &tracer::context_runtime::worker_context,
          &tracer::context_runtime::exchange_active_context);
  if (!detour->activate(reinterpret_cast<PthreadCreate>(trampoline))) {
    ::munmap(trampoline, trampoline_size);
    error = "cannot activate native pthread_create adapter";
    return false;
  }
  owner.pthread_trampoline = trampoline;
  owner.pthread_trampoline_size = trampoline_size;
  owner.pthread_patch = patch;
  owner.propagation_ledger = std::move(ledger);
  owner.pthread_detour = std::move(detour);
  copy_pthread_patch(owner.pthread_patch, probe.request);
  return true;
}

std::string make_wrapper(const tracer::Request &r,
                         std::uintptr_t original_trampoline, bool has_pre,
                         bool has_post,
                         const clangtracer::v1::ProbeSpec *spec = nullptr,
                         bool class_mode = false, bool shared_mode = false,
                         bool result_sret = false,
                         const tracer::ValueSnapshotCode *snapshots = nullptr) {
  const bool typed = spec && tracer::prepared::is_typed_version(
                                  spec->prepared().format_version());
  const bool override_arguments = spec && spec->allow_argument_overrides();
  const bool override_result = spec && spec->allow_result_override();
  const bool replace = spec && spec->allow_replacement();
  const bool callable_original = spec && spec->allow_call_original();
  const bool trace_exceptions = spec && spec->trace_exceptions();
  const bool overrides = override_arguments || override_result || replace || callable_original;
  // At constructor entry the receiver is raw storage (no vptr, no members): reading it through
  // its class layout can fault. Sample it when the constructor returns instead.
  const bool constructor_entry = spec && tracer::client::is_constructor_symbol(spec->symbol());
  const bool refresh_receiver =
      (spec && spec->snapshot_receiver_at_return()) ||
      (constructor_entry && typed && r.kind == tracer::CallableKind::Method && !shared_mode &&
       has_pre && has_post && snapshots && !snapshots->receiver.empty());
  const bool observe_returns = tracer::agent_has_observers(r.session_id);
  // A variadic callable is observed by its fixed arguments only. The wrapper is entered by an
  // entry stub that saved the whole argument state, runs PRE, and returns to the stub, which
  // restores that state and jumps to the original: the unknown tail is never re-passed.
  const bool variadic = spec && spec->debug_info().callable_is_variadic();
  if (variadic && (shared_mode || overrides || trace_exceptions || refresh_receiver))
    throw std::invalid_argument("a variadic callable supports only PRE observation of its fixed arguments");
  if (refresh_receiver && (!typed || r.kind != tracer::CallableKind::Method ||
                           shared_mode || !has_pre || !has_post || !snapshots ||
                           snapshots->receiver.empty()))
    throw std::invalid_argument("receiver return sampling requires a typed prepared method with PRE and POST capture");
  const auto indirect_argument = [&](unsigned i) {
    return typed && r.arguments[i] == tracer::ValueType::Object &&
           spec->debug_info().arguments(i).object_type().record_pass_by_reference();
  };
  const auto result_type = [&]() -> std::string {
    if (typed && r.result != tracer::ValueType::Pointer)
      return "tracer::ResolvedResult";
    if (r.result != tracer::ValueType::Object) {
      if (spec && spec->has_debug_info()) {
        if (spec->debug_info().has_result_enum_type() &&
            r.result != tracer::ValueType::Pointer && r.result != tracer::ValueType::Void)
          return enum_integer_type(r.result, spec->debug_info().result_enum_type());
        if (auto narrow = narrow_integer_type(r.result, spec->debug_info().result_cpp_type());
            !narrow.empty())
          return narrow;
      }
      return type_name(r.result);
    }
    if (!spec || spec->debug_info().result_cpp_type().empty())
      throw std::runtime_error("missing object result ABI metadata");
    return spec->debug_info().result_cpp_type();
  };
  const auto argument_type = [&](unsigned i) -> std::string {
    if (indirect_argument(i))
      return "void*";
    if (typed && r.arguments[i] != tracer::ValueType::Pointer)
      return "tracer::ResolvedArgument<" + std::to_string(i) + ">";
    if (r.arguments[i] != tracer::ValueType::Object) {
      if (spec && spec->has_debug_info() &&
          i < static_cast<unsigned>(spec->debug_info().arguments_size())) {
        const auto &argument = spec->debug_info().arguments(i);
        if (argument.has_enum_type() && r.arguments[i] != tracer::ValueType::Pointer)
          return enum_integer_type(r.arguments[i], argument.enum_type());
        if (auto narrow = narrow_integer_type(r.arguments[i], argument.cpp_type());
            !narrow.empty())
          return narrow;
      }
      return type_name(r.arguments[i]);
    }
    if (!spec || i >= static_cast<unsigned>(spec->debug_info().arguments_size()))
      throw std::runtime_error("missing object argument ABI metadata");
    const auto &argument = spec->debug_info().arguments(i);
    if (!argument.is_object_value() || !argument.has_object_type())
      throw std::runtime_error("invalid by-value argument metadata");
    return argument.cpp_type();
  };
  std::string code = (snapshots ? snapshots->declarations : std::string()) +
      "extern \"C\" long long tracer_callback_dispatch(unsigned long "
      "long,const tracer::TraceContext*,void*);\nextern \"C\" void "
      "tracer_event_dispatch(unsigned long long,const "
      "tracer::TraceContext*,long long) noexcept;\nextern \"C\" void "
      "tracer_probe_enter(unsigned long long) noexcept;\nextern \"C\" void "
      "tracer_probe_leave(unsigned long long) noexcept;\nextern \"C\" "
      "unsigned long long tracer_next_call_id(unsigned long long) noexcept;\n";
  if (observe_returns)
    code += "extern \"C\" unsigned long long tracer_observer_clock() noexcept;\n"
            "extern \"C\" void tracer_observer_return(unsigned long long,unsigned long long,"
            "unsigned long long,unsigned long long) noexcept;\n";
  code += "extern \"C\" bool tracer_object_identity_lookup(unsigned long long,unsigned long long,std::uint64_t*,std::uint64_t*,bool*) noexcept;\n";
  code +=
      "extern \"C\" void tracer_stack_enter(tracer::TraceContext*,void*) "
      "noexcept;\n"
      "extern \"C\" void tracer_stack_leave(tracer::TraceContext*) noexcept;\n"
      "extern \"C\" void tracer_capture_entry_stack(const "
      "tracer::TraceContext*,unsigned) noexcept;\n"
      "extern \"C\" void tracer_capture_stack(const "
      "tracer::TraceContext*,unsigned,bool) noexcept;\n";
  code += "extern \"C\" void tracer_context_check_entry();\n";

  if (shared_mode || trace_exceptions || callable_original)
    code += "#include <bits/cxxabi_forced.h>\n";
  if (callable_original)
    code += "#include <exception>\n#include <cerrno>\n"
            "extern \"C\" unsigned tracer_swap_callback_depth(unsigned) noexcept;\n"
            "extern \"C\" unsigned long long tracer_original_entry_time() noexcept;\n";
  if (shared_mode)
    code += "extern \"C\" void* tracer_shared_enter(unsigned long long,const "
            "tracer::TraceContext*,unsigned);\n"
            "extern \"C\" void tracer_shared_leave(void*) noexcept;\n"
            "extern \"C\" void tracer_shared_release(void*) noexcept;\n"
            "extern \"C\" void tracer_shared_resume(void*);\n"
            "extern \"C\" void tracer_shared_dispatch(void*,const "
            "tracer::TraceContext*);\n"
            "extern \"C\" void tracer_shared_original_enter(void*);\n"
            "extern \"C\" void tracer_shared_original_leave(void*) noexcept;\n";
  code += "using Original= ";
  if (result_sret)
    code += result_type() + "* (*) (" + result_type() + "*";
  else
    code += result_type() + (overrides ? " (__attribute__((sysv_abi)) *) (" : " (*) (");
  bool comma = false;
  if (result_sret)
    comma = true;
  if (r.kind == tracer::CallableKind::Method) {
    if (comma)
      code += ",";
    code += "void*";
    comma = true;
  }
  for (unsigned i = 0; i < r.argument_count; ++i) {
    if (comma) {
      code += ",";
    }
    code += argument_type(i);
    comma = true;
  }
  if (!comma) {
    code += "void";
  }
  // A patched entry can have callers that relaxed stack alignment for the
  // original callee. Realign before entering the wrapper and calling helpers.
  code += ");\nextern \"C\" __attribute__((force_align_arg_pointer)) ";
  if (overrides)
    code += "__attribute__((sysv_abi)) ";
  code += variadic
              ? std::string("void clangtracer_wrapper(") +
                    (result_sret ? result_type() + "* result_slot" : std::string())
          : result_sret ? result_type() + "* clangtracer_wrapper(" + result_type() +
                              "* result_slot"
                        : result_type() + " clangtracer_wrapper(";
  comma = result_sret;
  if (r.kind == tracer::CallableKind::Method) {
    if (comma)
      code += ",";
    code += "void* self";
    comma = true;
  }
  for (unsigned i = 0; i < r.argument_count; ++i) {
    if (comma) {
      code += ",";
    }
    code += argument_type(i) + " a" + std::to_string(i);
    comma = true;
  }
  if (!comma) {
    code += "void";
  }
  code += "){\n";
  code += "tracer_context_check_entry();\ntracer_probe_enter(" +
          std::to_string(r.probe_id) +
          "ULL); struct FlightGuard{unsigned long long id; "
          "~FlightGuard(){if(id)tracer_probe_leave(id);}} flight{" +
          std::to_string(r.probe_id) +
          "ULL};\nstatic constexpr tracer::TraceContext empty_context{}; "
          "tracer::TraceContext ctx=empty_context; ctx.probe_id=" +
          std::to_string(r.probe_id) + "ULL; ctx.call_id=tracer_next_call_id(" +
          std::to_string(r.probe_id) +
          "ULL); ctx.argument_count=" + std::to_string(r.argument_count) +
          "; ctx.phase=tracer::TracePhase::Pre;\n";
  code += "tracer::TraceValue arguments[" + std::to_string(std::max(1u, r.argument_count)) +
          "]{}; const char *argument_names[" + std::to_string(std::max(1u, r.argument_count)) +
          "]{}; ctx.arguments=arguments; ctx.argument_names=argument_names;\n";
  if (overrides) {
    code += "std::uint64_t override_values[" + std::to_string(std::max(1u, r.argument_count)) +
            "]{}; unsigned char override_set[" + std::to_string(std::max(1u, r.argument_count)) +
            "]{}; tracer::ScalarOverrides overrides{}; overrides.arguments=override_values; "
            "overrides.argument_set=override_set; ctx.scalar_overrides=&overrides;\n";
    if (override_arguments)
      code += "overrides.allow_arguments=true;\n";
    if (override_result)
      code += "overrides.allow_result=true;\n";
    if (replace)
      code += "overrides.allow_replacement=true; ctx.result.type=static_cast<tracer::ValueType>(" +
              std::to_string(static_cast<unsigned>(r.result)) + ");\n";
  }
  code += "ctx.target_entry=" + std::to_string(r.original_entry) +
          "ULL; ctx.bridge_begin=__extension__ reinterpret_cast<uintptr_t>(&&original_begin);"
          "ctx.bridge_end=__extension__ reinterpret_cast<uintptr_t>(&&original_end);"
          "ctx.stack_capture=&tracer_capture_stack;"
          "tracer_stack_enter(&ctx,__builtin_frame_address(0));"
          "struct StackGuard { tracer::TraceContext *ctx;"
          "~StackGuard(){if(ctx)tracer_stack_leave(ctx);} } stack_guard{&ctx};\n";
  code +=
      std::string("ctx.stack_application_only=") +
      (spec && spec->include_internal_stack_frames() ? "false;\n" : "true;\n");
  code += std::string("ctx.printing.arguments=") +
          (spec && spec->print_arguments() ? "true;\n" : "false;\n");
  code += std::string("ctx.printing.result=") +
          (spec && spec->print_result() ? "true;\n" : "false;\n");
  if (r.kind == tracer::CallableKind::Method) {
    code += "ctx.object_address=reinterpret_cast<uintptr_t>(self);\n";
  }
  const bool grouped = r.object_watch_id != 0;
  const std::string admit = grouped ? "if(object_identity_active) " : "";
  if (grouped) {
    code += "ctx.object_watch_id=" + std::to_string(r.object_watch_id) +
            "ULL; const bool object_identity_active=tracer_object_identity_lookup(" +
            std::to_string(r.object_watch_id) +
            "ULL,ctx.object_address,&ctx.object_id,&ctx.object_generation,&ctx.object_identity_verified);\n";
  }
  if (snapshots) {
    if (grouped) {
      code += snapshots->storage;
    } else {
      code += snapshots->scope + snapshots->argument_storage +
              snapshots->receiver_storage;
    }
  }
  for (unsigned i = 0; i < r.argument_count; ++i) {
    fill_value(code, i, r.arguments[i], indirect_argument(i));
  }
  if (snapshots) {
    if (grouped)
      code += "if(object_identity_active){\n" + snapshots->arguments +
              (constructor_entry ? std::string() : snapshots->receiver) + "}\n";
    else
      code += snapshots->arguments + (constructor_entry ? std::string() : snapshots->receiver);
  }
  if (!shared_mode && spec && spec->stack_depth())
    code += "tracer_capture_entry_stack(&ctx," +
            std::to_string(spec->stack_depth()) + ");\n";
  const auto original_declaration = "auto original=reinterpret_cast<Original>(" +
          std::to_string(original_trampoline) + "ULL);\n";
  std::string call = result_sret ? "original(result_slot" : "original(";
  comma = result_sret;
  if (r.kind == tracer::CallableKind::Method) {
    if (comma)
      call += ",";
    call += "self";
    comma = true;
  }
  for (unsigned i = 0; i < r.argument_count; ++i) {
    if (comma) {
      call += ",";
    }
    const auto argument = "a" + std::to_string(i);
    call += typed && r.arguments[i] == tracer::ValueType::Object && !indirect_argument(i)
                ? "std::move_if_noexcept(" + argument + ")" : argument;
    comma = true;
  }
  call += ")";
  if (callable_original) {
    code += original_declaration;
    code += "ctx.result.type=static_cast<tracer::ValueType>(" +
            std::to_string(static_cast<unsigned>(r.result)) + ");\n"
            "bool original_called=false; uint64_t original_bits=0; "
            "std::exception_ptr original_failure; "
            "const int entry_errno=errno; int original_errno=entry_errno; "
            "ctx.original_entry_time_ns=tracer_original_entry_time();\n"
            "auto early_original = [&]() -> uint64_t { "
            "if (!original_called) { original_called=true; "
            "struct OriginalScope { unsigned depth; int error; "
            "~OriginalScope() { tracer_swap_callback_depth(depth); errno=error; } }; "
            "OriginalScope scope{tracer_swap_callback_depth(0), errno}; errno=entry_errno; "
            "try { ";
    if (r.result == tracer::ValueType::Void)
      code += call + "; ";
    else if (r.result == tracer::ValueType::Int32 || r.result == tracer::ValueType::Int64 ||
             r.result == tracer::ValueType::UInt64)
      // The callback reads the original's result at the carrier's width, so a narrow result is
      // extended by its own signedness before it is stored.
      code += std::string("original_bits=tracer::ScalarOverrides::pack(static_cast<") +
              type_name(r.result) + ">(" + call + ")); ";
    else
      code += "original_bits=tracer::ScalarOverrides::pack(" + call + "); ";
    code += "} catch (const __cxxabiv1::__forced_unwind&) { throw; } "
            "catch (...) { original_failure=std::current_exception(); } "
            "original_errno=errno; } "
            "if (original_failure) std::rethrow_exception(original_failure); "
            "return original_bits; };\n"
            "ctx.original_state=&early_original; "
            "ctx.invoke_original=+[](void* state) -> uint64_t { "
            "return (*static_cast<decltype(early_original)*>(state))(); };\n"
            "auto run_original = [&]() -> " + result_type() + " { "
            "if (!original_called) return " + call + "; "
            "errno=original_errno; if (original_failure) std::rethrow_exception(original_failure); ";
    if (r.result != tracer::ValueType::Void)
      code += "return tracer::ScalarOverrides::unpack<" + result_type() + ">(original_bits); ";
    code += "};\n";
    call = "run_original()";
  }
  if (shared_mode) {
    code += "struct SharedGuard { void* token=nullptr; ~SharedGuard() noexcept "
            "{ if(token){tracer_shared_leave(token); tracer_shared_release(token);} } } "
            "shared_guard;\n" + admit + "shared_guard.token=tracer_shared_enter(" +
            std::to_string(r.probe_id) + "ULL,&ctx," +
            std::to_string(spec ? spec->stack_depth() : 0) + "U);\ntry { " +
            (grouped ? "if(shared_guard.token) " : "") +
            "tracer_shared_resume(shared_guard.token);\n";
  } else if (class_mode) {
    if (grouped)
      code += "std::optional<tracer_invocation_instance> tracer_invocation;"
              "if(object_identity_active) tracer_invocation.emplace(ctx);\n";
    else
      code += "tracer_invocation_instance tracer_invocation(ctx);\n";
  }
  const std::string instance =
      class_mode ? (grouped
                        ? "(tracer_invocation ? &*tracer_invocation : nullptr)"
                        : "&tracer_invocation")
                 : "nullptr";
  if (has_pre) {
    if (shared_mode)
      code += admit + "tracer_shared_dispatch(shared_guard.token,&ctx);\n";
    else
      code += admit + "(void)tracer_callback_dispatch(" + std::to_string(r.probe_id) +
              "ULL,&ctx," + instance + ");\n";
  }
  if (variadic) {
    // The entry stub forwards the call after this returns; the labels only keep the
    // context's bridge range well-formed.
    code += "original_begin: ;\noriginal_end: ;\nreturn;\n}";
    return code;
  }
  if (callable_original)
    code += "ctx.invoke_original=nullptr; ctx.original_state=nullptr;\n";
  if (override_arguments) {
    for (unsigned i = 0; i < r.argument_count; ++i) {
      const auto index = std::to_string(i);
      const auto argument = "a" + index;
      code += "if (overrides.argument_set[" + index + "]) { " + argument +
              "=tracer::ScalarOverrides::unpack<decltype(" + argument + ")>(overrides.arguments[" +
              index + "]);\n";
      fill_value(code, i, r.arguments[i], false);
      code += "}\n";
    }
  }
  if (!callable_original)
    code += original_declaration;
  if (trace_exceptions) {
    code += "auto report_original = [&]() -> decltype(auto) { try { return " + call +
            "; } catch (const __cxxabiv1::__forced_unwind&) { throw; } catch (...) { "
            "ctx.phase=tracer::TracePhase::Exception; tracer_event_dispatch(" +
            std::to_string(r.probe_id) + "ULL,&ctx,0); throw; } };\n";
    call = "report_original()";
  }
  if (shared_mode && r.result != tracer::ValueType::Void)
    if (r.result != tracer::ValueType::Object)
      code += result_type() + " result;\n";
  if (observe_returns)
    code += "const auto observer_start=tracer_observer_clock();\n";
  if (shared_mode && r.result != tracer::ValueType::Object)
    code += "{ tracer_shared_original_enter(shared_guard.token); struct "
            "OriginalGuard { void* token; ~OriginalGuard() noexcept { "
            "tracer_shared_original_leave(token); } } "
            "original_guard{shared_guard.token};\n";
  if (r.result == tracer::ValueType::Void) {
    code += std::string("original_begin: ") +
            (replace ? "if (!overrides.replacement_set) " : "") + call +
            ";\noriginal_end: ctx.result.type=tracer::ValueType::Void;\n";
  } else if (result_sret) {
    if (shared_mode)
      code += "{ tracer_shared_original_enter(shared_guard.token); struct "
              "OriginalGuard { void* token; ~OriginalGuard() noexcept { "
              "tracer_shared_original_leave(token); } } "
              "original_guard{shared_guard.token};\n";
    code += "original_begin: result_slot=" + call +
            ";\noriginal_end: ;\nctx.result.type=static_cast<tracer::ValueType>(" +
            std::to_string(static_cast<unsigned>(r.result)) +
            ");\nctx.result.unsigned_value=reinterpret_cast<uintptr_t>(result_slot);\n";
    if (shared_mode)
      code += "}\n";
  } else {
    if (r.result == tracer::ValueType::Object) {
      code += "original_begin: ; " + result_type() +
              " result=([&]() -> " + result_type() + " { ";
      if (shared_mode)
        code += "tracer_shared_original_enter(shared_guard.token); struct "
                "OriginalGuard { void* token; ~OriginalGuard() noexcept { "
                "tracer_shared_original_leave(token); } } "
                "original_guard{shared_guard.token}; ";
      code += "return " + call + "; })();\noriginal_end: ;\n";
    } else {
      code += (shared_mode ? std::string()
                           : result_type() + " result; ") +
              "original_begin: result=" +
              (replace ? "overrides.replacement_set ? tracer::ScalarOverrides::unpack<decltype(result)>"
                         "(overrides.replacement) : " : "") + call + ";\noriginal_end: ;\n";
    }
    code += "ctx.result.type=static_cast<tracer::ValueType>(" +
            std::to_string(static_cast<unsigned>(r.result)) + ");\n";
    switch (r.result) {
    case tracer::ValueType::Int32:
    case tracer::ValueType::Int64:
      code += "ctx.result.signed_value=static_cast<int64_t>(result);\n";
      break;
    case tracer::ValueType::Bool:
      code += "ctx.result.signed_value=static_cast<int64_t>(result);\n";
      break;
    case tracer::ValueType::UInt64:
      code += "ctx.result.unsigned_value=static_cast<uint64_t>(result);\n";
      break;
    case tracer::ValueType::Float32:
    case tracer::ValueType::Float64:
      code += "ctx.result.real_value=static_cast<double>(result);\n";
      break;
    case tracer::ValueType::Pointer:
      code +=
          "ctx.result.unsigned_value=reinterpret_cast<uintptr_t>(result);\n";
      break;
    case tracer::ValueType::Object:
      code +=
          "ctx.result.unsigned_value=reinterpret_cast<uintptr_t>(__builtin_addressof(result));\n";
      break;
    default:
      break;
    }
  }
  if (shared_mode && r.result != tracer::ValueType::Object)
    code += "}\n";
  if (observe_returns)
    code += "const auto observer_end=observer_start?tracer_observer_clock():0;\n";
  if (has_post) {
    if (refresh_receiver) {
      if (grouped)
        code += "if(object_identity_active){ auto* entry_receiver_snapshot=ctx.receiver_snapshot;\n"
                "{ tracer_snapshot_detail::Scope return_receiver_scope;\n" +
                snapshots->receiver_storage + snapshots->receiver;
      else
        code += "auto* entry_receiver_snapshot=ctx.receiver_snapshot;\n"
                "{ tracer_snapshot_detail::Scope return_receiver_scope;\n" +
                snapshots->receiver_storage + snapshots->receiver;
    }
    if (snapshots) {
      if (grouped)
        code += "if(object_identity_active){ ctx.phase=tracer::TracePhase::Post;\n" +
                snapshots->result + "}\n";
      else
        code += "ctx.phase=tracer::TracePhase::Post;\n" +
                snapshots->result_storage + snapshots->result;
    }
    if (shared_mode)
      code += "ctx.phase=tracer::TracePhase::Post; " + admit +
              "tracer_shared_dispatch(shared_guard.token,&ctx);\n";
    else
      code += "ctx.phase=tracer::TracePhase::Post; " + admit +
              "(void)tracer_callback_dispatch(" +
              std::to_string(r.probe_id) + "ULL,&ctx," + instance + ");\n";
    if (refresh_receiver) {
      if (grouped)
        code += "} ctx.receiver_snapshot=entry_receiver_snapshot; }\n";
      else
        code += "} ctx.receiver_snapshot=entry_receiver_snapshot;\n";
    }
  }
  if (override_result)
    code += "if (overrides.result_set) result="
            "tracer::ScalarOverrides::unpack<decltype(result)>(overrides.result);\n";
  if (shared_mode)
    code += "tracer_shared_leave(shared_guard.token); "
            "tracer_shared_resume(shared_guard.token); "
            "tracer_shared_release(shared_guard.token); "
            "shared_guard.token=nullptr;\n";
  if (observe_returns)
    code += "if(observer_end)tracer_observer_return(ctx.probe_id,ctx.call_id,"
            "observer_start,observer_end);\n";
  if (result_sret) {
    if (class_mode)
      code += grouped
                  ? "if(tracer_invocation) tracer_invocation->destroy();\n"
                  : "tracer_invocation.destroy();\n";
    if (snapshots && !snapshots->scope.empty())
      code += "snapshot_scope.release();\n";
    code += "tracer_stack_leave(&ctx); stack_guard.ctx=nullptr; "
            "tracer_probe_leave(" + std::to_string(r.probe_id) +
            "ULL); flight.id=0; return result_slot;\n";
  } else if (r.result == tracer::ValueType::Void) {
    code += "return;\n";
  } else {
    code += "return result;\n";
  }
  if (shared_mode) {
    code += "}\n#if defined(__GLIBCXX__)\ncatch (const "
            "__cxxabiv1::__forced_unwind&) { throw; }\n#endif\ncatch "
            "(...) { tracer_shared_leave(shared_guard.token); "
            "tracer_shared_resume(shared_guard.token); "
            "tracer_shared_release(shared_guard.token); "
            "shared_guard.token=nullptr; throw; }\n";
  }
  code += "}";
  return code;
}

struct CallbackShape {
  bool class_mode = false;
  bool has_pre = false;
  bool has_post = false;
  bool encoder_only = false;
  bool signature_specialization = false;
  std::string interceptor_type = "Interceptor";
  std::string specialized_signature_spelling;
  clang::QualType specialized_signature;
  const clang::FunctionDecl *legacy = nullptr;
  const clang::CXXMethodDecl *pre = nullptr;
  const clang::CXXMethodDecl *post = nullptr;
};

bool is_long_long(clang::QualType type) {
  const auto *builtin = type->getAs<clang::BuiltinType>();
  return builtin && builtin->getKind() == clang::BuiltinType::LongLong;
}

bool is_call_reference(clang::QualType type, llvm::StringRef name,
                       bool require_const = true) {
  const auto *reference = type->getAs<clang::LValueReferenceType>();
  if (!reference ||
      (require_const && !reference->getPointeeType().isConstQualified())) {
    return false;
  }
  const auto *record = reference->getPointeeType()->getAsCXXRecordDecl();
  return record && llvm::StringRef(record->getQualifiedNameAsString()) == name;
}

bool has_identifier(const clang::NamedDecl &declaration, llvm::StringRef name) {
  const clang::IdentifierInfo *identifier = declaration.getIdentifier();
  return identifier && identifier->getName() == name;
}

bool inspect_callback_shape(clang::ASTContext &context, CallbackShape &shape,
                            std::string &error,
                            bool allow_shared_context = false) {
  if (!allow_shared_context)
    error = tracer::context_activation_error(
        tracer::analyze_context_declarations(context));
  if (!error.empty())
    return false;
  const clang::TranslationUnitDecl *unit = context.getTranslationUnitDecl();
  const clang::CXXRecordDecl *interceptor = nullptr;
  const clang::FunctionDecl *legacy = nullptr;
  std::vector<const clang::FunctionDecl *> legacy_declarations;
  bool saw_interceptor = false;
  bool saw_legacy = false;
  auto inspect_function = [&](const clang::Decl *declaration) {
    const auto *function = llvm::dyn_cast<clang::FunctionDecl>(declaration);
    if (!function || !has_identifier(*function, "trace_callback")) {
      return;
    }
    saw_legacy = true;
    const auto *canonical = function->getCanonicalDecl();
    if (std::find(legacy_declarations.begin(), legacy_declarations.end(),
                  canonical) == legacy_declarations.end()) {
      legacy_declarations.push_back(canonical);
    }
    if (function->doesThisDeclarationHaveABody()) {
      if (legacy) {
        error = "callback source defines trace_callback more than once";
        return;
      }
      legacy = function;
    }
  };
  for (const clang::Decl *decl : unit->decls()) {
    if (const auto *record = llvm::dyn_cast<clang::CXXRecordDecl>(decl);
        record && has_identifier(*record, "Interceptor")) {
      saw_interceptor = true;
      if (record->isThisDeclarationADefinition()) {
        if (interceptor) {
          error = "callback source defines more than one top-level Interceptor";
          return false;
        }
        interceptor = record;
        if (const auto *specialization =
                llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record)) {
          shape.signature_specialization = true;
          if (specialization->getTemplateArgs().size() != 1 ||
              specialization->getTemplateArgs()[0].getKind() !=
                  clang::TemplateArgument::Type) {
            error = "Interceptor specialization requires one function signature";
            return false;
          }
          shape.specialized_signature =
              specialization->getTemplateArgs()[0].getAsType();
          clang::PrintingPolicy policy(context.getLangOpts());
          shape.specialized_signature_spelling =
              shape.specialized_signature.getAsString(policy);
          shape.interceptor_type =
              tracer::prepared::record_type(context, specialization).getAsString(policy);
        }
      }
    }
    if (const auto *linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(decl)) {
      for (const clang::Decl *nested : linkage->decls()) {
        inspect_function(nested);
        if (!error.empty()) {
          return false;
        }
      }
    } else {
      inspect_function(decl);
      if (!error.empty()) {
        return false;
      }
    }
  }
  if (saw_interceptor && !interceptor) {
    error = "Interceptor must be a complete top-level class or struct";
    return false;
  }
  if (interceptor && saw_legacy) {
    error = "callback source cannot define both Interceptor and trace_callback";
    return false;
  }
  if (legacy_declarations.size() > 1) {
    error = "trace_callback must not be overloaded";
    return false;
  }
  if (interceptor) {
    shape.class_mode = true;
    for (const clang::Decl *member : interceptor->decls()) {
      const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(member);
      if (!method || (!has_identifier(*method, "pre") &&
                      !has_identifier(*method, "post"))) {
        continue;
      }
      const bool is_pre = has_identifier(*method, "pre");
      const clang::CXXMethodDecl *&selected = is_pre ? shape.pre : shape.post;
      if (selected) {
        error = std::string("Interceptor has an overloaded ") +
                (is_pre ? "pre" : "post") + " hook; exactly one is allowed";
        return false;
      }
      selected = method;
      const bool valid_return = method->getReturnType()->isVoidType() ||
                                is_long_long(method->getReturnType());
      if (method->isStatic() || method->getAccess() != clang::AS_public ||
          method->getNumParams() != 1 ||
          !is_call_reference(method->getParamDecl(0)->getType(), "tracer::Call",
                             false) ||
          !valid_return || !method->hasBody()) {
        error = std::string("Interceptor::") + (is_pre ? "pre" : "post") +
                " must be a public, defined instance method taking Call& or "
                "const Call& "
                "and returning void or long long";
        return false;
      }
    }
    shape.has_pre = shape.pre != nullptr;
    shape.has_post = shape.post != nullptr;
    if (!shape.has_pre && !shape.has_post) {
      error =
          "Interceptor must define at least one public pre() or post() hook";
      return false;
    }
    return true;
  }
  if (!legacy) {
    if (!saw_legacy && tracer::find_snapshot_encoders(context).requested) {
      shape.encoder_only = true;
      shape.has_pre = shape.has_post = true;
      return true;
    }
    error = "callback source must define Interceptor or trace_callback";
    return false;
  }
  shape.legacy = legacy;
  shape.has_pre = true;
  if (legacy->getNumParams() != 1 || !is_long_long(legacy->getReturnType()) ||
      !is_call_reference(legacy->getParamDecl(0)->getType(),
                         "tracer::TraceContext")) {
    error = "trace_callback must be defined as long long(const TraceContext&)";
    return false;
  }
  return true;
}

bool validate_snapshot_encoder(const CallbackShape &shape,
                               const tracer::ValueSnapshotCode &snapshots,
                               std::string &error) {
  if (!snapshots.error.empty()) {
    error = snapshots.error;
    return false;
  }
  if (snapshots.has_custom_encoder && !shape.has_post) {
    error = "tracer_snapshot::encode requires a POST hook when combined with an Interceptor";
    return false;
  }
  return true;
}

// Whether `actual` is the type DWARF spells for an enumeration: `[const ]E[ volatile]`, then
// pointer levels (each with its own `const`/`volatile`) and at most one reference. The enum must
// be the one DWARF names and have the underlying size and signedness DWARF recorded.
bool debug_enum_type_matches(clang::ASTContext &context, clang::QualType actual,
                             const std::string &spelling,
                             const clangtracer::v1::DebugTemplateType &enumeration,
                             bool is_reference) {
  struct Level {
    bool is_const = false, is_volatile = false;
  };
  std::string rest = spelling;
  std::vector<Level> levels(1);
  if (::tracer::compat::starts_with(rest, "const ")) {
    levels[0].is_const = true;
    rest.erase(0, 6);
  }
  if (!::tracer::compat::starts_with(rest, enumeration.name()))
    return false;
  rest.erase(0, enumeration.name().size());
  bool lvalue_reference = false, rvalue_reference = false;
  for (std::size_t i = 0; i < rest.size();) {
    if (rest[i] == ' ') {
      ++i;
    } else if (lvalue_reference || rvalue_reference) {
      return false;  // Nothing follows a reference.
    } else if (rest[i] == '*') {
      levels.emplace_back();
      ++i;
    } else if (rest[i] == '&') {
      if (i + 1 < rest.size() && rest[i + 1] == '&') {
        rvalue_reference = true;
        i += 2;
      } else {
        lvalue_reference = true;
        ++i;
      }
    } else if (::tracer::compat::starts_with(std::string_view(rest).substr(i), "const")) {
      levels.back().is_const = true;
      i += 5;
    } else if (::tracer::compat::starts_with(std::string_view(rest).substr(i), "volatile")) {
      levels.back().is_volatile = true;
      i += 8;
    } else {
      return false;
    }
  }
  if (is_reference != (lvalue_reference || rvalue_reference))
    return false;

  clang::QualType actual_base = actual;
  if (const auto *reference = actual_base->getAs<clang::ReferenceType>()) {
    if (actual_base->isRValueReferenceType() != rvalue_reference)
      return false;
    actual_base = reference->getPointeeType();
  }
  for (std::size_t i = 1; i < levels.size(); ++i) {
    const auto *pointer = actual_base->getAs<clang::PointerType>();
    if (!pointer)
      return false;
    actual_base = pointer->getPointeeType();
  }
  const auto *enum_type = actual_base->getAs<clang::EnumType>();
  if (!enum_type)
    return false;
  const auto *enum_decl = enum_type->getDecl();
  // A standard-library enumeration is named without its inline ABI namespace by Clang and with
  // it by libc++'s DWARF (std::__1::errc).
  if (normalized_cpp_type(declared_name(enum_decl->getQualifiedNameAsString())) !=
      normalized_cpp_type(declared_name(enumeration.name())))
    return false;
  const auto integer = enum_decl->getIntegerType();
  if (integer.isNull() || !integer->isIntegerType() ||
      context.getTypeSize(integer) != enumeration.byte_size() * 8)
    return false;
  if (enumeration.has_element() && enumeration.element().kind() ==
                                       clangtracer::v1::DebugTemplateType::BUILTIN) {
    const auto encoding = enumeration.element().dwarf_encoding();
    if (integer->isSignedIntegerType() != (encoding == 0x05 || encoding == 0x06))
      return false;
  }

  clang::QualType expected = actual_base.getUnqualifiedType();
  const auto qualify = [&](clang::QualType type, const Level &level) {
    if (level.is_const)
      type.addConst();
    if (level.is_volatile)
      type.addVolatile();
    return type;
  };
  expected = qualify(expected, levels[0]);
  for (std::size_t i = 1; i < levels.size(); ++i)
    expected = qualify(context.getPointerType(expected), levels[i]);
  if (rvalue_reference)
    expected = context.getRValueReferenceType(expected);
  else if (lvalue_reference)
    expected = context.getLValueReferenceType(expected);
  else {
    // Function types discard top-level cv from by-value parameters, as for builtins.
    expected = expected.getUnqualifiedType();
    actual = actual.getUnqualifiedType();
  }
  return context.hasSameType(actual.getCanonicalType(), expected.getCanonicalType());
}

bool debug_type_matches(clang::ASTContext &context, clang::QualType actual,
                        const std::string &spelling,
                        const clangtracer::v1::DebugInfo *object,
                        bool is_reference, bool object_value,
                        const clangtracer::v1::DebugTemplateType *enumeration = nullptr) {
  if (enumeration && !(object && !object->record_name().empty()))
    return debug_enum_type_matches(context, actual, spelling, *enumeration, is_reference);
  clang::QualType expected;
  bool record_pointer = false;
  if (object && !object->record_name().empty()) {
    std::string base = spelling;
    const bool rvalue_reference = ::tracer::compat::ends_with(base, "&&");
    const bool lvalue_reference =
        !rvalue_reference && ::tracer::compat::ends_with(base, "&");
    if (rvalue_reference)
      base.resize(base.size() - 2);
    else if (lvalue_reference)
      base.resize(base.size() - 1);
    if (is_reference != (rvalue_reference || lvalue_reference))
      return false;
    if (!is_reference && ::tracer::compat::contains(base, "*") &&
        ::tracer::compat::ends_with(base, " const"))
      base.resize(base.size() - 6);
    if (!is_reference && ::tracer::compat::contains(base, "*") &&
        ::tracer::compat::ends_with(base, " volatile"))
      base.resize(base.size() - 9);

    clang::QualType actual_base = actual;
    if (const auto *reference = actual_base->getAs<clang::ReferenceType>()) {
      if (actual_base->isRValueReferenceType() != rvalue_reference)
        return false;
      actual_base = reference->getPointeeType();
    }
    record_pointer = ::tracer::compat::ends_with(base, "*");
    if (record_pointer) {
      base.resize(base.size() - 1);
      while (!base.empty() && std::isspace(static_cast<unsigned char>(base.back())))
        base.pop_back();
      // Top-level cv on a by-value pointer is not part of its function type.
      if (::tracer::compat::ends_with(base, " const"))
        base.resize(base.size() - 6);
      if (::tracer::compat::ends_with(base, " volatile"))
        base.resize(base.size() - 9);
      const auto *pointer_type = actual_base->getAs<clang::PointerType>();
      if (!pointer_type)
        return false;
      actual_base = pointer_type->getPointeeType();
    }
    bool expected_const =
        ::tracer::compat::starts_with(base, "const ") ||
        ::tracer::compat::contains(base, " const");
    bool expected_volatile =
        ::tracer::compat::starts_with(base, "volatile ") ||
        ::tracer::compat::contains(base, " volatile");
    if (::tracer::compat::starts_with(base, "const "))
      base.erase(0, 6);
    if (::tracer::compat::starts_with(base, "volatile "))
      base.erase(0, 9);
    if (::tracer::compat::ends_with(base, " const"))
      base.resize(base.size() - 6);
    if (::tracer::compat::ends_with(base, " volatile"))
      base.resize(base.size() - 9);
    // Function types discard top-level cv from by-value parameters, including
    // records. Keep qualifiers on references and pointer pointees intact.
    if (object_value && !is_reference && !record_pointer) {
      expected_const = false;
      expected_volatile = false;
    }
    const auto *record = actual_base->getAsCXXRecordDecl();
    if (!record)
      return false;
    const auto expected_identity = normalized_cpp_type(object->record_name());
    if (!debug_record_identity_matches(context, record, *object))
      return false;
    if (normalized_cpp_type(base) != expected_identity &&
        normalized_cpp_type(base) !=
            normalized_cpp_type(record->getQualifiedNameAsString()))
      return false;
    expected = actual_base.getUnqualifiedType();
    if (expected_const)
      expected.addConst();
    if (expected_volatile)
      expected.addVolatile();
    if (record_pointer)
      expected = context.getPointerType(expected);
    if (rvalue_reference)
      expected = context.getRValueReferenceType(expected);
    else if (lvalue_reference)
      expected = context.getLValueReferenceType(expected);
  } else {
    std::string base = spelling;
    const bool rvalue_reference =
        ::tracer::compat::ends_with(base, "&&");
    const bool lvalue_reference =
        !rvalue_reference && ::tracer::compat::ends_with(base, "&");
    if (rvalue_reference)
      base.resize(base.size() - 2);
    else if (lvalue_reference)
      base.resize(base.size() - 1);
    try {
      expected = debug_builtin(context, base);
    } catch (const std::exception &) {
      return false;
    }
    if (is_reference != (rvalue_reference || lvalue_reference))
      return false;
    if (rvalue_reference)
      expected = context.getRValueReferenceType(expected);
    else if (lvalue_reference)
      expected = context.getLValueReferenceType(expected);
    else
      expected = expected.getUnqualifiedType();
  }
  if (object_value && (actual->isReferenceType() || actual->isPointerType()))
    return false;
  return context.hasSameType(actual.getCanonicalType(),
                             expected.getCanonicalType());
}

// Why an enumeration the target signature names is not declared for the callback, or empty
// when every one is. Without it `tracer::ResolvedSignature` cannot be formed, and the callback
// only sees that its signature is not a function type.
std::string undeclared_enum_diagnostic(clang::ASTContext &context,
                                       const clangtracer::v1::ProbeSpec &spec) {
  const auto enums = callable_enums(spec, true);
  if (enums.empty())
    return {};
  const auto declared = declared_names(context);
  for (const auto *type : enums) {
    if (declared.enums.count(declared_name(type->name())))
      continue;
    std::string remedy;
    if (::tracer::standard_headers::standard_library_scope(type->name())) {
      std::vector<::tracer::standard_headers::Entry> user_entries;
      std::string ignored, key;
      parse_standard_header_entries(spec, user_entries, ignored);
      if (const auto *entry =
              ::tracer::standard_headers::find_entry(type->name(), user_entries, key)) {
        auto line = ::tracer::standard_headers::include_line(entry->header);
        line.pop_back();  // The newline.
        remedy = "add " + line + " to the callback";
      } else
        remedy = "supply its header with --include or an #include in the callback, or map it "
                 "with --std-header " + key + "=<header>";
    } else {
      remedy = "supply its header with --include or an #include in the callback";
    }
    return "enumeration " + type->name() + " named by the target signature is not declared for "
           "the callback: " + remedy;
  }
  return {};
}

bool validate_specialized_signature(
    clang::ASTContext &context, const CallbackShape &shape,
    const clangtracer::v1::ProbeSpec &spec, std::string &error) {
  if (!shape.signature_specialization)
    return true;
  if (!spec.has_debug_info()) {
    error = "Interceptor<Signature> requires a DWARF-resolved target";
    return false;
  }
  const auto *function =
      shape.specialized_signature->getAs<clang::FunctionProtoType>();
  if (!function || function->isVariadic()) {
    // A signature that names an enumeration the callback does not declare is no type at all.
    error = undeclared_enum_diagnostic(context, spec);
    if (error.empty())
      error = "Interceptor template argument must be a function signature";
    return false;
  }
  // Prepared aliases retain the exact Clang type. The generated
  // static_assert compares the complete signature in the same TU.
  if (tracer::prepared::is_typed_version(spec.prepared().format_version()))
    return true;
  const auto &info = spec.debug_info();
  if (function->getNumParams() != info.arguments_size()) {
    error = "Interceptor signature argument count does not match target";
    return false;
  }
  const auto has_multilevel_record_pointer = [](const std::string &type,
                                                 bool has_record) {
    return has_record && record_pointer_levels(type) > 1;
  };
  const auto &result_info = info;
  if (has_multilevel_record_pointer(
          result_info.result_cpp_type(), result_info.has_result_object_type())) {
    error = "multi-level pointers to recovered record results are not "
            "supported by Interceptor signature validation";
    return false;
  }
  if (!debug_type_matches(context, function->getReturnType(),
                          result_info.result_cpp_type(),
                          result_info.has_result_object_type()
                              ? &result_info.result_object_type()
                              : nullptr,
                          result_info.result_is_reference(),
                          result_info.result_is_object_value(),
                          result_info.has_result_enum_type()
                              ? &result_info.result_enum_type()
                              : nullptr)) {
    error = "Interceptor signature result type does not match target DWARF";
    return false;
  }
  for (unsigned i = 0; i < function->getNumParams(); ++i) {
    const auto &argument = info.arguments(static_cast<int>(i));
    if (has_multilevel_record_pointer(argument.cpp_type(),
                                      argument.has_object_type())) {
      error = "multi-level pointers to recovered record argument " +
              std::to_string(i) +
              " are not supported by Interceptor signature validation";
      return false;
    }
    if (!debug_type_matches(
            context, function->getParamType(i), argument.cpp_type(),
            argument.has_object_type() ? &argument.object_type() : nullptr,
            argument.is_reference(), argument.is_object_value(),
            argument.has_enum_type() ? &argument.enum_type() : nullptr)) {
      error = "Interceptor signature argument " + std::to_string(i) +
              " type does not match target DWARF (callback: " +
              function->getParamType(i).getAsString() + "; target: " +
              argument.cpp_type() + ")";
      return false;
    }
  }
  return true;
}

clang::TypeAliasDecl *
create_receiver_alias(clang::ASTContext &context,
                      const clang::CXXRecordDecl *imported_record,
                      const clangtracer::v1::ReceiverContext &receiver) {
  auto &tracer_identifier = context.Idents.get("tracer");
  clang::NamespaceDecl *tracer_namespace = nullptr;
  for (clang::NamedDecl *named :
       context.getTranslationUnitDecl()->lookup(&tracer_identifier)) {
    if ((tracer_namespace = llvm::dyn_cast<clang::NamespaceDecl>(named))) {
      break;
    }
  }
  if (!tracer_namespace) {
    tracer_namespace = clang::NamespaceDecl::Create(
        context, context.getTranslationUnitDecl(), false,
        clang::SourceLocation(), clang::SourceLocation(), &tracer_identifier,
        nullptr, false);
    context.getTranslationUnitDecl()->addDecl(tracer_namespace);
  }

  auto &alias_identifier = context.Idents.get("ResolvedTarget");
  for (clang::NamedDecl *named : tracer_namespace->lookup(&alias_identifier)) {
    if (auto *existing = llvm::dyn_cast<clang::TypeAliasDecl>(named)) {
      return existing;
    }
  }
  clang::QualType receiver_type = context.getTypeDeclType(
      static_cast<const clang::TypeDecl *>(imported_record));
  receiver_type.addConst();
  if (receiver.is_volatile()) {
    receiver_type.addVolatile();
  }
  auto *type_info = context.CreateTypeSourceInfo(receiver_type);
  auto *alias = clang::TypeAliasDecl::Create(
      context, tracer_namespace, clang::SourceLocation(),
      clang::SourceLocation(), &alias_identifier, type_info);
  tracer_namespace->addDecl(alias);
  return alias;
}

struct ReceiverCallbackShape {
  bool valid = false;
  bool class_mode = false;
  bool has_pre = false;
  bool has_post = false;
  bool signature_specialization = false;
  std::string interceptor_type = "Interceptor";
  clang::QualType specialized_signature;
  std::string bridge;
  std::string error;
  std::vector<std::pair<unsigned, std::string>> inferred_fields;
  std::vector<std::string> argument_names;
  tracer::ValueSnapshotCode snapshots;
  // The same capture without followed members, for when that code does not compile.
  tracer::ValueSnapshotCode snapshots_plain;
};

std::string make_class_bridge(const CallbackShape &shape);

std::string make_invocation_bridge(const CallbackShape &shape) {
  if (shape.class_mode)
    return make_class_bridge(shape);
  if (shape.encoder_only)
    return R"CPP(
extern "C" long long tracer_user_dispatch(const tracer::TraceContext *,
                                          void *, bool *ran) {
  *ran = true;
  return 0;
}
)CPP";
  return tracer::make_legacy_callback_bridge();
}

class ReceiverShapeConsumer final : public clang::ASTConsumer {
public:
  ReceiverShapeConsumer(ReceiverCallbackShape &result,
                        const clangtracer::v1::ProbeSpec &spec)
      : result_(result), spec_(spec) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    CallbackShape shape;
    if (!inspect_callback_shape(context, shape, result_.error)) {
      return;
    }
    if (!validate_specialized_signature(context, shape, spec_, result_.error))
      return;
    result_.snapshots = tracer::make_value_snapshot_code(context, spec_);
    if (!validate_snapshot_encoder(shape, result_.snapshots, result_.error))
      return;
    if (result_.snapshots.has_followed_members)
      result_.snapshots_plain = tracer::make_value_snapshot_code(context, spec_, false);
    class Fields : public clang::RecursiveASTVisitor<Fields> {
    public:
      Fields(clang::ASTContext &c, ReceiverCallbackShape &r)
          : context(c), result(r) {}
      int origin(clang::Expr *e, unsigned depth = 0) {
        if (!e || depth > 16)
          return -1;
        e = e->IgnoreParenImpCasts();
        if (auto *ref = llvm::dyn_cast<clang::DeclRefExpr>(e)) {
          if (auto *var = llvm::dyn_cast<clang::VarDecl>(ref->getDecl()))
            return origin(var->getInit(), depth + 1);
        }
        if (auto *clean = llvm::dyn_cast<clang::ExprWithCleanups>(e))
          return origin(clean->getSubExpr(), depth + 1);
        if (auto *call = llvm::dyn_cast<clang::CXXMemberCallExpr>(e)) {
          auto *method = call->getMethodDecl();
          if (method && method->getNameAsString() == "argument" &&
              method->getParent()->getQualifiedNameAsString() ==
                  "tracer::Call" &&
              call->getNumArgs() == 0) {
            if (const auto *args = method->getTemplateSpecializationArgs();
                args && args->size() == 1 &&
                args->get(0).getKind() == clang::TemplateArgument::Integral)
              return args->get(0).getAsIntegral().getLimitedValue(8);
          }
        }
        return -1;
      }
      bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *call) {
        const auto *method = call->getMethodDecl();
        if (!method || method->getNameAsString() != "field" ||
            method->getParent()->getQualifiedNameAsString() != "tracer::Call" ||
            call->getNumArgs() != 1)
          return true;
        auto *member = llvm::dyn_cast<clang::MemberExpr>(
            call->getArg(0)->IgnoreParenImpCasts());
        auto loc = call->getArg(0)->getBeginLoc();
        auto *local = llvm::dyn_cast<clang::DeclRefExpr>(
            call->getArg(0)->IgnoreParenImpCasts());
        if ((!member ||
             !llvm::isa<clang::FieldDecl>(member->getMemberDecl())) &&
            !local) {
          result.error = "cannot infer value name: use a member/local variable "
                         "or field(name, value)";
          return false;
        }
        if (loc.isMacroID() ||
            !context.getSourceManager().isWrittenInMainFile(loc)) {
          result.error = "cannot infer value names inside macros or headers";
          return false;
        }
        std::string label = member ? member->getMemberDecl()->getNameAsString()
                                   : local->getDecl()->getNameAsString();
        const int index = origin(member ? member->getBase() : local);
        if (index >= 0 &&
            static_cast<std::size_t>(index) < result.argument_names.size())
          label = member ? result.argument_names[index] + "." + label
                         : result.argument_names[index];
        result.inferred_fields.emplace_back(
            context.getSourceManager().getFileOffset(loc), label);
        return true;
      }
      clang::ASTContext &context;
      ReceiverCallbackShape &result;
    } fields(context, result_);
    fields.TraverseDecl(context.getTranslationUnitDecl());
    if (!result_.error.empty())
      return;
    result_.class_mode = shape.class_mode;
    result_.has_pre = shape.has_pre;
    result_.has_post = shape.has_post;
    result_.signature_specialization = shape.signature_specialization;
    result_.interceptor_type = shape.interceptor_type;
    result_.bridge = make_invocation_bridge(shape);
    result_.valid = true;
  }

private:
  ReceiverCallbackShape &result_;
  const clangtracer::v1::ProbeSpec &spec_;
};

class ReceiverASTImporter : public clang::ASTImporter {
public:
  using clang::ASTImporter::ASTImporter;

protected:
  llvm::Expected<clang::Decl *> ImportImpl(clang::Decl *from) override {
    if (auto *function = llvm::dyn_cast<clang::FunctionDecl>(from);
        function && !function->isDefaulted() && !function->isDeleted() &&
        !function->isLateTemplateParsed()) {
      const clang::FunctionDecl *definition = nullptr;
      clang::Stmt *body = function->getBody(definition);
      // Never move a body from a sibling redeclaration onto this declaration.
      if (body && definition == function &&
          function->doesThisDeclarationHaveABody()) {
        class BodyGuard {
        public:
          BodyGuard(clang::FunctionDecl &function, clang::Stmt *body)
              : function_(function), body_(body),
                constructor_(
                    llvm::dyn_cast<clang::CXXConstructorDecl>(&function)),
                initializer_count_(
                    constructor_ ? constructor_->getNumCtorInitializers() : 0) {
            function_.setBody(nullptr);
            if (constructor_) {
              constructor_->setNumCtorInitializers(0);
            }
          }

          ~BodyGuard() {
            if (constructor_) {
              constructor_->setNumCtorInitializers(initializer_count_);
            }
            function_.setBody(body_);
          }

        private:
          clang::FunctionDecl &function_;
          clang::Stmt *body_;
          clang::CXXConstructorDecl *constructor_;
          unsigned initializer_count_;
        } guard(*function, body);

        return clang::ASTImporter::ImportImpl(from);
      }
    }
    if (auto *field = llvm::dyn_cast<clang::FieldDecl>(from)) {
      if (clang::Expr *initializer = field->getInClassInitializer()) {
        class InitializerGuard {
        public:
          InitializerGuard(clang::FieldDecl &field, clang::Expr *initializer)
              : field_(field), initializer_(initializer) {
            field_.setInClassInitializer(nullptr);
          }

          ~InitializerGuard() { field_.setInClassInitializer(initializer_); }

        private:
          clang::FieldDecl &field_;
          clang::Expr *initializer_;
        } guard(*field, initializer);

        return clang::ASTImporter::ImportImpl(from);
      }
    }
    return clang::ASTImporter::ImportImpl(from);
  }
};

class TrackingASTImporter final : public ReceiverASTImporter {
public:
  using ReceiverASTImporter::ReceiverASTImporter;

  void Imported(clang::Decl *, clang::Decl *imported) override {
    if (imported) {
      declarations.insert(imported->getCanonicalDecl());
    }
  }

  std::unordered_set<const clang::Decl *> declarations;
};

clang::CXXRecordDecl *find_callback_record(clang::ASTContext &context,
                                           llvm::StringRef name) {
  class Finder final : public clang::RecursiveASTVisitor<Finder> {
  public:
    Finder(clang::ASTContext &context, llvm::StringRef name)
        : context_(context), name_(name) {}
    bool shouldVisitTemplateInstantiations() const { return true; }
    bool VisitCXXRecordDecl(clang::CXXRecordDecl *record) {
      if (record->isCompleteDefinition() && !record->isDependentType() &&
          debug_record_identity_matches(context_, record, name_))
        found = record;
      return true;
    }
    clang::CXXRecordDecl *found = nullptr;

  private:
    clang::ASTContext &context_;
    llvm::StringRef name_;
  } finder(context, name);
  finder.TraverseDecl(context.getTranslationUnitDecl());
  return finder.found;
}

class ReceiverImportAction final : public clang::ASTFrontendAction {
public:
  ReceiverImportAction(clang::ASTContext &donor_context,
                       clang::FileManager &donor_files,
                       clang::CXXRecordDecl *donor_record,
                       ReceiverCallbackShape &shape,
                       const clangtracer::v1::ReceiverContext *receiver_context,
                       clang::CXXRecordDecl *target_record,
                       std::string *validation_error,
                       const DebugRecords *records = nullptr,
                       const clangtracer::v1::ProbeSpec *spec = nullptr,
                       bool header_types = false)
      : records_(records), donor_context_(donor_context),
        donor_files_(donor_files), donor_record_(donor_record), shape_(shape),
        receiver_context_(receiver_context), target_record_(target_record),
        validation_error_(validation_error), spec_(spec),
        header_types_(header_types) {}

  bool BeginSourceFileAction(clang::CompilerInstance &compiler) override {
    if (!clang::ASTFrontendAction::BeginSourceFileAction(compiler)) {
      return false;
    }
    diagnostic_capture_ =
        std::make_unique<DiagnosticCaptureScope>(compiler.getDiagnostics());
    return true;
  }

  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &, llvm::StringRef) override {
    if (!spec_) {
      *validation_error_ = "signature metadata is unavailable during callback validation";
      return std::make_unique<clang::ASTConsumer>();
    }
    return std::make_unique<ReceiverShapeConsumer>(shape_, *spec_);
  }

  void ExecuteAction() override {
    auto &compiler = getCompilerInstance();
    compiler.createSema(getTranslationUnitKind(), nullptr);
    TrackingASTImporter importer(compiler.getASTContext(),
                                 compiler.getFileManager(), donor_context_,
                                 donor_files_, false);
    std::vector<clang::CXXRecordDecl *> imported_targets;
    clang::CXXRecordDecl *selected_imported_record = nullptr;
    if (records_)
      for (const auto &entry : *records_) {
        // Real header definitions are parsed below. Import only synthesized
        // DWARF records here, otherwise the later #include redefines them.
        if (header_types_ && debug_record_from_header(entry.decl))
          continue;
        if (auto err = importer.ImportDefinition(entry.decl)) {
          *validation_error_ = llvm::toString(std::move(err));
          return;
        }
        auto imported = importer.Import(entry.decl);
        if (!imported) {
          *validation_error_ = llvm::toString(imported.takeError());
          return;
        }
        auto *target = llvm::cast<clang::CXXRecordDecl>(*imported);
        if (!entry.opaque &&
            !validate_receiver_layout(compiler.getASTContext(), target,
                                      entry.layout, *validation_error_))
          return;
        imported_targets.push_back(target);
      }
    if (donor_record_ &&
        !(header_types_ && debug_record_from_header(donor_record_))) {
      if (llvm::Error import_error = importer.ImportDefinition(donor_record_)) {
        *validation_error_ = llvm::toString(std::move(import_error));
        return;
      }
      auto imported_decl = importer.Import(donor_record_);
      if (!imported_decl) {
        *validation_error_ = llvm::toString(imported_decl.takeError());
        return;
      }
      auto *imported_record_const =
          llvm::dyn_cast<clang::CXXRecordDecl>(*imported_decl);
      auto *imported_record =
          const_cast<clang::CXXRecordDecl *>(imported_record_const);
      selected_imported_record = imported_record;
      if (!imported_record || !receiver_context_ ||
          !validate_receiver_layout(compiler.getASTContext(), imported_record,
                                    *receiver_context_, *validation_error_)) {
        if (validation_error_->empty()) {
          *validation_error_ =
              "receiver declaration did not survive AST import";
        } else {
          *validation_error_ =
              "preflight receiver layout mismatch: " + *validation_error_;
        }
        return;
      }
      if (!create_receiver_alias(compiler.getASTContext(), imported_record,
                                 *receiver_context_)) {
        *validation_error_ =
            "cannot create typed receiver alias in preflight AST";
        return;
      }
    }
    clang::ParseAST(compiler.getSema());
    if (diagnostic_capture_ && diagnostic_capture_->had_error()) {
      *validation_error_ = diagnostic_capture_->message();
      if (header_types_)
        return;
    }
    if (header_types_ && records_)
      for (const auto &entry : *records_) {
        if (!debug_record_from_header(entry.decl))
          continue;
        auto &context = compiler.getASTContext();
        auto *record =
            find_callback_record(context, entry.layout.record_name());
        if (!record) {
          *validation_error_ = "callback header record is unavailable: " +
                               entry.layout.record_name();
          return;
        }
        if (entry.opaque) {
          const auto &layout = context.getASTRecordLayout(record);
          if ((entry.layout.size() &&
               static_cast<std::uint64_t>(layout.getSize().getQuantity()) !=
                   entry.layout.size()) ||
              (entry.layout.alignment() &&
               static_cast<std::uint64_t>(
                   layout.getAlignment().getQuantity()) !=
                   entry.layout.alignment())) {
            *validation_error_ =
                "callback header record size or alignment changed";
            return;
          }
        } else if (!validate_receiver_layout(context, record, entry.layout,
                                             *validation_error_)) {
          return;
        }
        imported_targets.push_back(record);
        if (donor_record_ &&
            entry.decl->getCanonicalDecl() == donor_record_->getCanonicalDecl())
          selected_imported_record = record;
      }
    if (target_record_ && donor_record_ && selected_imported_record) {
      // Skip imported receiver method bodies while checking callback and
      // included helper code for ODR uses of the selected target.
      visitor_ = std::make_unique<ForbiddenReceiverUseVisitor>(
          selected_imported_record, importer.declarations, imported_targets,
          spec_->symbol());
    }
    if (validation_error_->empty() && !shape_.error.empty()) {
      *validation_error_ = shape_.error;
    }
    if (visitor_) {
      visitor_->TraverseDecl(compiler.getASTContext().getTranslationUnitDecl());
      if (!visitor_->error().empty()) {
        *validation_error_ = visitor_->error();
      }
    }
  }

  static std::string validate_probed_method_uses(
      clang::ASTContext &context, const std::string &record_name,
      const std::string &symbol) {
    auto *record = find_callback_record(context, record_name);
    if (!record)
      return {};
    const std::unordered_set<const clang::Decl *> imported;
    const std::vector<clang::CXXRecordDecl *> targets;
    ForbiddenReceiverUseVisitor visitor(record, imported, targets, symbol, true);
    visitor.TraverseDecl(context.getTranslationUnitDecl());
    return visitor.error();
  }

private:
  class ForbiddenReceiverUseVisitor final
      : public clang::RecursiveASTVisitor<ForbiddenReceiverUseVisitor> {
  public:
    ForbiddenReceiverUseVisitor(
        clang::CXXRecordDecl *target,
        const std::unordered_set<const clang::Decl *> &imported,
        const std::vector<clang::CXXRecordDecl *> &targets,
        std::string target_symbol, bool methods_only = false)
        : source_manager_(target->getASTContext().getSourceManager()),
          imported_(imported), target_symbol_(std::move(target_symbol)),
          methods_only_(methods_only) {
      targets_.insert(target->getCanonicalDecl());
      for (auto *t : targets)
        targets_.insert(t->getCanonicalDecl());
      const auto location =
          source_manager_.getExpansionLoc(target->getLocation());
      if (location.isValid() && !source_manager_.isWrittenInMainFile(location))
        receiver_header_file_ = source_manager_.getFileID(location);
    }
    bool is_target(const clang::CXXRecordDecl *record) const {
      return record && targets_.count(record->getCanonicalDecl());
    }

    bool is_guarded_object(const clang::CXXRecordDecl *record) const {
      return !methods_only_ && is_target(record);
    }

    bool is_probed_method(const clang::CXXMethodDecl *method) const {
      return method && !llvm::isa<clang::CXXConstructorDecl>(method) &&
             !llvm::isa<clang::CXXDestructorDecl>(method) &&
             !method->isDependentContext() &&
             !method->getType()->isDependentType() &&
             mangle_function(method, method->getASTContext()) == target_symbol_;
    }

    bool shouldVisitTemplateInstantiations() const { return true; }

    bool shouldVisitImplicitCode() const { return true; }

    bool TraverseDecl(clang::Decl *declaration) {
      // Other classes and free functions in the receiver header are also
      // application code, not callback helpers. Keep checking callback code
      // and helpers from other files, including their direct method references.
      if (declaration && receiver_header_file_.isValid()) {
        const auto location =
            source_manager_.getExpansionLoc(declaration->getLocation());
        if (location.isValid() &&
            source_manager_.getFileID(location) == receiver_header_file_)
          return true;
      }
      // Out-of-class definitions belong to the receiver implementation too.
      // Calls from these bodies run under the existing callback-depth guard.
      if (const auto *method =
              llvm::dyn_cast_or_null<clang::CXXMethodDecl>(declaration);
          method && is_target(method->getParent()))
        return true;
      return clang::RecursiveASTVisitor<
          ForbiddenReceiverUseVisitor>::TraverseDecl(declaration);
    }

    bool TraverseCXXRecordDecl(clang::CXXRecordDecl *record) {
      if (record && is_target(record)) {
        return true;
      }
      return clang::RecursiveASTVisitor<
          ForbiddenReceiverUseVisitor>::TraverseCXXRecordDecl(record);
    }

    bool TraverseFunctionDecl(clang::FunctionDecl *function) {
      if (function && function->getNameAsString() == "clangtracer_wrapper")
        return true;
      if (function && function->doesThisDeclarationHaveABody() &&
          imported_.count(function->getCanonicalDecl()) != 0) {
        return true;
      }
      return clang::RecursiveASTVisitor<
          ForbiddenReceiverUseVisitor>::TraverseFunctionDecl(function);
    }

    bool TraverseVarDecl(clang::VarDecl *variable) {
      if (variable && variable->hasGlobalStorage() && variable->hasInit() &&
          imported_.count(variable->getCanonicalDecl()) != 0) {
        return true;
      }
      return clang::RecursiveASTVisitor<
          ForbiddenReceiverUseVisitor>::TraverseVarDecl(variable);
    }

    bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *expression) {
      if (const auto *member =
              llvm::dyn_cast<clang::MemberExpr>(expression->getCallee())) {
        if (const auto *method =
                llvm::dyn_cast<clang::CXXMethodDecl>(member->getMemberDecl());
            is_probed_method(method)) {
          reject_probed_method(expression);
        }
      }
      return true;
    }

    bool VisitMemberExpr(clang::MemberExpr *expression) {
      if (const auto *method =
              llvm::dyn_cast<clang::CXXMethodDecl>(expression->getMemberDecl());
          is_probed_method(method)) {
        reject_probed_method(expression);
      }
      return true;
    }

    bool VisitDeclRefExpr(clang::DeclRefExpr *expression) {
      if (imported_.count(expression->getDecl()->getCanonicalDecl()) != 0) {
        if (const auto *function =
                llvm::dyn_cast<clang::FunctionDecl>(expression->getDecl());
            function && function->getDefinition()) {
          if (!llvm::isa<clang::CXXMethodDecl>(function))
            error_ = "callback cannot call or take the address of imported "
                     "target functions";
        } else if (const auto *variable =
                       llvm::dyn_cast<clang::VarDecl>(expression->getDecl());
                   variable && variable->hasGlobalStorage()) {
          error_ = "callback cannot odr-use imported target globals";
        }
      }
      if (const auto *method =
              llvm::dyn_cast<clang::CXXMethodDecl>(expression->getDecl());
          is_probed_method(method)) {
        reject_probed_method(expression);
      } else if (const auto *variable =
                     llvm::dyn_cast<clang::VarDecl>(expression->getDecl());
                 variable && variable->getDeclContext()->isRecord() &&
                 is_guarded_object(llvm::dyn_cast<clang::CXXRecordDecl>(
                     variable->getDeclContext()))) {
        error_ =
            "callback cannot odr-use imported receiver static data members";
      }
      return true;
    }

    bool VisitCXXConstructExpr(clang::CXXConstructExpr *expression) {
      if (auto *record = expression->getType()->getAsCXXRecordDecl();
          record && is_guarded_object(record)) {
        error_ =
            "callback cannot construct objects of the imported receiver type";
      }
      return true;
    }

    bool VisitInitListExpr(clang::InitListExpr *expression) {
      if (auto *record = expression->getType()->getAsCXXRecordDecl();
          record && is_guarded_object(record)) {
        error_ =
            "callback cannot construct objects of the imported receiver type";
      }
      return true;
    }

    bool VisitCXXNewExpr(clang::CXXNewExpr *expression) {
      if (auto *record = expression->getAllocatedType()->getAsCXXRecordDecl();
          record && is_guarded_object(record)) {
        error_ =
            "callback cannot allocate objects of the imported receiver type";
      }
      return true;
    }

    bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *expression) {
      const auto argument_type = expression->getArgument()->getType();
      if (argument_type->isPointerType()) {
        if (auto *record =
                argument_type->getPointeeType()->getAsCXXRecordDecl();
            record && is_guarded_object(record)) {
          error_ =
              "callback cannot destroy objects of the imported receiver type";
        }
      }
      return true;
    }

    const std::string &error() const { return error_; }

  private:
    void reject_probed_method(const clang::Expr *expression) {
      if (!error_.empty())
        return;
      error_ = "callback cannot call or take the address of the probed method";
      const auto location = source_manager_.getPresumedLoc(
          source_manager_.getExpansionLoc(expression->getExprLoc()));
      if (location.isValid())
        error_ += " at " + std::string(location.getFilename()) + ":" +
                  std::to_string(location.getLine()) + ":" +
                  std::to_string(location.getColumn());
    }

    std::unordered_set<const clang::CXXRecordDecl *> targets_;
    clang::SourceManager &source_manager_;
    clang::FileID receiver_header_file_;
    const std::unordered_set<const clang::Decl *> &imported_;
    std::string target_symbol_;
    bool methods_only_;
    std::string error_;
  };

  const DebugRecords *records_;
  clang::ASTContext &donor_context_;
  clang::FileManager &donor_files_;
  clang::CXXRecordDecl *donor_record_;
  ReceiverCallbackShape &shape_;
  const clangtracer::v1::ReceiverContext *receiver_context_;
  clang::CXXRecordDecl *target_record_;
  std::string *validation_error_;
  const clangtracer::v1::ProbeSpec *spec_;
  bool header_types_;
  std::unique_ptr<ForbiddenReceiverUseVisitor> visitor_;
  std::unique_ptr<DiagnosticCaptureScope> diagnostic_capture_;
};

std::string make_class_bridge(const CallbackShape &shape) {
  return tracer::make_class_callback_bridge(
      {shape.interceptor_type, shape.has_pre,
       !shape.pre || shape.pre->getReturnType()->isVoidType(), shape.has_post,
       !shape.post || shape.post->getReturnType()->isVoidType()});
}

std::string argument_aliases(const clangtracer::v1::ProbeSpec &spec) {
  if (!spec.has_debug_info())
    return {};
  std::string code =
      "namespace tracer { template<unsigned long> struct ArgumentType; "
      "template<unsigned long> struct ResultType;\n";
  const bool typed = tracer::prepared::is_typed_version(
      spec.prepared().format_version());
  if (typed)
    code += "template<unsigned> struct PreparedArgument;\n"
            "template<unsigned N> using ResolvedArgument = typename PreparedArgument<N>::type;\n";
  for (const auto &argument : spec.debug_info().arguments()) {
    // Preserve the full DWARF pointer chain. Multi-level pointers to records
    // are rejected by signature validation until their CV chain is modeled.
    std::string type = "typename ReadOnlyObservationType<" +
                       argument.cpp_type() + ">::type";
    if (argument.is_object_value()) {
      type = "const " + argument.cpp_type() + "&";
    }
    if (typed)
      code += "template<> struct PreparedArgument<" +
              std::to_string(argument.index()) + "> { using type = " +
              argument.cpp_type() + "; };\n";
    code += "template<> struct ArgumentType<" +
            std::to_string(argument.index()) + "> { using type=" + type +
            "; static constexpr bool object_value=" +
            (argument.is_object_value() ? "true" : "false") + "; };\n";
  }
  const auto &info = spec.debug_info();
  if (!info.result_cpp_type().empty()) {
    std::string result_type = info.result_is_object_value()
                                  ? info.result_cpp_type()
                                  : "typename ReadOnlyObservationType<" +
                                        info.result_cpp_type() + ">::type";
    code += "template<> struct ResultType<0> { using type=" + result_type +
            "; static constexpr bool object_value=" +
            (info.result_is_object_value() ? "true" : "false") + "; };\n";
  }
  code += "using ResolvedResult=" + info.result_cpp_type() + ";\n";
  code += "using ResolvedSignature=ResolvedResult(";
  for (int i = 0; i < info.arguments_size(); ++i) {
    if (i)
      code += ",";
    code += info.arguments(i).cpp_type();
  }
  code += ");\n";
  return code + "}\n";
}

std::string specialized_signature_assert(clang::ASTContext &context,
                                         const CallbackShape &shape) {
  if (!shape.signature_specialization ||
      shape.specialized_signature_spelling.empty())
    return {};
  (void)context;
  return "static_assert(std::is_same_v<" +
         shape.specialized_signature_spelling +
         ", tracer::ResolvedSignature>, \"Interceptor signature does not "
         "match the resolved target signature\");\n";
}

std::string interceptor_template_prelude(const std::string &source) {
  const auto skip_trivia = [&](std::size_t &offset) {
    for (;;) {
      while (offset < source.size() &&
             std::isspace(static_cast<unsigned char>(source[offset])))
        ++offset;
      if (offset + 1 < source.size() && source[offset] == '/' &&
          source[offset + 1] == '/') {
        offset = source.find('\n', offset + 2);
        if (offset == std::string::npos)
          return;
        continue;
      }
      if (offset + 1 < source.size() && source[offset] == '/' &&
          source[offset + 1] == '*') {
        const auto end = source.find("*/", offset + 2);
        if (end == std::string::npos) {
          offset = source.size();
          return;
        }
        offset = end + 2;
        continue;
      }
      return;
    }
  };
  for (std::size_t offset = 0; offset < source.size();) {
    skip_trivia(offset);
    if (offset >= source.size())
      break;
    const char c = source[offset];
    std::size_t raw_prefix = 0;
    if (source.compare(offset, 4, "u8R\"") == 0)
      raw_prefix = 4;
    else if (source.compare(offset, 3, "uR\"") == 0 ||
             source.compare(offset, 3, "UR\"") == 0 ||
             source.compare(offset, 3, "LR\"") == 0)
      raw_prefix = 3;
    else if (source.compare(offset, 2, "R\"") == 0)
      raw_prefix = 2;
    if (raw_prefix) {
      const auto delimiter_begin = offset + raw_prefix;
      const auto open = source.find('(', delimiter_begin);
      if (open == std::string::npos) {
        offset = source.size();
        break;
      }
      const auto delimiter = source.substr(delimiter_begin, open - delimiter_begin);
      const auto close = source.find(")" + delimiter + "\"", open + 1);
      offset = close == std::string::npos ? source.size()
                                          : close + delimiter.size() + 2;
      continue;
    }
    if (c == '"' || c == '\'') {
      const char quote = c;
      ++offset;
      while (offset < source.size()) {
        if (source[offset] == '\\') {
          offset = std::min(source.size(), offset + 2);
          continue;
        }
        if (source[offset++] == quote)
          break;
      }
      continue;
    }
    if (!(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
      ++offset;
      continue;
    }
    const auto begin = offset++;
    while (offset < source.size() &&
           (std::isalnum(static_cast<unsigned char>(source[offset])) ||
            source[offset] == '_'))
      ++offset;
    if (source.compare(begin, offset - begin, "Interceptor") != 0)
      continue;
    auto next = offset;
    skip_trivia(next);
    if (next < source.size() && source[next] == '<')
      return "template<class Signature> class Interceptor;\n";
    offset = next;
  }
  return {};
}

std::pair<std::string, std::string>
split_callback_includes(const std::string &source) {
  std::string includes;
  std::size_t consumed = 0;
  for (;;) {
    std::size_t offset = consumed;
    while (offset < source.size()) {
      if (std::isspace(static_cast<unsigned char>(source[offset]))) {
        ++offset;
      } else if (offset + 1 < source.size() && source[offset] == '/' &&
                 source[offset + 1] == '/') {
        const auto end = source.find('\n', offset + 2);
        offset = end == std::string::npos ? source.size() : end + 1;
      } else if (offset + 1 < source.size() && source[offset] == '/' &&
                 source[offset + 1] == '*') {
        const auto end = source.find("*/", offset + 2);
        if (end == std::string::npos)
          break;
        offset = end + 2;
      } else {
        break;
      }
    }
    if (offset >= source.size())
      break;
    const auto line_end = source.find('\n', offset);
    const auto end = line_end == std::string::npos ? source.size() : line_end + 1;
    std::string_view line(source.data() + offset, end - offset);
    std::size_t first = 0;
    while (first < line.size() &&
           std::isspace(static_cast<unsigned char>(line[first])) &&
           line[first] != '\n' && line[first] != '\r')
      ++first;
    if (line.substr(first, 8) != "#include")
      break;
    includes.append(source.data() + consumed, end - consumed);
    consumed = end;
  }
  return {std::move(includes), source.substr(consumed)};
}

bool extract_forced_includes(std::vector<std::string> &arguments,
                             std::string &source, std::string &error,
                             const std::filesystem::path &directory = {}) {
  std::vector<std::string> filtered;
  filtered.reserve(arguments.size());
  for (std::size_t i = 0; i < arguments.size(); ++i) {
    const auto &argument = arguments[i];
    if (argument == "-imacros" ||
        ::tracer::compat::starts_with(argument, "-imacros")) {
      error = "-imacros is not supported by side-effect-free preparation";
      return false;
    }
    if (argument == "-include") {
      if (++i >= arguments.size()) {
        error = "-include requires a header path";
        return false;
      }
      auto path = arguments[i];
      // Compilers search from the compile command's directory for -include.
      // The JIT does not change the target process's working directory.
      if (!directory.empty() && std::filesystem::path(path).is_relative()) {
        const auto candidate = directory / path;
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec))
          path = candidate.string();
      }
      if (path.empty() || path.find_first_of("\"\r\n") != std::string::npos ||
          path.find('\0') != std::string::npos) {
        error = "-include has an invalid header path";
        return false;
      }
      source += "#include \"" + path + "\"\n";
      continue;
    }
    if (::tracer::compat::starts_with(argument, "-include")) {
      if (::tracer::compat::starts_with(argument, "-include-pch")) {
        error = "-include-pch is not supported by side-effect-free preparation";
        return false;
      }
      const auto path = ::tracer::compat::starts_with(argument, "-include=")
                            ? argument.substr(sizeof("-include=") - 1)
                            : argument.substr(sizeof("-include") - 1);
      if (path.empty() || path.find_first_of("\"\r\n") != std::string::npos ||
          path.find('\0') != std::string::npos) {
        error = "-include has an invalid header path";
        return false;
      }
      source += "#include \"" + path + "\"\n";
      continue;
    }
    filtered.push_back(argument);
  }
  arguments.swap(filtered);
  return true;
}

void resolve_callback_option_paths(std::vector<std::string> &arguments,
                                   const std::filesystem::path &directory) {
  // Called after preflight validation: -iprefix, -iwithprefix and -isysroot
  // are rejected there. Keep this aligned with the accepted path options.
  for (std::size_t i = 0; i < arguments.size(); ++i) {
    for (const llvm::StringRef option : {"-isystem-after", "-isystem", "-iquote",
                                        "-idirafter", "--gcc-toolchain=", "--sysroot=",
                                        "--gcc-toolchain", "--sysroot", "-I"}) {
      auto value = llvm::StringRef(arguments[i]);
      if (!value.consume_front(option))
        continue;
      auto *argument = &arguments[i];
      if (value.empty()) {
        if (i + 1 == arguments.size())
          break;
        argument = &arguments[++i];
        value = *argument;
      }
      if (!value.empty() && !::tracer::compat::starts_with(value, "=") && !::tracer::compat::starts_with(value, "$SYSROOT") &&
          !::tracer::compat::starts_with(value, "-") && std::filesystem::path(value.str()).is_relative())
        argument->replace(argument->size() - value.size(), value.size(),
                          (directory / value.str()).string());
      break;
    }
  }
}

bool parse_and_materialize(Probe &probe, clang::Interpreter &interpreter,
                          const std::string &source, std::string &error) {
  auto parsed = interpreter.Parse(source);
  if (!parsed) {
    error = llvm::toString(parsed.takeError());
    return false;
  }
  if (!parsed->TheModule) {
    error = "Clang parser produced no LLVM module";
    return false;
  }
  for (const auto &function : parsed->TheModule->functions()) {
    if (function.isDeclaration() && function.hasExternalLinkage() &&
        !function.isIntrinsic() &&
        !::tracer::compat::starts_with(function.getName(), "llvm."))
      probe.external_link_candidates.push_back(function.getName().str());
  }
  for (const auto &global : parsed->TheModule->globals()) {
    if (global.isDeclaration() && global.hasExternalLinkage() &&
        !::tracer::compat::starts_with(global.getName(), "llvm."))
      probe.external_link_candidates.push_back(global.getName().str());
  }
  llvm::SmallVector<char, 0> bitcode;
  llvm::raw_svector_ostream output(bitcode);
  llvm::WriteBitcodeToFile(*parsed->TheModule, output);
  auto owned_context = std::make_unique<llvm::LLVMContext>();
  auto copied_buffer = llvm::MemoryBuffer::getMemBufferCopy(
      llvm::StringRef(bitcode.data(), bitcode.size()),
      "clang-tracer-prepared-callback.bc");
  auto copied_module = llvm::parseBitcodeFile(copied_buffer->getMemBufferRef(),
                                               *owned_context);
  if (!copied_module) {
    error = llvm::toString(copied_module.takeError());
    return false;
  }
  auto execution_engine = interpreter.getExecutionEngine();
  if (!execution_engine) {
    error = llvm::toString(execution_engine.takeError());
    return false;
  }
  llvm::orc::ThreadSafeContext safe_context(std::move(owned_context));
  llvm::orc::ThreadSafeModule safe_module(std::move(*copied_module),
                                          std::move(safe_context));
  if (auto failure = execution_engine->addIRModule(std::move(safe_module))) {
    error = llvm::toString(std::move(failure));
    return false;
  }
  return true;
}

bool capture_orc_errors(Probe &probe, std::string &error) {
  auto jit = probe.interpreter->getExecutionEngine();
  if (!jit) {
    error = llvm::toString(jit.takeError());
    return false;
  }
  probe.orc_errors = std::make_shared<OrcErrorCapture>();
  const auto capture = probe.orc_errors;
  jit->getExecutionSession().setErrorReporter([capture](llvm::Error failure) {
    const auto message = llvm::toString(std::move(failure));
    std::lock_guard<std::mutex> lock(capture->mutex);
    if (!capture->message.empty())
      capture->message += '\n';
    capture->message += message;
    capture->ready.notify_all();
  });
  return true;
}

void append_orc_errors(const Probe &probe, std::string &error) {
  if (!probe.orc_errors)
    return;
  std::unique_lock<std::mutex> lock(probe.orc_errors->mutex);
  if (probe.orc_errors->message.empty())
    probe.orc_errors->ready.wait_for(
        lock, std::chrono::milliseconds(100),
        [&] { return !probe.orc_errors->message.empty(); });
  if (probe.orc_errors->message.empty())
    return;
  if (!error.empty())
    error += '\n';
  error += probe.orc_errors->message;
}

void append_unresolved_link_candidates(
    const Probe &probe, const clangtracer::v1::ProbeSpec &spec,
    std::string &error) {
  std::unordered_set<std::string> bound_methods;
  const auto collect_methods = [&](const auto &self,
                                  const clangtracer::v1::DebugInfo &info) -> void {
    for (const auto &method : info.methods())
      if (method.runtime_entry() && !method.linkage_symbol().empty())
        bound_methods.insert(method.linkage_symbol());
    for (const auto &argument : info.arguments())
      if (argument.has_object_type())
        self(self, argument.object_type());
    if (info.has_result_object_type())
      self(self, info.result_object_type());
  };
  if (spec.has_debug_info())
    collect_methods(collect_methods, spec.debug_info());

  std::vector<std::string> missing;
  for (const auto &name : probe.external_link_candidates) {
    if (name.empty() || name.rfind("tracer_", 0) == 0 ||
        bound_methods.count(name) || ::dlsym(RTLD_DEFAULT, name.c_str()))
      continue;
    if (std::find(missing.begin(), missing.end(), name) == missing.end())
      missing.push_back(name);
  }
  if (missing.empty())
    return;
  for (const auto &name : missing) {
    if (error.find(name) != std::string::npos)
      continue;
    if (!error.empty())
      error += '\n';
    error += "unresolved external symbol: " + name;
  }
}

bool initialize_prepared_code(clang::Interpreter &interpreter,
                              std::string &error) {
  auto execution_engine = interpreter.getExecutionEngine();
  if (!execution_engine) {
    error = llvm::toString(execution_engine.takeError());
    return false;
  }
  if (auto failure = execution_engine->initialize(
          execution_engine->getMainJITDylib())) {
    error = llvm::toString(std::move(failure));
    return false;
  }
  return true;
}

bool compile_shared_probe(Probe &p,
                          const clangtracer::v1::ProbeSpec &specification,
                          const std::vector<std::string> &arguments,
                          const std::string &wrapper_prelude,
                          const std::string &forced_include_source,
                          const std::string &receiver_alias,
                          const std::string &signature_assert,
                          std::string &error, bool run_initializers,
                          clangtracer::v1::ValidationReport *report,
                          bool result_sret, bool syntax_only,
                          const tracer::ValueSnapshotCode &snapshots,
                          const tracer::ValueSnapshotCode *plain_snapshots = nullptr,
                          const std::string &enum_declarations = {}) {
  tracer::ContextActivationRequest activation;
  const auto [callback_includes, callback_body] =
      split_callback_includes(specification.callback_source());
  const bool typed_prepared = tracer::prepared::is_typed_version(
      specification.prepared().format_version());
  std::string requested_header_source = typed_prepared
      ? tracer::prepared::adapter_source(specification.prepared()) : std::string();
  if (typed_prepared) {
    activation.prepared_adapter.offset =
        forced_include_source.size() + callback_includes.size();
    activation.prepared_adapter.size = requested_header_source.size();
    activation.prepared_adapter.symbol = specification.symbol();
  }
  for (const auto &header : specification.include_headers())
    requested_header_source += "#include \"" + header + "\"\n";
  const bool has_real_headers = typed_prepared || specification.has_header_context() ||
                                specification.include_headers_size() != 0 ||
                                has_user_includes(callback_includes) ||
                                !forced_include_source.empty();
  activation.source = forced_include_source + callback_includes +
                      requested_header_source +
                      (has_real_headers
                           ? enum_declarations
                           : recovered_record_declarations(specification)) +
                      argument_aliases(specification) +
                      interceptor_template_prelude(specification.callback_source()) +
                      "#line 1 \"callback_source\"\n" +
                      callback_body + "\n" + signature_assert;
  // Scalar shared wrappers need no user declarations. Recompiling their full
  // callback headers here can consume the operation deadline before admission.
  const auto *wrapper_snapshots = snapshots.arguments.empty() ? nullptr : &snapshots;
  const std::string wrapper_types = !wrapper_snapshots && !typed_prepared ? std::string() :
      callback_includes + requested_header_source +
      (has_real_headers ? enum_declarations : recovered_record_declarations(specification)) +
      receiver_alias + argument_aliases(specification);
  // Followed members must never turn a probe that worked into one that does not
  // compile: when the wrapper fails to compile with them, it is built without.
  const bool can_fall_back = snapshots.has_followed_members && plain_snapshots;
  const auto fall_back_note = [&] {
    if (!report)
      return;
    auto *note = report->add_diagnostics();
    note->set_stage("snapshot");
    note->set_severity("note");
    note->set_message("followed pointer and reference members were disabled because the "
                      "generated capture code did not compile");
  };
  const auto plain_wrapper = [&] {
    return wrapper_prelude + wrapper_types +
           make_wrapper(p.request, reinterpret_cast<std::uintptr_t>(p.trampoline), true, true,
                        &specification, false, true, result_sret,
                        plain_snapshots->arguments.empty() ? nullptr : plain_snapshots);
  };
  const auto syntax_ok = [&](const std::string &source, DiagnosticCapture &capture) {
    auto syntax_arguments = arguments;
    syntax_arguments.push_back("-fsyntax-only");
    auto ast = clang::tooling::buildASTFromCodeWithArgs(
        source, tracer::clang_driver_arguments(std::move(syntax_arguments)),
        "clang-tracer-shared-wrapper.cc",
        "clang-tracer", std::make_shared<clang::PCHContainerOperations>(),
        clang::tooling::getClangStripOutputAdjuster(), {}, &capture);
    return ast && !capture.had_error();
  };
  // A failed parse leaves partial declarations in the interpreter, so a retry
  // there fails on conflicts. Installing therefore checks the wrapper on a
  // throwaway AST first; only probes that follow members pay for it.
  bool use_plain = false;
  if (can_fall_back && !syntax_only) {
    DiagnosticCapture capture(nullptr);
    if (!syntax_ok(wrapper_prelude + wrapper_types +
                       make_wrapper(p.request, reinterpret_cast<std::uintptr_t>(p.trampoline),
                                    true, true, &specification, false, true, result_sret,
                                    wrapper_snapshots),
                   capture)) {
      use_plain = true;
      fall_back_note();
    }
  }
  const auto *effective_snapshots =
      use_plain ? (plain_snapshots->arguments.empty() ? nullptr : plain_snapshots)
                : wrapper_snapshots;
  activation.compiler_arguments = arguments;
  activation.callback_log = &tracer::detail::tracer_enqueue_log;
  if (!specification.callback_package_path().empty())
    activation.file_name = specification.callback_package_path();
  std::copy_n(p.request.source_lineage_id, activation.source_lineage_id.size(),
              activation.source_lineage_id.begin());
  if (!run_initializers) {
    const auto preflight =
        tracer::preflight_context_source(activation, syntax_only);
    if (!preflight) {
      error = preflight.error;
      if (report)
        append_source_errors(*report, preflight.source_errors);
      return false;
    }
    const auto generated_wrapper =
        make_wrapper(p.request,
                     reinterpret_cast<std::uintptr_t>(p.trampoline), true,
                     true, &specification, false, true, result_sret, effective_snapshots);
    const auto complete_source = wrapper_prelude + wrapper_types + generated_wrapper;
    if (syntax_only) {
      DiagnosticCapture capture(nullptr);
      const bool ok = syntax_ok(complete_source, capture);
      if (!ok && can_fall_back) {
        DiagnosticCapture retry(nullptr);
        if (syntax_ok(plain_wrapper(), retry)) {
          fall_back_note();
          return true;
        }
      }
      if (report)
        append_diagnostics(*report, capture, "syntax");
      if (!ok) {
        error = capture.message().empty()
                    ? "shared callback wrapper failed syntax validation"
                    : capture.message();
        return false;
      }
      return true;
    }
    if (!parse_and_materialize(p, *p.interpreter, complete_source, error))
      return false;
    auto wrapper = p.interpreter->getSymbolAddress("clangtracer_wrapper");
    if (!wrapper) {
      error = llvm::toString(wrapper.takeError());
      append_orc_errors(p, error);
      append_unresolved_link_candidates(p, specification, error);
      return false;
    }
    return true;
  }
  auto *owner = agent.load(std::memory_order_acquire);
  if (!owner) {
    error = "shared-context agent is unavailable";
    return false;
  }
  if (owner->next_context_hook == std::numeric_limits<std::uint64_t>::max()) {
    error = "shared-context hook identity space exhausted";
    return false;
  }
  if (std::all_of(activation.source_lineage_id.begin(),
                  activation.source_lineage_id.end(),
                  [](std::uint8_t byte) { return byte == 0; })) {
    error = "engine package catalog did not assign a source lineage";
    return false;
  }
  auto generation =
      tracer::activate_context_source(activation, owner->context_registry);
  if (!generation) {
    error = generation.error;
    return false;
  }
  if (!generation.generation->owner_code()->standard_library()->warm_unwind(
          p.request.original_entry)) {
    error = "unsupported_unwind_path: original target has no admitted native "
            "unwind frame";
    return false;
  }
  try {
    p.context_delivery =
        tracer::context_delivery::ContextAgentDelivery::acquire(p.request);
    const auto event_bytes = p.request.context_budget.event_bytes;
    if (!owner->context_capture || event_bytes > owner->context_capture->capacity())
      owner->context_capture =
          std::make_shared<tracer::context_engine::ContextCapturePool>(event_bytes);
    p.capture_pool = owner->context_capture;
    tracer::context_runtime::EventMetadata metadata;
    metadata.source_id.assign(
        reinterpret_cast<const char *>(activation.source_lineage_id.data()),
        activation.source_lineage_id.size());
    metadata.task_id = metadata.source_id;
    metadata.probe_id.assign(16, '\0');
    for (unsigned i = 0; i < 8; ++i)
      metadata.probe_id[i] = static_cast<char>(p.request.probe_id >> (8 * i));
    metadata.print_arguments = specification.print_arguments();
    metadata.print_result = specification.print_result();
    metadata.capture_hook_id = owner->next_context_hook++;
    metadata.capture_event_bytes = event_bytes;
    p.shared_runtime = std::make_shared<tracer::context_runtime::SharedSourceRuntime>(
        generation.generation->scopes(), *p.capture_pool, std::move(metadata),
        [](void* opaque, ::tracer::compat::span<const std::byte> bytes,
           const std::shared_ptr<const void>& schema_lease) noexcept {
          return static_cast<tracer::context_delivery::ContextAgentDelivery *>(
                     opaque)
              ->try_publish_capture(bytes, schema_lease);
        },
        p.context_delivery.get());
    p.shared_generation = std::move(generation.generation);
  } catch (const std::exception &exception) {
    error = exception.what();
    return false;
  }
  p.class_callback = true;
  p.has_pre = p.has_post = true;
  p.request.context_required = true;
  const auto generated_wrapper =
      make_wrapper(p.request, reinterpret_cast<std::uintptr_t>(p.trampoline),
                   true, true, &specification, false, true, result_sret, effective_snapshots);
  if (!parse_and_materialize(p, *p.interpreter,
                             wrapper_prelude + wrapper_types + generated_wrapper, error)) {
    return false;
  }
  if (run_initializers &&
      !initialize_prepared_code(*p.interpreter, error))
    return false;
  auto wrapper = p.interpreter->getSymbolAddress("clangtracer_wrapper");
  if (!wrapper) {
    error = llvm::toString(wrapper.takeError());
    return false;
  }
  p.wrapper = reinterpret_cast<void *>(
      static_cast<std::uintptr_t>(address_value(*wrapper)));
  if (!p.shared_generation->owner_code()->standard_library()->warm_unwind(
          reinterpret_cast<std::uintptr_t>(p.wrapper))) {
    error = "unsupported_unwind_path: generated native dispatch frame was not "
            "registered";
    return false;
  }
  p.request.wrapper_address = reinterpret_cast<std::uintptr_t>(p.wrapper);
  p.request.callback_address = p.shared_generation->pre_entry();
  p.receiver_ast.reset();
  return true;
}

// The agent's version script keeps BoundedProtoWriter's out-of-line members
// local, so dlsym cannot resolve them for generated snapshot code. Answer only
// those lookups, per interpreter, and only for the exact signature the agent
// implements: a declaration with other parameter types stays unresolved, so
// the JIT linker still reports it instead of calling an incompatible member.
// Nothing is exported from the agent.
class HiddenSnapshotSymbols final : public llvm::orc::DefinitionGenerator {
public:
  llvm::Error tryToGenerate(llvm::orc::LookupState &, llvm::orc::LookupKind,
                            llvm::orc::JITDylib &dylib,
                            llvm::orc::JITDylibLookupFlags,
                            const llvm::orc::SymbolLookupSet &names) override {
    using Writer = tracer::context_engine::BoundedProtoWriter;
    const Entry members[] = {
        entry("finish", &Writer::finish),
        entry("u64", &Writer::u64),
        entry("s64", &Writer::s64),
        entry("boolean", &Writer::boolean),
        entry("f32", &Writer::f32),
        entry("f64", &Writer::f64),
        entry("string", &Writer::string),
        entry("begin_message", &Writer::begin_message),
        entry("end_message", &Writer::end_message),
        entry("container_items", &Writer::container_items),
    };
    llvm::orc::SymbolMap found;
    for (const auto &requested : names) {
      const auto &name = requested.first;
      const std::string demangled = llvm::demangle(std::string(*name));
      for (const auto &member : members)
        if (demangled == member.signature) {
          found[name] = llvm::orc::ExecutorSymbolDef(
              llvm::orc::ExecutorAddr::fromPtr(member.address),
              llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable);
          break;
        }
    }
    if (found.empty())
      return llvm::Error::success();
    return dylib.define(llvm::orc::absoluteSymbols(std::move(found)));
  }

private:
  struct Entry {
    std::string signature; // demangled, parameter types included
    void *address;
  };

  // The Itanium spelling of each parameter type of the members above. The
  // agent is built without RTTI, so the spellings are written out.
  template <class T> static std::string mangled_type() {
    if constexpr (std::is_lvalue_reference_v<T>)
      return "R" + mangled_type<std::remove_reference_t<T>>();
    else if constexpr (std::is_same_v<T, bool>)
      return "b";
    else if constexpr (std::is_same_v<T, float>)
      return "f";
    else if constexpr (std::is_same_v<T, double>)
      return "d";
    else if constexpr (std::is_same_v<T, unsigned int>)
      return "j";
    else if constexpr (std::is_same_v<T, unsigned long>)
      return "m";
    else if constexpr (std::is_same_v<T, long>)
      return "l";
    else if constexpr (std::is_same_v<T, std::string_view>)
#ifdef _LIBCPP_VERSION
      return "NSt3__117basic_string_viewIcNS_11char_traitsIcEEEE";
#else
      return "St17basic_string_viewIcSt11char_traitsIcEE";
#endif
    else
      static_assert(!sizeof(T), "unmapped BoundedProtoWriter parameter type");
  }

  template <class... Args> static std::string parameters() {
    std::string mangled = "_Z1f";
    if constexpr (sizeof...(Args) == 0)
      mangled += 'v';
    else
      ((mangled += mangled_type<Args>()), ...);
    const std::string demangled = llvm::demangle(mangled);
    return demangled.substr(demangled.find('('));
  }

  template <class R, class... Args>
  static Entry entry(llvm::StringRef method,
                     R (tracer::context_engine::BoundedProtoWriter::*member)(
                         Args...) noexcept) {
    return {"tracer::context_engine::BoundedProtoWriter::" + method.str() +
                parameters<Args...>(),
            address(member)};
  }

  // Non-virtual Itanium member pointers keep the entry address in word one.
  template <class Member> static void *address(Member member) {
    static_assert(sizeof(Member) == 2 * sizeof(void *));
    std::uintptr_t words[2];
    std::memcpy(words, &member, sizeof(words));
    return reinterpret_cast<void *>(words[0]);
  }
};

bool bind_probe_symbols(clang::Interpreter &interpreter,
                        const clangtracer::v1::ProbeSpec &spec,
                        std::string &error) {
  auto jit = interpreter.getExecutionEngine();
  if (!jit) {
    error = llvm::toString(jit.takeError());
    return false;
  }
  // Clang's process-symbol generator uses dlsym, which cannot see our hidden
  // ELF helpers. Define their addresses in this interpreter's own JITDylib.
  llvm::orc::SymbolMap symbols;
  auto bind = [&](llvm::StringRef name, auto *address) {
    symbols[jit->mangleAndIntern(name)] = llvm::orc::ExecutorSymbolDef(
        llvm::orc::ExecutorAddr::fromPtr(address),
        llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable);
  };
  bind("tracer_callback_dispatch", &tracer_callback_dispatch);
  bind("tracer_swap_callback_depth", +[](unsigned depth) noexcept {
    return std::exchange(callback_depth, depth);
  });
  bind("tracer_original_entry_time", +[]() noexcept -> unsigned long long {
    timespec now{};
    if (::clock_gettime(CLOCK_MONOTONIC, &now) != 0)
      return 0;
    return static_cast<unsigned long long>(now.tv_sec) * 1000000000ULL + now.tv_nsec;
  });
  bind("tracer_context_check_entry", &tracer_context_check_entry);
  bind("tracer_shared_enter", &tracer_shared_enter);
  bind("tracer_shared_leave", &tracer_shared_leave);
  bind("tracer_shared_release", &tracer_shared_release);
  bind("tracer_shared_resume", &tracer_shared_resume);
  bind("tracer_shared_dispatch", &tracer_shared_dispatch);
  bind("tracer_shared_original_enter", &tracer_shared_original_enter);
  bind("tracer_shared_original_leave", &tracer_shared_original_leave);
  bind("tracer_callback_lifecycle_enter", &tracer_callback_lifecycle_enter);
  bind("tracer_callback_cleanup_enter", &tracer_callback_cleanup_enter);
  bind("tracer_callback_lifecycle_leave", &tracer_callback_lifecycle_leave);
  bind("tracer_callback_failure", &tracer_callback_failure);
  bind("tracer_enqueue_log", &tracer::detail::tracer_enqueue_log);
  bind("tracer_event_dispatch", &tracer_event_dispatch);
  bind("tracer_probe_enter", &tracer_probe_enter);
  bind("tracer_probe_leave", &tracer_probe_leave);
  bind("tracer_observer_clock", &tracer_observer_clock);
  bind("tracer_observer_return", &tracer_observer_return);
  bind("tracer_object_identity_lookup", &tracer_object_identity_lookup);
  bind("tracer_next_call_id", &tracer_next_call_id);
  bind("tracer_stack_enter", &tracer_stack_enter);
  bind("tracer_stack_leave", &tracer_stack_leave);
  bind("tracer_capture_stack", &tracer_capture_stack);
  bind("tracer_snapshot_mark", &tracer_snapshot_mark);
  bind("tracer_snapshot_read_rtti", &tracer_snapshot_read_rtti);
  bind("tracer_snapshot_read_rtti_provider", &tracer_snapshot_read_rtti_provider);
  bind("tracer_snapshot_readable", &tracer_snapshot_readable);
  bind("tracer_snapshot_rtti_matches", &tracer_snapshot_rtti_matches);
  bind("tracer_snapshot_readable", &tracer_snapshot_readable);
  bind("tracer_snapshot_configure", &tracer_snapshot_configure);
  bind("tracer_snapshot_read_cstring", &tracer_snapshot_read_cstring);
  bind("tracer_snapshot_copy_bytes", &tracer_snapshot_copy_bytes);
  bind("tracer_snapshot_is_scratch", &tracer_snapshot_is_scratch);
  bind("tracer_snapshot_storage_limited", &tracer_snapshot_storage_limited);
  bind("tracer_snapshot_reset", &tracer_snapshot_reset);
  bind("tracer_snapshot_storage", &tracer_snapshot_storage);
  bind("tracer_snapshot_commit", &tracer_snapshot_commit);
  bind("tracer_capture_entry_stack", &tracer_capture_entry_stack);
  auto bind_debug_methods = [&](const clangtracer::v1::DebugInfo &info) {
    for (const auto &method : info.methods()) {
      // A virtual member of the receiver class is not bound as a direct symbol (a call to it
      // dispatches through the vtable), so it must not block probes on the class's other entries.
      if (method.is_virtual())
        continue;
      // A variadic member has no fixed signature to call it by; it stays unbound, so only a
      // callback that calls it directly fails to link.
      if (method.is_variadic())
        continue;
      if (method.calling_convention() != llvm::dwarf::DW_CC_normal) {
        error = "debug method uses an unsupported virtual, variadic, or "
                "nondefault calling convention: " + method.name();
        return false;
      }
      if (method.linkage_symbol().empty() || !method.runtime_entry())
        continue;
      symbols[jit->mangleAndIntern(method.linkage_symbol())] =
          llvm::orc::ExecutorSymbolDef(
              llvm::orc::ExecutorAddr::fromPtr(reinterpret_cast<void *>(
                  static_cast<std::uintptr_t>(method.runtime_entry()))),
              llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable);
    }
    return true;
  };
  if (spec.has_debug_info()) {
    if (!bind_debug_methods(spec.debug_info()))
      return false;
    for (const auto &argument : spec.debug_info().arguments())
      if (argument.has_object_type() &&
          !bind_debug_methods(argument.object_type()))
        return false;
    if (spec.debug_info().has_result_object_type() &&
        !bind_debug_methods(spec.debug_info().result_object_type()))
      return false;
  }
  if (auto failure = jit->getMainJITDylib().define(
          llvm::orc::absoluteSymbols(std::move(symbols)))) {
    error = llvm::toString(std::move(failure));
    return false;
  }
  jit->getMainJITDylib().addGenerator(std::make_unique<HiddenSnapshotSymbols>());
  return true;
}

bool validate_prepared_types(clang::ASTUnit &unit,
                             const clangtracer::v1::ProbeSpec &spec,
                             bool &result_sret, std::string &error) {
  using namespace clangtracer::v1;
  auto &ast = unit.getASTContext();
  class Finder : public clang::RecursiveASTVisitor<Finder> {
  public:
    Finder(clang::ASTContext &ast, const ProbeSpec &spec)
        : ast(ast), spec(spec), name(spec.prepared().selector().substr(
              0, spec.prepared().selector().find('('))) {
      name.erase(0, name.find_first_not_of(" \t"));
      name.erase(name.find_last_not_of(" \t") + 1);
      if (name.compare(0, 2, "::") == 0)
        name.erase(0, 2);
    }
    bool VisitFunctionDecl(clang::FunctionDecl *candidate) {
      if (!tracer::prepared::adapter_callable(*candidate, ast, name, spec.symbol()))
        return true;
      decl = candidate;
      return false;
    }
    clang::ASTContext &ast;
    const ProbeSpec &spec;
    std::string name;
    clang::FunctionDecl *decl = nullptr;
  } finder(ast, spec);
  try {
    finder.TraverseDecl(ast.getTranslationUnitDecl());
    const auto *decl = finder.decl;
    if (!decl)
      throw std::runtime_error("prepared declaration is absent from its adapter");
    const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(decl);
    const auto *function = decl->getType()->getAs<clang::FunctionProtoType>();
    if (!function || function->isVariadic() || function->getCallConv() != clang::CC_C)
      throw std::runtime_error("prepared declaration now has an unsupported ABI");
    const auto function_info = function->getExtInfo();
    if (function_info.getHasRegParm() || function_info.getProducesResult() ||
        function_info.getNoCallerSavedRegs() || function_info.getCmseNSCall())
      throw std::runtime_error("prepared declaration now has an ABI-changing attribute");
    if (function->hasExtParameterInfos())
      for (const auto &parameter : function->getExtParameterInfos())
        if (parameter != clang::FunctionProtoType::ExtParameterInfo{})
          throw std::runtime_error("prepared declaration now has an ABI-annotated parameter");
    ProbeSpec actual;
    actual.mutable_signature()->set_kind(method && !method->isStatic() ? METHOD : FUNCTION);
    tracer::prepared::describe_prepared_callable(ast, *decl, method, actual,
        unit.hasSema() ? &unit.getSema() : nullptr, &spec.prepared().header_types());
    if (actual.debug_info().SerializeAsString() != spec.prepared().header_types().SerializeAsString() ||
        actual.signature().SerializeAsString() != spec.signature().SerializeAsString())
      throw std::runtime_error("prepared types or record layout changed; prepare again");
    if (spec.signature().result() == OBJECT) {
      llvm::LLVMContext llvm;
      clang::CodeGenOptions options;
      auto fs = llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem>(
          &unit.getFileManager().getVirtualFileSystem());
      std::unique_ptr<clang::CodeGenerator> generator(clang::CreateLLVMCodeGen(
          unit.getDiagnostics(), "prepared-abi", std::move(fs),
          unit.getHeaderSearchOpts(), unit.getPreprocessorOpts(), options, llvm));
      generator->Initialize(ast);
      const auto &abi = actual.signature().kind() == METHOD
          ? clang::CodeGen::arrangeCXXMethodType(generator->CGM(), method->getParent(), function, method)
          : clang::CodeGen::arrangeFreeFunctionType(generator->CGM(),
                ast.getCanonicalType(decl->getType()).getAs<clang::FunctionProtoType>());
      result_sret = abi.getReturnInfo().isIndirect();
    }
    return true;
  } catch (const std::exception &exception) {
    error = exception.what();
    return false;
  }
}

bool compile_probe(Probe &p, clangtracer::v1::ProbeSpec &target_specification,
                   std::string &error, bool run_initializers = true,
                   bool syntax_only = false,
                   clangtracer::v1::ValidationReport *report = nullptr,
                   const VerifiedDebugEntry *verified_entry = nullptr) {
  const bool typed_prepared = tracer::prepared::is_typed_version(
      target_specification.prepared().format_version());
  const bool has_header_context = target_specification.has_header_context();
  const bool prepared_callback_context = typed_prepared && has_header_context;
  if (const auto *invalid = tracer::scalar_override_error(target_specification)) {
    error = invalid;
    return false;
  }
  const bool overrides = tracer::has_scalar_overrides(target_specification);
  const auto validate_options = [&error](const auto &first, const auto &last,
                                         const char *source,
                                         bool allow_toolchain = false) {
    for (auto current = first; current != last; ++current) {
      if (current->find('\0') != std::string::npos) {
        error = std::string("invalid compiler option in ") + source;
        return false;
      }
    }
    if (!tracer::safe_preflight_compiler_arguments(first, last,
                                                   allow_toolchain)) {
      error = std::string("unsupported compiler option in ") + source;
      return false;
    }
    return true;
  };
  // Do this before header AST recovery and before constructing either Clang
  // compiler. Driver plugins and raw frontend escapes can execute code during
  // nominally syntax-only validation.
  const auto &extra = target_specification.extra_compile_arguments();
  if (!extra.empty()) {
    try {
      tracer::validate_extra_compile_arguments(
          {extra.begin(), extra.end()}, tracer::primary_probe_compile_arguments(target_specification),
          tracer::primary_probe_compile_directory(target_specification));
    } catch (const std::invalid_argument &exception) {
      error = exception.what();
      return false;
    }
  }
  if (!validate_options(target_specification.compile_options().begin(),
                        target_specification.compile_options().end(),
                        "probe options") ||
      (!typed_prepared && target_specification.has_header_context() &&
       !validate_options(
           target_specification.header_context().compiler_arguments().begin(),
           target_specification.header_context().compiler_arguments().end(),
           "header context", true)) ||
      (target_specification.has_receiver() &&
       (target_specification.receiver().compiler_arguments_size() == 0 ||
        !validate_options(
            std::next(target_specification.receiver().compiler_arguments().begin()),
            target_specification.receiver().compiler_arguments().end(),
            "receiver context", true)))) {
    return false;
  }
  if (typed_prepared) {
    if (!prepared_callback_context && !target_specification.include_headers().empty()) {
      error = "prepared injection requires a callback compilation context for optional includes";
      return false;
    }
    if (prepared_callback_context) {
      const auto &context = target_specification.header_context();
      const auto &directory = context.working_directory();
      if (directory.empty() || directory.front() != '/' || directory.size() > 4095 ||
          directory.find('\0') != std::string::npos || context.ByteSizeLong() > 64 * 1024 ||
          context.compiler_arguments_size() > 512 ||
          !tracer::safe_prepared_callback_compiler_arguments(
              context.compiler_arguments().begin(), context.compiler_arguments().end())) {
        error = "invalid prepared callback compilation context";
        return false;
      }
    }
    if (target_specification.include_headers_size() > 16 ||
        std::any_of(target_specification.include_headers().begin(), target_specification.include_headers().end(),
                    [](const auto &header) {
                      return header.empty() || header.front() != '/' || header.size() > 4095 ||
                             header.find_first_of("\"\r\n\0", 0, 4) != std::string::npos;
                    })) {
      error = "invalid optional include path";
      return false;
    }
  }
  const std::string &source = target_specification.callback_source();
  auto [callback_includes, callback_body] = split_callback_includes(source);
  std::vector<std::string> user_options;
  if (has_header_context) {
    const auto &options = target_specification.header_context().compiler_arguments();
    user_options.assign(options.begin(), options.end());
  }
  if (prepared_callback_context) {
    const auto &explicit_options = target_specification.compile_options();
    for (auto it = explicit_options.begin(); it != explicit_options.end(); ++it) {
      const std::string_view option(*it);
      if (option.rfind("-D", 0) != 0 && option.rfind("-U", 0) != 0)
        continue;
      // The explicit options passed preflight, so split macro flags have operands.
      const auto value = option.size() == 2 ? std::string_view(*++it) : option.substr(2);
      if (tracer::prepared_callback_abi_macro(value)) {
        error = "callback compiler options cannot override prepared _GLIBCXX_USE_CXX11_ABI";
        return false;
      }
    }
  }
  // resolve_header_context() appends compile_options verbatim as the suffix
  // of a legacy context's compiler_arguments. Check that suffix before adding
  // them again; prepared contexts use a different builder and need the append.
  // Keep this check before path normalization, which can change those strings.
  const auto &explicit_options = target_specification.compile_options();
  if (typed_prepared || user_options.size() < static_cast<std::size_t>(explicit_options.size()) ||
      !std::equal(explicit_options.rbegin(), explicit_options.rend(), user_options.rbegin()))
    user_options.insert(user_options.end(), explicit_options.begin(), explicit_options.end());
  std::filesystem::path callback_directory;
  if (!typed_prepared && has_header_context) {
    callback_directory = target_specification.header_context().working_directory();
    resolve_callback_option_paths(user_options, callback_directory);
  }
  user_options.insert(user_options.end(), extra.begin(), extra.end());
  std::string forced_include_source;
  if (!extract_forced_includes(user_options, forced_include_source, error, callback_directory))
    return false;
  // A callable that uses standard-library records and comes with no headers of the caller's
  // gets the real standard headers those records need, chosen from the target's DWARF and
  // compiled with the agent's own toolchain: the manual header route, selected automatically.
  // Recovered records are never rebuilt as handwritten std:: classes.
  const bool automatic_route =
      !typed_prepared && target_specification.has_debug_info() && !has_header_context &&
      target_specification.include_headers().empty() && forced_include_source.empty() &&
      !has_user_includes(callback_includes);
  if (!automatic_route && target_specification.standard_headers_size() != 0) {
    error = "--std-header maps records for the automatic selection of standard headers, which "
            "applies only to a DWARF-resolved probe without headers of its own (no --include, "
            "--compdb, forced include, callback #include or prepared context); remove it, or "
            "the headers";
    return false;
  }
  if (automatic_route && !has_automatic_headers(callback_includes)) {
    std::string automatic;
    int standard = 0;
    if (!automatic_standard_headers(target_specification, automatic, standard, error))
      return false;
    if (!automatic.empty()) {
      if (auto problem = undeclarable_argument_type(target_specification); !problem.empty()) {
        error = std::move(problem);
        return false;
      }
      // Options the selection adds are part of the probe from here on: the type AST and the
      // callback are compiled with them, and a validation report lists them.
      const auto add_option = [&](std::string option) {
        target_specification.add_compile_options(option);
        user_options.push_back(std::move(option));
      };
      // The headers must select the libstdc++ ABI the target was built with. The target's
      // debug data says which one that is; the agent's headers default to the ABI of the
      // agent's own toolchain. An explicit _GLIBCXX_USE_CXX11_ABI from the caller always wins,
      // but one that disagrees with the target is reported here, before any header is read.
      const int target_abi = target_cxx11_abi(target_specification);
      const int explicit_abi = ::tracer::standard_headers::explicit_cxx11_abi(user_options);
      if (target_abi >= 0 && (explicit_abi == 0 || explicit_abi == 1) &&
          explicit_abi != target_abi) {
        error = std::string("libstdc++ ABI mismatch: the target uses the ") +
                (target_abi ? "C++11 (std::__cxx11)" : "pre-C++11") +
                " string/list ABI according to its DWARF, but the options select "
                "_GLIBCXX_USE_CXX11_ABI=" + std::to_string(explicit_abi) +
                "; pass -D_GLIBCXX_USE_CXX11_ABI=" + std::to_string(target_abi) +
                " or remove the option that sets it";
        return false;
      }
      tracer::AgentRuntimeProfile profile;
      const bool have_profile = tracer_agent_runtime_profile_v1(&profile, sizeof(profile)) == 0;
      if (target_abi >= 0 && explicit_abi == -1 && have_profile &&
          profile.libstdcxx_default_cxx11_abi >= 0 &&
          profile.libstdcxx_default_cxx11_abi != target_abi)
        add_option(std::string("-D_GLIBCXX_USE_CXX11_ABI=") + (target_abi ? "1" : "0"));
      // The headers are compiled as C++20 unless the target is known to be newer: its producer
      // names a later standard, or it uses a record that only a later standard has
      // (std::expected). An explicit -std= from the caller always wins.
      standard = std::max(standard,
                          ::tracer::standard_headers::producer_cxx_standard(
                              target_specification.debug_info().producer()));
      const bool explicit_standard =
          std::any_of(user_options.begin(), user_options.end(), [](const std::string &option) {
            return ::tracer::compat::starts_with(option, "-std") ||
                   ::tracer::compat::starts_with(option, "--std");
          });
      if (standard > 20 && !explicit_standard)
        add_option("-std=c++" + std::to_string(standard));
      target_specification.set_callback_source(automatic + target_specification.callback_source());
      auto split = split_callback_includes(target_specification.callback_source());
      callback_includes = std::move(split.first);
      callback_body = std::move(split.second);
    }
  }
  const std::string type_header_prefix =
      forced_include_source + callback_includes;
  std::string requested_header_source = typed_prepared
      ? tracer::prepared::adapter_source(target_specification.prepared()) : std::string();
  for (const auto &header : target_specification.include_headers())
    requested_header_source += "#include \"" + header + "\"\n";
  const std::string callback_header_source =
      callback_includes + requested_header_source;
  // Headers the caller supplied (or an adapter or forced include): records they define are
  // used as they are, and any other record is an error rather than a recovered declaration.
  const bool user_headers_available =
      typed_prepared || target_specification.has_header_context() ||
      target_specification.include_headers_size() != 0 ||
      has_user_includes(callback_includes) || !forced_include_source.empty();
  // Real headers of any kind, including the standard headers chosen automatically.
  const bool callback_headers_available =
      user_headers_available || has_automatic_headers(callback_includes);
  std::optional<VerifiedDebugEntry> local_verified_entry;
  if (target_specification.has_debug_info()) {
    try {
      if (!verified_entry)
        local_verified_entry = VerifiedDebugEntry{
            &target_specification, validate_debug_entry(target_specification)};
      else if (verified_entry->spec != &target_specification)
        throw std::runtime_error("debug entry verification does not match probe");
      if (local_verified_entry)
        verified_entry = &*local_verified_entry;
    } catch (const std::exception &exception) {
      error = exception.what();
      return false;
    }
  }
  std::optional<clangtracer::v1::ProbeSpec> recovered;
  try {
    if (!user_headers_available && target_specification.has_debug_info())
      recovered = template_recovery_spec(target_specification);
  } catch (const std::exception &exception) {
    error = exception.what();
    return false;
  }
  auto &specification = recovered ? *recovered : target_specification;
  if (recovered) {
    local_verified_entry = VerifiedDebugEntry{&specification, verified_entry->runtime_entry};
    verified_entry = &*local_verified_entry;
  }
  // Declarations recovered from DWARF for the callback. With headers of the caller's only the
  // enumerations the signature names are declared, and only those the headers lack; see
  // user_enum_declarations below.
  std::string callback_type_declarations =
      user_headers_available ? std::string() : recovered_record_declarations(specification);
  if (!typed_prepared && specification.has_debug_info() &&
      !interceptor_template_prelude(source).empty()) {
    const auto has_multiple_pointers = [](const std::string &type) {
      return record_pointer_levels(type) > 1;
    };
    for (const auto &argument : specification.debug_info().arguments()) {
      if (argument.has_object_type() &&
          has_multiple_pointers(argument.cpp_type())) {
        error = "multi-level pointers to recovered record argument " +
                std::to_string(argument.index()) +
                " are not supported by Interceptor signature validation";
        return false;
      }
    }
    if (specification.debug_info().has_result_object_type() &&
        has_multiple_pointers(
            specification.debug_info().result_cpp_type())) {
      error = "multi-level pointers to recovered record results are not "
              "supported by Interceptor signature validation";
      return false;
    }
  }
  const bool variadic_target =
      specification.has_debug_info() && specification.debug_info().callable_is_variadic();
  if (variadic_target) {
    // The entry stub forwards the original call before any return is seen, and re-passes no
    // argument: nothing that needs the original's result or call can be offered.
    const char *unsupported =
        typed_prepared ? "prepared injection" :
        overrides ? "argument, result or replacement control, and calling the original," :
        specification.trace_exceptions() ? "exception tracing" :
        specification.snapshot_receiver_at_return() ? "receiver sampling at return" :
        specification.stack_depth() ? "stack capture" :
        p.request.patch_backend == tracer::NativePatch::CallSite ? "a call-site patch" : nullptr;
    if (unsupported) {
      error = std::string("variadic callable: ") + unsupported +
              " is unsupported; only PRE observation of the fixed arguments is available";
      return false;
    }
  }
  p.argument_names.assign(specification.argument_names().begin(),
                          specification.argument_names().end());
  clang::CXXRecordDecl *receiver_record = nullptr;
  // Header-backed callbacks need the real declaration for their receiver
  // alias. Headerless callbacks can reject syntax before recovering types.
  const bool callback_header_types = !typed_prepared &&
                                     specification.has_debug_info() &&
                                     callback_headers_available;
  if (callback_header_types &&
      !build_and_validate_receiver_ast(specification, p.request, p.receiver_ast,
                                       receiver_record, error,
                                       p.debug_records,
                                       has_header_context ? callback_includes : type_header_prefix,
                                       verified_entry)) {
    return false;
  }
  // Header-backed records must be compiled from those headers in every
  // callback TU. Only records synthesized from DWARF need AST import.
  const bool callback_header_receiver =
      callback_header_types && receiver_record &&
      debug_record_from_header(receiver_record);
  const std::string header_receiver_alias =
      callback_header_receiver
          ? "namespace tracer { using ResolvedTarget = const " +
                std::string(specification.receiver().is_volatile() ? "volatile "
                                                                   : "") +
                "::" + specification.receiver().record_name() + "; }\n"
          : std::string();
  if (!initialize_llvm(error)) {
    return false;
  }
  const std::string resource_dir_argument =
      std::string("-resource-dir=") + tracer::runtime::clang_resource_dir();
  auto compiler_arguments = tracer::runtime::jit_arguments();
  if (typed_prepared) {
    try {
      compiler_arguments = tracer::prepared::toolchain_arguments(
          specification.prepared(), std::move(compiler_arguments));
    } catch (const std::exception &exception) {
      error = exception.what();
      return false;
    }
  }
  compiler_arguments.insert(
      compiler_arguments.end(),
      {"-std=c++20", "-x", "c++", "-fPIC", "-O0", resource_dir_argument});
  // The generated wrapper uses C++ exceptions, which CL mode disables by default.
  if (tracer::clang_cl_driver(compiler_arguments))
    compiler_arguments.push_back("-fcxx-exceptions");
  std::vector<llvm::sys::fs::UniqueID> prepared_header_files;
  if (typed_prepared) {
    auto header_check_arguments = compiler_arguments;
    header_check_arguments.push_back("-fsyntax-only");
    DiagnosticCapture capture(nullptr);
    auto toolchain_ast = clang::tooling::buildASTFromCodeWithArgs(
        "", tracer::clang_driver_arguments(std::move(header_check_arguments)),
        "clang-tracer-toolchain-headers.cc",
        "clang-tracer", std::make_shared<clang::PCHContainerOperations>(),
        clang::tooling::getClangStripOutputAdjuster(), {}, &capture);
    if (!toolchain_ast || capture.had_error()) {
      error = capture.message().empty()
                  ? "cannot initialize the selected toolchain header search"
                  : capture.message();
      return false;
    }
    try {
      const auto &target = toolchain_ast->getASTContext().getTargetInfo();
      const auto &triple = target.getTriple();
      if (triple.getArch() != llvm::Triple::x86_64 || !triple.isOSLinux() ||
          !triple.isOSBinFormatELF() || !target.isLittleEndian() ||
          target.getPointerWidth(clang::LangAS::Default) != 64 || target.getLongWidth() != 64 ||
          !target.getCXXABI().isItaniumFamily())
        throw std::invalid_argument("prepared toolchain ABI does not match the trace target");
      const auto &search = toolchain_ast->getPreprocessor().getHeaderSearchInfo();
      tracer::prepared::check_toolchain_headers(search, specification.prepared());
      if (prepared_callback_context)
        for (const auto &header : specification.prepared().adapter_standard_headers())
          prepared_header_files.push_back(tracer::prepared::toolchain_header(search, header)->getUniqueID());
    } catch (const std::exception &exception) {
      error = exception.what();
      return false;
    }
  }
  const auto &snapshot_debug = specification.debug_info();
  const bool needs_snapshot_headers =
      (typed_prepared && specification.signature().kind() == clangtracer::v1::METHOD) ||
      snapshot_debug.result_is_reference() ||
      snapshot_debug.result_is_object_value() ||
      specification.snapshot_return_pointee() ||
      std::any_of(snapshot_debug.arguments().begin(), snapshot_debug.arguments().end(),
                  [](const auto &argument) {
                    return argument.is_reference() || argument.is_object_value();
                  });
  if (needs_snapshot_headers) {
    try {
      const auto snapshot_headers = tracer::context_header_directory();
      compiler_arguments.push_back("-I" + snapshot_headers);
      compiler_arguments.push_back("-I" + snapshot_headers + "/../include");
    } catch (const std::exception &) {
      // Scalar/legacy hooks use embedded headers and need no snapshot codecs.
    }
  }
  if (typed_prepared) {
    const auto &options = specification.prepared().adapter_abi_options();
    compiler_arguments.insert(compiler_arguments.end(), options.begin(), options.end());
  }
  compiler_arguments.insert(compiler_arguments.end(), user_options.begin(),
                            user_options.end());
  const auto interpreter_arguments = tracer::clang_driver_arguments(compiler_arguments);
  std::vector<const char *> args;
  for (const auto &argument : interpreter_arguments)
    args.push_back(argument.c_str());
  clang::IncrementalCompilerBuilder builder;
  builder.SetCompilerArgs(args);
  auto compiler = builder.CreateCpp();
  if (!compiler) {
    error = llvm::toString(compiler.takeError());
    return false;
  }
  std::optional<tracer::callback_pch::Artifact> interpreter_pch;
  std::optional<tracer::callback_pch::Artifact> syntax_pch;
  // C++17 extracts user -include options into source text. Keep the PCH out of
  // both frontends in that case so macro and include order stay unchanged.
  if (forced_include_source.empty()) {
    interpreter_pch = tracer::callback_pch::prepare(
        (*compiler)->getInvocation(), tracer::callback_pch::Mode::Interpreter);
    syntax_pch = tracer::callback_pch::prepare(
        (*compiler)->getInvocation(), tracer::callback_pch::Mode::Syntax);
  }
  if (interpreter_pch) {
    try {
      tracer::callback_pch::attach((*compiler)->getInvocation(),
                                   *interpreter_pch);
    } catch (...) {
      tracer::callback_pch::disable(*interpreter_pch);
      interpreter_pch.reset();
    }
  }
  {
    // Interpreter creation may initialize the PCH's initial header IR and
    // start LLVM worker threads. Those are compiler workers, not application
    // child tasks; do not associate them with an active shared context.
    tracer::context_execution::Scope compiler_startup(
        tracer::context_execution::Mode::LifecycleNoTrace);
    if (!compiler_startup.entered()) {
      error = "cannot suppress context propagation during interpreter "
              "creation";
      return false;
    }
    const auto create_interpreter = [](std::unique_ptr<clang::CompilerInstance> ci) {
      // Clang parses its own runtime here (including a signed/unsigned loop).
      // A donor's -Werror must apply to callbacks, not this generated runtime.
      const bool ignore_warnings = ci->getDiagnostics().getIgnoreAllWarnings();
      ci->getDiagnostics().setIgnoreAllWarnings(true);
      auto result = clang::Interpreter::create(std::move(ci));
      if (result)
        (*result)->getCompilerInstance()->getDiagnostics().setIgnoreAllWarnings(
            ignore_warnings);
      return result;
    };
    auto created = create_interpreter(std::move(*compiler));
    if (!created && interpreter_pch) {
      llvm::consumeError(created.takeError());
      tracer::callback_pch::disable(*interpreter_pch);
      interpreter_pch.reset();
      clang::IncrementalCompilerBuilder fallback_builder;
      fallback_builder.SetCompilerArgs(args);
      auto fallback_compiler = fallback_builder.CreateCpp();
      if (!fallback_compiler) {
        error = llvm::toString(fallback_compiler.takeError());
        return false;
      }
      created = create_interpreter(std::move(*fallback_compiler));
    }
    if (!created) {
      error = llvm::toString(created.takeError());
      return false;
    }
    p.interpreter = std::move(*created);
  }
  if (!capture_orc_errors(p, error))
    return false;
  const std::string prelude =
      std::string(tracer::kEmbeddedProtocolHeader) +
      "\nusing int32_t=std::int32_t; using int64_t=std::int64_t; "
      "using uint32_t=std::uint32_t; using uint64_t=std::uint64_t; "
      "using uintptr_t=std::uintptr_t;\n"
      "#define CLANG_TRACER_CALLBACK_TYPES_DEFINED 1\n";
  const std::string context_prelude =
      std::string(tracer::kEmbeddedContextHeader) +
      "\n#define CLANG_TRACER_CONTEXT_TYPES_DEFINED 1\n";
  const std::string receiver_alias =
      typed_prepared && specification.signature().kind() == clangtracer::v1::METHOD
          ? "namespace tracer { using ResolvedTarget = " +
                std::string(snapshot_debug.receiver_volatile() ? "volatile " : "") +
                snapshot_debug.record_name() +
                "; }\n#define CLANG_TRACER_HAS_TYPED_RECEIVER 1\n"
          : std::string();
  /*
   * A typed receiver preflight imports the same real Clang declaration into a
   * temporary syntax-only translation unit. This keeps callback validation
   * ahead of Interpreter::ParseAndExecute without copying the class text.
   */
  auto syntax_args = compiler_arguments;
  syntax_args.push_back("-fsyntax-only");
  auto syntax_pch_args = syntax_args;
  if (syntax_pch)
    tracer::callback_pch::append_syntax_args(syntax_pch_args, *syntax_pch);
  syntax_args = tracer::clang_driver_arguments(std::move(syntax_args));
  syntax_pch_args = tracer::clang_driver_arguments(std::move(syntax_pch_args));
  const auto build_callback_ast =
      [&](const std::string &code, const char *file_name,
          DiagnosticCapture &capture) -> std::unique_ptr<clang::ASTUnit> {
    auto ast = clang::tooling::buildASTFromCodeWithArgs(
        code, syntax_pch ? syntax_pch_args : syntax_args, file_name,
        "clang-tracer", std::make_shared<clang::PCHContainerOperations>(),
        clang::tooling::getClangStripOutputAdjuster(), {}, &capture);
    if ((!ast || capture.had_error()) && syntax_pch) {
      capture.reset_capture();
      auto fallback = clang::tooling::buildASTFromCodeWithArgs(
          code, syntax_args, file_name, "clang-tracer",
          std::make_shared<clang::PCHContainerOperations>(),
          clang::tooling::getClangStripOutputAdjuster(), {}, &capture);
      if (fallback && !capture.had_error()) {
        tracer::callback_pch::disable(*syntax_pch);
        syntax_pch.reset();
        syntax_pch_args = syntax_args;
      }
      return fallback;
    }
    return ast;
  };
  const auto run_callback_tool = [&](auto &&make_action,
                                     const std::string &code,
                                     auto &&postcondition) {
    const auto run = [&](const std::vector<std::string> &arguments) {
      const bool frontend_succeeded = clang::tooling::runToolOnCodeWithArgs(
          make_action(), code, arguments, "clang-tracer-callback.cc",
          "clang-tracer", std::make_shared<clang::PCHContainerOperations>());
      return frontend_succeeded && postcondition();
    };
    const bool tried_pch = syntax_pch.has_value();
    const bool succeeded = run(tried_pch ? syntax_pch_args : syntax_args);
    if (!succeeded && tried_pch) {
      const bool fallback_succeeded = run(syntax_args);
      if (fallback_succeeded) {
        tracer::callback_pch::disable(*syntax_pch);
        syntax_pch.reset();
        syntax_pch_args = syntax_args;
      }
      return fallback_succeeded;
    }
    return succeeded;
  };
  // A caller's headers say nothing about an enumeration they do not declare, and an enumeration
  // carries no layout or ABI evidence that a header could contradict. So the ones the headers
  // lack are still declared from DWARF. The headers are parsed on their own to find them; a
  // header that does not parse is left to the parse of the whole callback to report.
  std::vector<const clangtracer::v1::DebugTemplateType *> header_missing_enums;
  if (!typed_prepared && user_headers_available && specification.has_debug_info() &&
      !callable_enums(specification).empty()) {
    DiagnosticCapture header_capture(nullptr);
    const auto header_ast = build_callback_ast(
        forced_include_source + prelude + context_prelude + callback_header_source,
        "clang-tracer-callback.cc", header_capture);
    if (header_ast && !header_capture.had_error()) {
      try {
        header_missing_enums = missing_callable_enums(header_ast->getASTContext(), specification);
        callback_type_declarations = recovered_record_group({}, header_missing_enums);
      } catch (const std::exception &exception) {
        error = exception.what();
        return false;
      }
    }
  }
  const std::string user_source = forced_include_source + prelude + context_prelude +
                                  callback_header_source +
                                  callback_type_declarations + receiver_alias +
                                  tracer::kEmbeddedCallbackHeader +
                                  argument_aliases(specification) +
                                  interceptor_template_prelude(source) +
                                  "#line 1 \"callback_source\"\n" +
                                  callback_body;
  CallbackShape shape;
  std::string bridge;
  std::string complete_source;
  std::string baseline_error;
  DiagnosticCapture user_capture(nullptr);
  auto user_ast =
      build_callback_ast(user_source, "clang-tracer-callback.cc", user_capture);
  // An enumeration the signature names but the callback does not declare is the usual reason a
  // callback that names it fails to parse; the compiler only says the name is unknown.
  const auto undeclared_enum_hint = [&]() -> std::string {
    if (!user_ast || typed_prepared || !specification.has_debug_info())
      return {};
    try {
      return undeclared_enum_diagnostic(user_ast->getASTContext(), specification);
    } catch (const std::exception &) {
      return {};
    }
  };
  const auto add_undeclared_enum_hint = [&] {
    const auto hint = undeclared_enum_hint();
    if (hint.empty())
      return;
    if (report) {
      auto *note = report->add_diagnostics();
      note->set_stage("syntax");
      note->set_severity("error");
      note->set_message(hint);
    }
    error += (error.empty() ? "" : "\n") + hint;
  };
  const auto report_parse_errors = [&] {
    if (report)
      append_diagnostics(*report, user_capture, "syntax");
    error = user_capture.message();
    add_undeclared_enum_hint();
  };
  // Receiver/field inference can repair semantic errors in this plain parse,
  // but cannot repair malformed C++. Keep its diagnostics ahead of ABI checks
  // and AST import, which can otherwise mask the callback's syntax failure.
  if (user_capture.had_syntax_error()) {
    report_parse_errors();
    return false;
  }
  if (!typed_prepared &&
      !validate_object_abi_evidence(specification, callback_includes,
                                    forced_include_source, error))
    return false;
  if (!typed_prepared && !callback_header_types &&
      !build_and_validate_receiver_ast(specification, p.request, p.receiver_ast,
                                      receiver_record, error, p.debug_records,
                                      has_header_context ? callback_includes
                                                         : type_header_prefix,
                                      verified_entry)) {
    return false;
  }
  bool result_sret = false;
  if (!typed_prepared &&
      !determine_object_result_sret(specification, p.debug_records,
                                   result_sret, error))
    return false;
  if (!bind_probe_symbols(*p.interpreter, specification, error))
    return false;
  // A callback that failed to parse explains the later generic rejections.
  const auto prefer_parse_errors = [&] {
    if (!user_capture.had_error())
      return;
    report_parse_errors();
  };
  bool shape_valid = false;
  if (user_ast) {
    if (prepared_callback_context) {
      const auto &search = user_ast->getPreprocessor().getHeaderSearchInfo();
      std::size_t index = 0;
      for (const auto &header : specification.prepared().adapter_standard_headers()) {
        const auto file = tracer::prepared::toolchain_header(search, header);
        if (!file || file->getUniqueID() != prepared_header_files[index++]) {
          error = "callback include paths shadow prepared toolchain header <" + header + ">";
          return false;
        }
      }
    }
    const auto context_declarations =
        tracer::analyze_context_declarations(user_ast->getASTContext());
    if (typed_prepared && !validate_prepared_types(*user_ast, specification, result_sret, error)) {
      // State shorthand fails this plain parse by design, so a context
      // callback keeps the specific rejection.
      if (!context_declarations.requires_context)
        prefer_parse_errors();
      return false;
    }
    if (context_declarations.requires_context && variadic_target) {
      error = "variadic callable: a shared-context callback is unsupported; only PRE observation "
              "of the fixed arguments is available";
      return false;
    }
    if (context_declarations.requires_context) {
      if (tracer::find_snapshot_encoders(user_ast->getASTContext()).requested) {
        error = "tracer_snapshot::encode requires an invocation-scoped callback";
        return false;
      }
      if (overrides) {
        error = "scalar overrides require an invocation-scoped Interceptor";
        return false;
      }
      std::string signature_assert;
      if (!interceptor_template_prelude(source).empty()) {
        shape_valid = inspect_callback_shape(user_ast->getASTContext(), shape,
                                             baseline_error, true);
        if (!shape_valid ||
            !validate_specialized_signature(user_ast->getASTContext(), shape,
                                            specification, error)) {
          if (error.empty())
            error = baseline_error.empty()
                        ? "shared callback has an invalid Interceptor signature"
                        : baseline_error;
          return false;
        }
        signature_assert = specialized_signature_assert(
            user_ast->getASTContext(), shape);
      }
      const auto plain_snapshots =
          tracer::make_value_snapshot_code(user_ast->getASTContext(), specification, false);
      return compile_shared_probe(
          p, specification, compiler_arguments,
          forced_include_source + prelude + context_prelude +
              tracer::kEmbeddedCallbackHeader,
          forced_include_source, receiver_alias,
          signature_assert, error, run_initializers, report, result_sret,
          syntax_only, tracer::make_value_snapshot_code(user_ast->getASTContext(), specification),
          &plain_snapshots, callback_type_declarations);
    }
    shape_valid = inspect_callback_shape(user_ast->getASTContext(), shape,
                                         baseline_error);
    // The default observer declares an empty post(); it observes nothing, so it is accepted.
    const auto empty_body = [](const clang::CXXMethodDecl *method) {
      const auto *body = method ? llvm::dyn_cast_or_null<clang::CompoundStmt>(method->getBody()) : nullptr;
      return body && body->body_empty();
    };
    if (shape_valid && variadic_target && shape.has_post && !empty_body(shape.post)) {
      error = "variadic callable: post() is unsupported because the original runs after the "
              "probe and its return cannot be observed; only pre() is available";
      return false;
    }
  } else {
    baseline_error = user_capture.message();
  }
  if (!user_capture.message().empty() &&
      (baseline_error.empty() || user_capture.had_error())) {
    baseline_error = user_capture.message();
  }

  bool baseline_preflight_succeeded = false;
  bool followed_members_disabled = false;
  DiagnosticCapture complete_capture(nullptr);
  // Clang can recover a legacy function declaration even when its body has
  // errors. Re-parsing that body with the wrapper only repeats its diagnostics.
  // Class callbacks still need the wrapper/receiver-inference recovery path.
  if (shape_valid && (shape.class_mode || !user_capture.had_error())) {
    if (overrides &&
        (!shape.class_mode ||
         ((specification.allow_argument_overrides() || specification.allow_replacement() ||
           specification.allow_call_original()) &&
          !shape.has_pre) ||
         (specification.allow_result_override() && !shape.has_post))) {
      error = "scalar control requires an Interceptor with pre() for arguments, replacement or calling the original and post() for result overrides";
      return false;
    }
    if (!validate_specialized_signature(user_ast->getASTContext(), shape,
                                        specification, error))
      return false;
    const auto snapshots = tracer::make_value_snapshot_code(user_ast->getASTContext(), specification);
    if (!validate_snapshot_encoder(shape, snapshots, error))
      return false;
    bridge = make_invocation_bridge(shape);
    const auto build_complete = [&](const tracer::ValueSnapshotCode &code,
                                    DiagnosticCapture &capture) {
      const std::string generated = make_wrapper(
          p.request, reinterpret_cast<std::uintptr_t>(p.trampoline),
          shape.has_pre, shape.has_post, &specification, shape.class_mode,
          false, result_sret, &code);
      complete_source = user_source + "\n" +
                        specialized_signature_assert(user_ast->getASTContext(),
                                                     shape) +
                        bridge + "\n" + generated;
      auto ast = build_callback_ast(complete_source, "clang-tracer-callback.cc", capture);
      return ast && !capture.had_error();
    };
    baseline_preflight_succeeded = build_complete(snapshots, complete_capture);
    if (!baseline_preflight_succeeded && snapshots.has_followed_members) {
      // Followed pointer and reference members must never turn a probe that
      // worked into one that does not compile: capture without them instead.
      const auto fallback = tracer::make_value_snapshot_code(
          user_ast->getASTContext(), specification, false);
      DiagnosticCapture retry_capture(nullptr);
      std::string ignored;
      if (validate_snapshot_encoder(shape, fallback, ignored) &&
          build_complete(fallback, retry_capture)) {
        if (report) {
          // The failed attempt's errors are kept as notes: the probe was built.
          const int first = report->diagnostics_size();
          append_diagnostics(*report, complete_capture, "snapshot");
          for (int i = first; i < report->diagnostics_size(); ++i)
            report->mutable_diagnostics(i)->set_severity("note");
          auto *note = report->add_diagnostics();
          note->set_stage("snapshot");
          note->set_severity("note");
          note->set_message("followed pointer and reference members were disabled because the "
                            "generated capture code did not compile");
        }
        followed_members_disabled = true;
        baseline_preflight_succeeded = true;
      }
    }
    if (!baseline_preflight_succeeded) {
      baseline_error = complete_capture.message();
      if (baseline_error.empty()) {
        baseline_error = "callback source failed Clang syntax validation";
      }
    }
  }

  // Header references such as &Meter::charge can parse without self() and
  // therefore bypass the receiver-import fallback. Check method references
  // there too, without changing the existing object-use admission paths.
  if (baseline_preflight_succeeded && callback_header_receiver) {
    error = ReceiverImportAction::validate_probed_method_uses(
        user_ast->getASTContext(), specification.receiver().record_name(),
        specification.symbol());
    if (!error.empty())
      return false;
  }

  if (baseline_preflight_succeeded && report && !followed_members_disabled) {
    append_diagnostics(*report, complete_capture, "wrapper");
  }

  if (!baseline_preflight_succeeded) {
    // Legacy callbacks do not need receiver-field inference. Re-importing a
    // header AST after a syntax error duplicates standard-library declarations
    // and can crash Clang instead of returning the original diagnostic.
    if (!p.receiver_ast || (has_header_context && !shape.class_mode)) {
      if (report) {
        append_diagnostics(*report, user_capture, "syntax");
        append_diagnostics(*report, complete_capture, "wrapper");
      }
      error = baseline_error.empty()
                  ? "callback source failed Clang syntax validation"
                  : baseline_error;
      add_undeclared_enum_hint();
      return false;
    }

    // A receiver that DWARF does not describe member by member is only a name with a size: there
    // is nothing to import, and a callback that reads its members cannot compile. Say that,
    // instead of reporting the failed import of an empty stand-in.
    if (receiver_record && !callback_header_receiver) {
      const auto opaque_receiver = std::find_if(
          p.debug_records.begin(), p.debug_records.end(), [&](const DebugRecord &entry) {
            return entry.decl == receiver_record && entry.opaque &&
                   !debug_record_from_header(entry.decl);
          });
      if (opaque_receiver != p.debug_records.end()) {
        const auto &debug = specification.debug_info();
        std::string reason = debug.record_opaque_reason();
        const auto prefix = debug.record_name() + ": ";
        if (::tracer::compat::starts_with(reason, prefix))
          reason.erase(0, prefix.size());
        if (reason.empty())
          reason = "its layout is not described by the debug data";
        if (report) {
          append_diagnostics(*report, user_capture, "syntax");
          append_diagnostics(*report, complete_capture, "wrapper");
        }
        error = "receiver " + debug.record_name() +
                " has no members on the headerless DWARF route: " + reason +
                "; the probe can still observe the call and the receiver address, but this "
                "callback needs the class's members; supply its header with --include (and "
                "--compdb) or an #include in the callback";
        if (!baseline_error.empty())
          error += "\n" + baseline_error;
        return false;
      }
    }
    auto *ci = p.interpreter->getCompilerInstance();
    if (!ci) {
      error = "Clang Interpreter has no CompilerInstance for receiver import";
      return false;
    }
    ReceiverASTImporter importer(p.interpreter->getASTContext(),
                                 ci->getFileManager(),
                                 p.receiver_ast->getASTContext(),
                                 p.receiver_ast->getFileManager(), false);
    for (const auto &entry : p.debug_records) {
      // Real standard-library headers are compiled in the callback/JIT TU.
      // Importing their donor-AST declarations and then including the header
      // again creates duplicate specializations and builtin declarations.
      if (standard_library_record_name(entry.layout.record_name()))
        continue;
      if (callback_header_types && debug_record_from_header(entry.decl))
        continue;
      if (auto err = importer.ImportDefinition(entry.decl)) {
        error = llvm::toString(std::move(err));
        if (has_header_context)
          prefer_parse_errors();
        return false;
      }
      auto imported = importer.Import(entry.decl);
      if (!imported) {
        error = llvm::toString(imported.takeError());
        return false;
      }
      if (!entry.opaque &&
          !validate_receiver_layout(
              p.interpreter->getASTContext(),
              llvm::cast<clang::CXXRecordDecl>(*imported), entry.layout,
              error))
        return false;
    }
    if (receiver_record && !callback_header_receiver) {
      if (llvm::Error import_error = importer.ImportDefinition(receiver_record)) {
        error = "cannot import selected receiver declaration: " +
                llvm::toString(std::move(import_error));
        return false;
      }
      auto imported_decl = importer.Import(receiver_record);
      if (!imported_decl) {
        error = "cannot retrieve imported selected receiver declaration: " +
                llvm::toString(imported_decl.takeError());
        return false;
      }
      auto *imported_receiver =
          llvm::dyn_cast<clang::CXXRecordDecl>(*imported_decl);
      if (!imported_receiver ||
          !validate_receiver_layout(p.interpreter->getASTContext(),
                                    imported_receiver, specification.receiver(),
                                    error)) {
        error = "receiver layout changed during AST import: " + error;
        // With automatically selected standard headers this import is only the fallback
        // after the generated callback failed to compile; that failure is the cause.
        if (!user_headers_available && has_automatic_headers(callback_includes) &&
            !baseline_error.empty())
          error = automatic_headers_failure(
              std::string(kAutomaticCallbackFailure) + ": " + baseline_error, specification,
              callback_includes);
        return false;
      }
      if (!create_receiver_alias(p.interpreter->getASTContext(),
                                 imported_receiver, specification.receiver())) {
        error = "cannot create typed receiver alias in interpreter AST";
        return false;
      }
    }

    // The typed parse has the receiver class imported from the debug-type AST, with the
    // enumerations nested in it. Declaring those again would define that class a second time.
    std::string typed_enum_declarations;
    if (user_headers_available && !header_missing_enums.empty()) {
      const auto declared = declared_names(p.interpreter->getASTContext());
      std::vector<const clangtracer::v1::DebugTemplateType *> remaining;
      for (const auto *type : header_missing_enums)
        if (!declared.enums.count(type->name()))
          remaining.push_back(type);
      typed_enum_declarations = recovered_record_group({}, remaining);
    }
    std::string typed_user_source =
        forced_include_source + prelude + context_prelude +
        (receiver_record && p.request.kind == tracer::CallableKind::Method
             ? "#define CLANG_TRACER_HAS_TYPED_RECEIVER 1\n"
             : "") +
        "#define CLANG_TRACER_FIELD_INFERENCE 1\n" +
        callback_header_source + header_receiver_alias +
        (user_headers_available
             ? typed_enum_declarations
             : argument_enum_declarations(specification) +
                   missing_recovered_record_declarations(
                       p.interpreter->getASTContext(), specification)) +
        tracer::kEmbeddedCallbackHeader +
        argument_aliases(specification) +
        interceptor_template_prelude(source) +
        "#line 1 \"callback_source\"\n" + callback_body;
    std::string import_error;
    ReceiverCallbackShape receiver_shape;
    const auto make_user_action =
        [&]() -> std::unique_ptr<clang::FrontendAction> {
      receiver_shape = ReceiverCallbackShape{};
      receiver_shape.argument_names = p.argument_names;
      import_error.clear();
      return std::make_unique<ReceiverImportAction>(
          p.receiver_ast->getASTContext(), p.receiver_ast->getFileManager(),
          receiver_record, receiver_shape, &specification.receiver(),
          receiver_record, &import_error, &p.debug_records, &specification,
          callback_header_types);
    };
    const bool user_syntax_ok =
        run_callback_tool(make_user_action, typed_user_source, [&] {
          return receiver_shape.valid && import_error.empty();
        });
    if (!user_syntax_ok || !receiver_shape.valid || !import_error.empty()) {
      error = import_error.empty() ? receiver_shape.error : import_error;
      if (error.empty()) {
        error = "typed receiver callback failed Clang syntax validation";
      }
      if (has_header_context)
        prefer_parse_errors();
      return false;
    }
    std::sort(receiver_shape.inferred_fields.rbegin(),
              receiver_shape.inferred_fields.rend());
    receiver_shape.inferred_fields.erase(
        std::unique(receiver_shape.inferred_fields.begin(),
                    receiver_shape.inferred_fields.end()),
        receiver_shape.inferred_fields.end());
    for (const auto &[offset, name] : receiver_shape.inferred_fields) {
      if (offset > typed_user_source.size() || !debug_path_identifier(name)) {
        error = "invalid inferred field location";
        return false;
      }
      typed_user_source.insert(offset, "\"" + name + "\", ");
    }
    const std::string inference_define =
        "#define CLANG_TRACER_FIELD_INFERENCE 1";
    const auto inference_pos = typed_user_source.find(inference_define);
    if (inference_pos != std::string::npos)
      typed_user_source.erase(inference_pos, inference_define.size());
    shape.class_mode = receiver_shape.class_mode;
    shape.has_pre = receiver_shape.has_pre;
    shape.has_post = receiver_shape.has_post;
    bridge = receiver_shape.bridge;
    const auto assemble = [&](const tracer::ValueSnapshotCode &code) {
      const std::string generated = make_wrapper(
          p.request, reinterpret_cast<std::uintptr_t>(p.trampoline),
          shape.has_pre, shape.has_post, &specification, shape.class_mode,
          false, result_sret, &code);
      complete_source = typed_user_source + "\n" +
                        specialized_signature_assert(user_ast->getASTContext(),
                                                     shape) +
                        bridge + "\n" + generated;
    };
    assemble(receiver_shape.snapshots);
    import_error.clear();
    ReceiverCallbackShape final_shape;
    const auto make_final_action =
        [&]() -> std::unique_ptr<clang::FrontendAction> {
      final_shape = ReceiverCallbackShape{};
      final_shape.argument_names = p.argument_names;
      import_error.clear();
      return std::make_unique<ReceiverImportAction>(
          p.receiver_ast->getASTContext(), p.receiver_ast->getFileManager(),
          receiver_record, final_shape, &specification.receiver(),
          receiver_record, &import_error, &p.debug_records, &specification,
          callback_header_types);
    };
    bool complete_syntax_ok =
        run_callback_tool(make_final_action, complete_source, [&] {
          return final_shape.valid && import_error.empty();
        });
    if ((!complete_syntax_ok || !final_shape.valid || !import_error.empty()) &&
        receiver_shape.snapshots.has_followed_members) {
      // Followed members must never turn a probe that worked into one that
      // does not compile: capture without them instead.
      assemble(receiver_shape.snapshots_plain);
      complete_syntax_ok = run_callback_tool(make_final_action, complete_source, [&] {
        return final_shape.valid && import_error.empty();
      });
      if (complete_syntax_ok && final_shape.valid && import_error.empty() && report) {
        auto *note = report->add_diagnostics();
        note->set_stage("snapshot");
        note->set_severity("note");
        note->set_message("followed pointer and reference members were disabled because the "
                          "generated capture code did not compile");
      }
    }
    if (!complete_syntax_ok || !final_shape.valid || !import_error.empty()) {
      error = import_error.empty() ? final_shape.error : import_error;
      if (error.empty()) {
        error = "typed receiver callback or wrapper failed Clang preflight";
      }
      return false;
    }
    if (syntax_only)
      return true;
  } else {
    // The callback and generated wrapper compiled without the receiver alias;
    // preserve legacy callbacks and avoid importing unrelated target bodies.
    p.receiver_ast.reset();
  }
  if (syntax_only)
    return true;
  p.class_callback = shape.class_mode;
  p.has_pre = shape.has_pre;
  p.has_post = shape.has_post;
  auto *ci = p.interpreter->getCompilerInstance();
  if (!ci) {
    error = "Clang Interpreter has no CompilerInstance";
    return false;
  }
  DiagnosticCaptureScope capture(ci->getDiagnostics());
  if (!parse_and_materialize(p, *p.interpreter, complete_source, error)) {
    append_diagnostic_text(error, capture.message());
    append_orc_errors(p, error);
    append_unresolved_link_candidates(p, specification, error);
    return false;
  }
  if (run_initializers &&
      !initialize_prepared_code(*p.interpreter, error))
    return false;
  auto cb = p.interpreter->getSymbolAddress("tracer_user_dispatch");
  if (!cb) {
    error = llvm::toString(cb.takeError());
    append_orc_errors(p, error);
    append_unresolved_link_candidates(p, specification, error);
    return false;
  }
  p.request.callback_address = address_value(*cb);
  p.callback = reinterpret_cast<long long (*)(const tracer::TraceContext *,
                                              void *, bool *)>(
      static_cast<std::uintptr_t>(p.request.callback_address));
  auto wrapper = p.interpreter->getSymbolAddress("clangtracer_wrapper");
  if (!wrapper) {
    error = llvm::toString(wrapper.takeError());
    append_orc_errors(p, error);
    append_unresolved_link_candidates(p, specification, error);
    return false;
  }
  append_diagnostic_text(error, capture.message());
  p.wrapper = reinterpret_cast<void *>(
      static_cast<std::uintptr_t>(address_value(*wrapper)));
  p.request.wrapper_address = reinterpret_cast<std::uintptr_t>(p.wrapper);
  p.receiver_ast.reset(); // Import, validation, and JIT parsing are complete.
  return true;
}

void copy_value_snapshot(const tracer::ValueSnapshot &in,
                         clangtracer::context::v1::ValueSnapshot &out,
                         unsigned member_depth) {
  using Snapshot = clangtracer::context::v1::ValueSnapshot;
  out.set_status(static_cast<Snapshot::Status>(in.status));
  if (in.cpp_type) out.set_cpp_type(in.cpp_type);
  if (in.detail) out.set_detail(in.detail);
  if (in.status != tracer::ValueSnapshot::Complete)
    return;
  if (!in.descriptor || !in.root_type || in.descriptor_size > 4096 ||
      in.payload_size > tracer::snapshot_slice_request().load() ||
      (!in.payload && in.payload_size != 0) ||
      in.container_role_count > 128 || in.record_type_count > 128 ||
      (in.member_count && (!member_depth || !in.members ||
                           in.member_count > tracer::snapshot_member_request().load()))) {
    out.set_status(Snapshot::FAILED);
    out.set_detail("invalid snapshot storage or capture bounds");
    return;
  }
  out.set_root_type(in.root_type);
  out.set_descriptor_set(in.descriptor, in.descriptor_size);
  if (in.payload_size)
    out.set_payload(in.payload, in.payload_size);
  for (std::size_t i = 0; i < in.container_role_count; ++i)
    out.add_container_roles(in.container_roles[i]);
  for (std::size_t i = 0; i < in.record_type_count; ++i)
    out.add_record_types(in.record_types[i]);
  for (std::size_t i = 0; i < in.member_count; ++i) {
    const auto &member = in.members[i];
    if (!member.name || !member.snapshot)
      continue;
    auto &encoded = *out.add_members();
    encoded.set_name(member.name);
    copy_value_snapshot(*member.snapshot, *encoded.mutable_snapshot(), member_depth - 1);
  }
}

clangtracer::v1::Value to_proto_value(const tracer::TraceValue &v) {
  clangtracer::v1::Value out;
  out.set_type(static_cast<clangtracer::v1::ValueType>(v.type));
  if (v.snapshot)
    copy_value_snapshot(*v.snapshot, *out.mutable_snapshot(), tracer::snapshot_depth_request().load());
  switch (v.type) {
  case tracer::ValueType::Int32:
  case tracer::ValueType::Bool:
  case tracer::ValueType::Int64:
    out.set_signed_value(v.signed_value);
    break;
  case tracer::ValueType::UInt64:
  case tracer::ValueType::Object:
  case tracer::ValueType::Pointer:
    out.set_unsigned_value(v.unsigned_value);
    break;
  case tracer::ValueType::Float32:
  case tracer::ValueType::Float64:
    out.set_real_value(v.real_value);
    break;
  default:
    break;
  }
  return out;
}

extern "C" void tracer_probe_enter(unsigned long long id) noexcept {
  if (auto *p = find_probe(id)) {
    p->in_flight.fetch_add(1, std::memory_order_seq_cst);
  }
}

extern "C" void tracer_probe_leave(unsigned long long id) noexcept {
  if (auto *p = find_probe(id)) {
    p->in_flight.fetch_sub(1, std::memory_order_seq_cst);
  }
}

extern "C" unsigned long long tracer_observer_clock() noexcept {
  if (callback_depth || !tracer::agent_has_observers())
    return 0;
  const int saved_errno = errno;
  timespec now{};
  const bool valid = ::clock_gettime(CLOCK_MONOTONIC, &now) == 0;
  errno = saved_errno;
  return valid ? static_cast<std::uint64_t>(now.tv_sec) * 1000000000ULL + now.tv_nsec : 0;
}

struct ObservedReturn {
  Probe& probe;
  std::uint64_t call;
  std::uint64_t timestamp;
};

bool publish_plugin_latency(void* context, const std::string& plugin,
                            tracer::agent::LatencyEvent latency) noexcept {
  const auto& call = *static_cast<const ObservedReturn*>(context);
  auto& p = call.probe;
  if (latency.probe_id != p.request.probe_id)
    return false;
  const auto sequence = p.sequence.fetch_add(1, std::memory_order_relaxed) + 1;
  auto& transport = agent.load(std::memory_order_acquire)->transport;
  auto* packet = transport.try_acquire(p.delivery);
  if (!packet)
    return false;
  bool valid = false;
  try {
    clangtracer::v1::TraceEvent event;
    event.set_protocol_version(tracer::kProtocolVersion);
    event.set_session_id(p.session_id);
    event.set_probe_id(latency.probe_id);
    event.set_call_id(call.call);
    event.set_pid(static_cast<std::uint32_t>(::getpid()));
    event.set_tid(static_cast<std::uint32_t>(::syscall(SYS_gettid)));
    event.set_phase(clangtracer::v1::AGENT_OBSERVER);
    event.set_probe_sequence(sequence);
    event.set_monotonic_time_ns(call.timestamp);
    event.set_agent_dropped_events(p.delivery.dropped.load(std::memory_order_relaxed));
    auto& payload = *event.mutable_agent_latency();
    payload.set_plugin_id(plugin);
    payload.set_elapsed_ns(latency.elapsed_ns);
    const auto size = event.ByteSizeLong();
    packet->bytes.resize(size);
    valid = event.SerializeToArray(packet->bytes.data(), static_cast<int>(size));
  } catch (...) {
    // Unblock subsequent packets if serialization fails.
  }
  if (!valid)
    p.delivery.dropped.fetch_add(1, std::memory_order_relaxed);
  transport.publish(*packet, valid);
  return valid;
}

extern "C" void tracer_observer_return(unsigned long long id, unsigned long long call,
                                       unsigned long long start, unsigned long long end) noexcept {
  if (callback_depth || end < start || !tracer::agent_has_observers())
    return;
  auto* p = find_probe(id);
  if (!p || !p->callbacks_active() || p->delivery.socket_fd < 0)
    return;
  const int saved_errno = errno;
  ++callback_depth;
  ObservedReturn invocation{*p, call, end};
  tracer::agent_notify_return(p->session_id, {id, end - start}, &invocation,
                              publish_plugin_latency);
  --callback_depth;
  errno = saved_errno;
}

extern "C" unsigned long long
tracer_next_call_id(unsigned long long id) noexcept {
  if (auto *p = find_probe(id)) {
    return p->call_ids.fetch_add(1, std::memory_order_relaxed) + 1;
  }
  return 0;
}

extern "C" bool tracer_object_identity_lookup(
    unsigned long long watch_id, unsigned long long receiver,
    std::uint64_t *object_id, std::uint64_t *generation,
    bool *identity_verified) noexcept {
  if (!object_id || !generation || !identity_verified)
    return false;
  const int saved_errno = errno;
  const auto identity = tracer::object_cache::identity::lookup(watch_id, receiver);
  *object_id = identity ? identity->object_id : 0;
  *generation = identity ? identity->generation : 0;
  *identity_verified = *object_id != 0;
  errno = saved_errno;
  return identity.has_value();
}

extern "C" long long tracer_callback_dispatch(unsigned long long id,
                                              const tracer::TraceContext *ctx,
                                              void *instance) {
  auto *p = find_probe(id);
  if (!p || !ctx || !p->callbacks_active() ||
      !p->callback_enabled.load(std::memory_order_relaxed) || callback_depth) {
    return 0;
  }
  const int saved_errno = errno;
  long long value = 0;
  bool succeeded = false;
  bool callback_failed = false;
  tracer::TraceContext invocation = *ctx;
  invocation.stack_storage = nullptr;
  std::optional<tracer::ScalarOverrides> staged;
  if (ctx->scalar_overrides) {
    staged.emplace();
    staged->allow_arguments = ctx->scalar_overrides->allow_arguments;
    staged->allow_result = ctx->scalar_overrides->allow_result;
    staged->allow_replacement = ctx->scalar_overrides->allow_replacement;
    invocation.scalar_overrides = &*staged;
  }
  tracer::detail::NamedValueStorage named;
  invocation.named_storage = &named;
  invocation.named_values = nullptr;
  invocation.named_count = 0;
  invocation.append_named = &tracer::detail::append_named_value;
  for (unsigned i = 0; i < invocation.argument_count; ++i) {
    invocation.argument_names[i] =
        i < p->argument_names.size() ? p->argument_names[i].c_str() : nullptr;
  }
  ++callback_depth;
  struct DispatchGuard {
    tracer::TraceContext &context;
    tracer::detail::NamedValueStorage& named;
    int saved_errno;
    bool depth_active = true;
    ~DispatchGuard() {
      if (!depth_active) {
        ++callback_depth;
      }
      named.clear();
      tracer::release_native_stack(context);
      --callback_depth;
      errno = saved_errno;
    }
  } dispatch_guard{invocation, named, saved_errno};

  std::vector<std::uint64_t> staged_arguments;
  std::vector<unsigned char> staged_set;
  log_dispatch(*p, invocation);
  try {
    if (staged) {
      staged_arguments.resize(ctx->argument_count);
      staged_set.resize(ctx->argument_count);
      staged->arguments = staged_arguments.data();
      staged->argument_set = staged_set.data();
    }
    value = p->callback(&invocation, instance, &succeeded);
    // The bridge reports whether a hook exists for this phase and ran.
    if (succeeded)
      log_hook(*p, invocation, true);
  }
#if defined(__GLIBCXX__)
  catch (const __cxxabiv1::__forced_unwind &) { throw; }
#endif
  catch (...) {
    callback_failed = true;
    succeeded = false;
    log_hook(*p, invocation, false);
    // Class adapters disable only their failed invocation. Legacy callbacks
    // retain their existing probe-wide failure behavior.
    if (!p->class_callback)
      p->callback_enabled.store(false, std::memory_order_relaxed);
    p->callback_errors.fetch_add(1, std::memory_order_relaxed);
  }
  if (succeeded) {
    if (staged) {
      auto* values = ctx->scalar_overrides->arguments;
      auto* set = ctx->scalar_overrides->argument_set;
      std::copy(staged_arguments.begin(), staged_arguments.end(), values);
      std::copy(staged_set.begin(), staged_set.end(), set);
      *ctx->scalar_overrides = *staged;
      ctx->scalar_overrides->arguments = values;
      ctx->scalar_overrides->argument_set = set;
    }
  }
  std::vector<std::uint64_t>().swap(staged_arguments);
  std::vector<unsigned char>().swap(staged_set);
  --callback_depth;
  dispatch_guard.depth_active = false;
  if (succeeded) {
    tracer_event_dispatch(id, &invocation, value);
  } else if (callback_failed) {
    // Publish a bounded diagnostic after leaving callback admission so the
    // target's original invocation can continue unchanged.
    tracer_event_dispatch_impl(id, &invocation, 0, true, "callback threw");
  }
  return value;
}

extern "C" bool
tracer_callback_lifecycle_enter(unsigned long long id) noexcept {
  auto *p = find_probe(id);
  if (callback_depth || !p || !p->callbacks_active() ||
      !p->callback_enabled.load(std::memory_order_relaxed))
    return false;
  ++callback_depth;
  return true;
}
extern "C" void tracer_callback_cleanup_enter() noexcept { ++callback_depth; }
extern "C" void tracer_callback_lifecycle_leave() noexcept { --callback_depth; }
extern "C" void tracer_callback_failure(
    unsigned long long id, const tracer::TraceContext *ctx,
    const char *message) noexcept {
  if (auto *p = find_probe(id)) {
    p->callback_errors.fetch_add(1, std::memory_order_relaxed);
    tracer_event_dispatch_impl(id, ctx, 0, true, message);
  }
}

extern "C" void tracer_capture_stack(const tracer::TraceContext *ctx,
                                     unsigned depth,
                                     bool application_only) noexcept {
  if (!ctx || (ctx->stack.requested_depth == depth &&
               ctx->stack.application_only == application_only &&
               (ctx->stack.status == tracer::StackStatus::Complete ||
                ctx->stack.status == tracer::StackStatus::Truncated)))
    return;
  const int saved_errno = errno;
  ++callback_depth;
  tracer::capture_native_stack(*ctx, depth, application_only);
  --callback_depth;
  errno = saved_errno;
}

extern "C" void tracer_capture_entry_stack(const tracer::TraceContext *ctx,
                                           unsigned depth) noexcept {
  if (!callback_depth)
    tracer_capture_stack(ctx, depth, ctx->stack_application_only);
}


static void tracer_event_dispatch_impl(unsigned long long id,
                                       const tracer::TraceContext *ctx,
                                       long long cb, bool callback_error,
                                       const char *callback_error_message) noexcept {
  const int saved_errno = errno;
  auto *p = find_probe(id);
  if (callback_depth || !p || !ctx ||
      !p->callbacks_active() || p->delivery.socket_fd < 0) {
    errno = saved_errno;
    return;
  }
  ++callback_depth;
  const auto sequence = p->sequence.fetch_add(1, std::memory_order_relaxed) + 1;
  auto &transport = agent.load(std::memory_order_acquire)->transport;
  auto *packet = transport.try_acquire(p->delivery);
  if (!packet) {
    --callback_depth;
    errno = saved_errno;
    return;
  }
  bool valid = false;
  try {
    clangtracer::v1::TraceEvent event;
    event.set_protocol_version(tracer::kProtocolVersion);
    event.set_session_id(p->session_id);
    event.set_probe_id(id);
    event.set_phase(static_cast<clangtracer::v1::TracePhase>(ctx->phase));
    event.set_call_id(ctx->call_id);
    event.set_pid(static_cast<std::uint32_t>(::getpid()));
    event.set_tid(static_cast<std::uint32_t>(::syscall(SYS_gettid)));
    event.set_probe_sequence(sequence);
    timespec now{};
    if (ctx->phase == tracer::TracePhase::Pre && ctx->original_entry_time_ns) {
      event.set_monotonic_time_ns(ctx->original_entry_time_ns);
    } else if (::clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
      event.set_monotonic_time_ns(
          static_cast<std::uint64_t>(now.tv_sec) * 1000000000ULL + now.tv_nsec);
    }
    event.set_object_address(ctx->object_address);
    if (ctx->receiver_snapshot) {
      tracer::TraceValue receiver;
      receiver.snapshot = ctx->receiver_snapshot;
      *event.mutable_receiver_snapshot() = to_proto_value(receiver).snapshot();
      event.set_receiver_sampled_at_return(
          ctx->phase == tracer::TracePhase::Post && p->receiver_sampled_at_return);
    }
    for (std::uint32_t i = 0; i < ctx->argument_count; ++i) {
      *event.add_arguments() = to_proto_value(ctx->arguments[i]);
    }
    if (ctx->named_count && !ctx->named_values) {
      throw std::invalid_argument("invalid named value count");
    }
    for (std::uint32_t i = 0; i < ctx->named_count; ++i) {
      const auto& value = ctx->named_values[i];
      const auto length = value.name_size;
      if (length == 0 || (value.kind != 1 && value.kind != 2)) {
        throw std::invalid_argument("invalid named value");
      }
      auto *named = value.kind == 1 ? event.add_named_arguments()
                                    : event.add_object_fields();
      named->set_name(value.name, length);
      named->set_argument_index(value.argument_index);
      *named->mutable_value() = to_proto_value(value.value);
    }
    for (unsigned i = 0;
         i < p->argument_names.size() && i < ctx->argument_count; ++i) {
      bool captured = false;
      for (const auto &value : event.named_arguments())
        if (value.argument_index() == i)
          captured = true;
      if (!captured) {
        auto *named = event.add_named_arguments();
        named->set_name(p->argument_names[i]);
        named->set_argument_index(i);
        *named->mutable_value() = to_proto_value(ctx->arguments[i]);
      }
    }
    if (!callback_error)
      event.set_callback_value(cb);
    event.set_callback_error(callback_error);
    if (callback_error && callback_error_message)
      event.set_callback_error_message(callback_error_message);
    if (ctx->phase == tracer::TracePhase::Post) {
      *event.mutable_result() = to_proto_value(ctx->result);
    }
    event.set_agent_dropped_events(
        p->delivery.dropped.load(std::memory_order_relaxed));
    // Large events use the transport's sealed-file path; snapshot budgets
    // are enforced at capture, not by an unrelated inline datagram ceiling.
    auto size = event.ByteSizeLong();
    if (ctx->stack.status != tracer::StackStatus::Disabled) {
      tracer::serialize_native_stack(ctx->stack, *event.mutable_stack());
      size = event.ByteSizeLong();
    }
    if (size <= static_cast<std::size_t>(std::numeric_limits<int>::max())) {
      packet->bytes.resize(size);
      valid =
          event.SerializeToArray(packet->bytes.data(), static_cast<int>(size));
    }
    if (!valid)
      p->delivery.dropped.fetch_add(1, std::memory_order_relaxed);
  } catch (...) {
    p->delivery.dropped.fetch_add(1, std::memory_order_relaxed);
  }
  // Publish even after an exception, so no abandoned reservation stalls FIFO.
  transport.publish(*packet, valid);
  --callback_depth;
  errno = saved_errno;
}

extern "C" void tracer_event_dispatch(unsigned long long id,
                                      const tracer::TraceContext *ctx,
                                      long long cb) noexcept {
  tracer_event_dispatch_impl(id, ctx, cb, false, nullptr);
}

bool validate_request(const tracer::Request &r, std::string &error) {
  if (r.version != tracer::kBootstrapVersion) {
    error = "unsupported request protocol";
    return false;
  }
  if (!r.probe_id) {
    error = "probe_id must be nonzero";
    return false;
  }
  if (r.kind != tracer::CallableKind::Function &&
      r.kind != tracer::CallableKind::Method) {
    error = "unsupported callable kind";
    return false;
  }
  if (r.object_watch_id && r.kind != tracer::CallableKind::Method) {
    error = "object-watch identity requires a non-static method";
    return false;
  }
  if (!valid_type(r.result, true)) {
    error = "unsupported result type";
    return false;
  }
  if (!std::memchr(r.session_id, '\0', sizeof(r.session_id)) ||
      !r.session_id[0] || !std::memchr(r.symbol, '\0', sizeof(r.symbol)) ||
      !r.symbol[0] ||
      !std::memchr(r.source_path, '\0', sizeof(r.source_path)) ||
      !r.source_path[0] ||
      !std::memchr(r.event_socket, '\0', sizeof(r.event_socket)) ||
      !r.event_socket[0]) {
    error = "session, symbol, source path and event socket are required";
    return false;
  }
  return true;
}
} // namespace

bool tracer::detail::tracer_enqueue_log(const TraceContext *context,
                                       const char *message, std::size_t size,
                                       LogLevel level) noexcept {
  auto *owner = ::agent.load(std::memory_order_acquire);
  if (!owner || !owner->logging_enabled || !context || !message || size == 0)
    return false;
  // Messages above the configured level are filtered before they use a slot.
  const auto record_level = level == tracer::LogLevel::Error  ? Level::Error
                            : level == tracer::LogLevel::Warning ? Level::Warning
                                                                 : Level::Info;
  if (!owner->log_allows(record_level))
    return false;
  auto *probe = find_probe(context->probe_id);
  return probe && enqueue_agent_log(*probe, *context, record_level, "callback",
                                    log_level_name(level), message, size);
}

extern "C" {
// The server reads this word without stopping or executing code in the target.
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(sizeof(std::atomic<std::uint64_t>) == sizeof(std::uint64_t));
std::atomic<std::uint64_t> tracer_agent_capacity_v1{tracer::kAgentCapacityTag};
}

extern "C" int tracer_agent_start(tracer::Request *request) {
  AgentOperation operation("prepare", request);
  try {
    tracer::context_scheduler::ScopedPthreadBypass compiler_threads;
    if (!request || request->version != tracer::kBootstrapVersion) {
      return operation.finish(-1);
    }
    operation.progress("validate_request");
    request->error[0] = '\0';
    std::string error;
    if (!validate_request(*request, error)) {
      set_error(request, error.c_str());
      return operation.finish(-1);
    }
    operation.enter("initialize_agent");
    std::lock_guard<std::mutex> lock(agent_mutex);
    Agent *current = agent.load(std::memory_order_acquire);
    if (!current) {
      // Keep the agent alive until process exit: a late wrapper entrant may
      // still reference a logically detached probe.
      current = std::make_unique<Agent>().release();
      agent.store(current, std::memory_order_release);
    }
    Agent &a = *current;
    if (a.pid && a.pid != ::getpid()) {
      set_error(request, "agent session pid mismatch");
      return operation.finish(-1);
    }
    if (find_probe(request->probe_id)) {
      set_error(request, "probe_id has already been used in this process");
      return operation.finish(-1);
    }
    // Allocate ownership storage before enabling transport or scheduler state.
    // Readers traverse stable registry nodes, never the ownership vectors.
    if (a.probes.size() == a.probes.capacity())
      a.probes.reserve(a.probes.empty() ? 1 : a.probes.size() * 2);
    if (a.probe_slots.size() == a.probe_slots.capacity())
      a.probe_slots.reserve(a.probe_slots.empty() ? 1 : a.probe_slots.size() * 2);
    auto slot = std::make_unique<ProbeSlot>();
    // A durable collector can reconnect while old probes are still active.
    // Keep their shared metadata alive; refresh a new session only after the
    // previous callback cohort has drained.
    bool native_cache_quiescent = true;
    for (std::size_t i = 0; i < a.probes.size(); ++i) {
      const auto *prior = a.probes[i].get();
      if (prior &&
          (prior->active.load(std::memory_order_seq_cst) ||
           prior->invocations_in_flight() ||
           prior->delivery.pending.load(std::memory_order_acquire) ||
           prior->transport_active)) {
        native_cache_quiescent = false;
        break;
      }
    }
    if (native_cache_quiescent)
      tracer::begin_native_stack_session(request->session_id);
    auto p = std::make_unique<Probe>();
    p->request = *request;
    clangtracer::v1::ProbeSpec specification;
    operation.enter("read_probe_spec");
    if (!read_probe_spec(p->request, p->signature_arguments, specification, error)) {
      set_error(request, error.c_str());
      return operation.finish(-1);
    }
    if (const auto *invalid = tracer::scalar_override_error(specification)) {
      set_error(request, invalid);
      return operation.finish(-1);
    }
    // A prepared constructor probe samples its receiver when the constructor returns.
    p->receiver_sampled_at_return =
        specification.snapshot_receiver_at_return() ||
        (tracer::client::is_constructor_symbol(specification.symbol()) &&
         tracer::prepared::is_typed_version(specification.prepared().format_version()));
    p->object_snapshot = specification.snapshot_report_unavailable();
    // Named tracing uses the target's own independently re-read DWARF and ELF
    // mappings. No dynamic symbol lookup is involved, including for local and
    // hidden functions. Raw-symbol requests retain their explicit dlsym path.
    operation.enter("resolve_symbol");
    std::optional<VerifiedDebugEntry> verified_debug_entry;
    void *entry = nullptr;
    if (specification.has_debug_info()) {
      const auto runtime_entry = validate_debug_entry(specification);
      verified_debug_entry = VerifiedDebugEntry{&specification, runtime_entry};
      entry = reinterpret_cast<void *>(runtime_entry);
    } else {
      entry = ::dlsym(RTLD_DEFAULT, request->symbol);
    }
    if (!entry) {
      set_error(request, "target symbol was not found in the process");
      return operation.finish(-1);
    }
    operation.detail("entry=0x%llx source=%s symbol=%.160s",
                     static_cast<unsigned long long>(
                         reinterpret_cast<std::uintptr_t>(entry)),
                     specification.has_debug_info() ? "debug_info" : "dlsym",
                     request->symbol);
    operation.enter("validate_patch");
    p->request.original_entry = reinterpret_cast<std::uintptr_t>(entry);
    p->request.patch_address = p->request.original_entry;
    std::strcpy(p->request.patch_reason, "verified_entry_window");
    int prot = 0;
    std::uintptr_t map_lo = 0, map_hi = 0;
    if (!mapping(entry, map_lo, map_hi, prot) ||
        (prot & (PROT_READ | PROT_EXEC)) != (PROT_READ | PROT_EXEC)) {
      set_error(request, "target symbol is not in a readable executable mapping");
      return operation.finish(-1);
    }
    Probe *replaced = nullptr;
    bool entry_matches_original = true;
    for (std::size_t i = 0; i < a.probes.size(); ++i) {
      auto *other = a.probes[i].get();
      if (other && other->active.load() &&
          other->request.original_entry == p->request.original_entry) {
        const auto &prior = other->request;
        if (prior.patch_backend != tracer::NativePatch::Entry) {
          set_error(request, "target already has an active call-site or trap probe");
          return operation.finish(-1);
        }
        if (prior.patch_size > map_hi - p->request.original_entry) {
          set_error(request,
                    "target symbol address is already hooked by another probe");
          return operation.finish(-1);
        }
        if (std::memcmp(entry, prior.installed, prior.patch_size) == 0) {
          // A retired wrapper from a previous session can remain resident
          // after its entry is restored. Only the live detour has an owner.
          if (request->reject_replacement || replaced || other->session_id != request->session_id) {
            set_error(request, "target symbol address is already hooked by another probe");
            return operation.finish(-1);
          }
          replaced = other;
        }
        entry_matches_original &= std::memcmp(entry, prior.original, prior.patch_size) == 0;
      }
    }
    if (request->expected_replaced_probe_id &&
        (!replaced || replaced->request.probe_id != request->expected_replaced_probe_id)) {
      set_error(request, "reload probe no longer owns the target entry");
      return operation.finish(-1);
    }
    if (!replaced && !entry_matches_original) {
      set_error(request, "live entry does not match any retained probe generation");
      return operation.finish(-1);
    }
    tracer::native_detour::Patch target_patch;
    bool entry_decoded = true;
    if (replaced) {
      // The live entry contains our jump. Decode the saved prologue at its
      // original address so RIP-relative operands keep their original targets.
      p->request.replaced_probe_id = replaced->request.probe_id;
      p->request.patch_size = replaced->request.patch_size;
      std::memcpy(p->request.original, replaced->request.original,
                  p->request.patch_size);
      if (!tracer::native_detour::decode(
              {p->request.original, p->request.patch_size},
              p->request.original_entry, target_patch, error,
              replaced->entry_jump_size)) {
        set_error(request, error.c_str());
        return operation.finish(-1);
      }
    } else {
      entry_decoded = decode_patch(static_cast<const std::uint8_t *>(entry), p->request,
                                  target_patch, error,
                                  specification.debug_info().entry_size());
    }
    if (entry_decoded && specification.has_debug_info() &&
        p->request.patch_size > specification.debug_info().entry_size()) {
      set_error(request, "safe patch window exceeds the verified function range");
      return operation.finish(-1);
    }
    operation.detail(
        "entry=0x%llx mapping=0x%llx-0x%llx prot=%c%c%c patch_address=0x%llx "
        "patch_size=%u backend=%s reason=%s replaced_probe=%llu decoded=%d",
        static_cast<unsigned long long>(p->request.original_entry),
        static_cast<unsigned long long>(map_lo),
        static_cast<unsigned long long>(map_hi), prot & PROT_READ ? 'r' : '-',
        prot & PROT_WRITE ? 'w' : '-', prot & PROT_EXEC ? 'x' : '-',
        static_cast<unsigned long long>(p->request.patch_address),
        p->request.patch_size, patch_backend_name(p->request.patch_backend),
        p->request.patch_reason,
        static_cast<unsigned long long>(p->request.replaced_probe_id),
        entry_decoded ? 1 : 0);
    operation.enter("create_trampoline");
    bool getter_fallback = false;
    if (!entry_decoded || !tracer::native_detour::create_trampoline(
                              target_patch, p->trampoline, p->mapping_size, error)) {
      if (replaced || specification.native_backend() != "auto" ||
          !prepare_getter_fallback(*p, specification)) {
        set_error(request, error.c_str());
        return operation.finish(-1);
      }
      error.clear();
      getter_fallback = true;
    }
    for (std::size_t i = 0; i < a.probes.size(); ++i) {
      auto *other = a.probes[i].get();
      if (!other || !other->active.load()) {
        continue;
      }
      const auto &prior = other->request;
      const auto x = prior.patch_address, y = p->request.patch_address;
      const auto xe = x + prior.patch_size, ye = y + p->request.patch_size;
      const auto verified_start = p->request.original_entry;
      const auto other_verified_start = prior.original_entry;
      const bool overlaps = (x < ye && y < xe) ||
          (p->request.verified_size && x < verified_start + p->request.verified_size &&
           verified_start < xe) ||
          (prior.verified_size && y < other_verified_start + prior.verified_size &&
           other_verified_start < ye);
      if (!overlaps)
        continue;
      // Older observers can outlive the detour that routed calls to them.
      if (replaced && prior.patch_backend == tracer::NativePatch::Entry &&
          prior.original_entry == p->request.original_entry &&
          prior.patch_address == p->request.patch_address &&
          prior.patch_size == p->request.patch_size &&
          std::memcmp(prior.original, p->request.original, prior.patch_size) == 0)
        continue;
      const auto *previous_entry = reinterpret_cast<const void *>(x);
      std::uintptr_t previous_lo = 0, previous_hi = 0;
      int previous_protection = 0;
      if (mapping(previous_entry, previous_lo, previous_hi, previous_protection) &&
          (previous_protection & (PROT_READ | PROT_EXEC)) == (PROT_READ | PROT_EXEC) &&
          prior.patch_size <= previous_hi - x &&
          std::memcmp(previous_entry, prior.original, prior.patch_size) == 0)
        continue;
      set_error(request, "target patch overlaps an already hooked function");
      return operation.finish(-1);
    }
    operation.detail(
        "trampoline=0x%llx mapping_size=%zu getter_fallback=%d backend=%s "
        "patch_address=0x%llx patch_size=%u reason=%s",
        static_cast<unsigned long long>(
            reinterpret_cast<std::uintptr_t>(p->trampoline)),
        p->mapping_size, getter_fallback ? 1 : 0,
        patch_backend_name(p->request.patch_backend),
        static_cast<unsigned long long>(p->request.patch_address),
        p->request.patch_size, p->request.patch_reason);
    operation.progress("compile_callback");
    const auto compile_started = std::chrono::steady_clock::now();
    if (!compile_probe(
            *p, specification, error, true, false, nullptr,
            verified_debug_entry ? &*verified_debug_entry : nullptr)) {
      set_error(request, error.c_str());
      return operation.finish(-1);
    }
    if (p->request.patch_backend == tracer::NativePatch::CallSite) {
      if (!tracer::native_detour::prepare_call_relay(
              p->request.patch_address, p->trampoline, p->mapping_size, p->wrapper,
              {p->request.installed, p->request.patch_size}, p->request.relay_address, error)) {
        set_error(request, error.c_str());
        return operation.finish(-1);
      }
    } else if (p->request.patch_backend == tracer::NativePatch::Entry) {
      const void *detour_destination = p->wrapper;
      if (specification.has_debug_info() && specification.debug_info().callable_is_variadic()) {
        if (!tracer::native_detour::create_variadic_forwarder(
                p->trampoline, p->mapping_size, p->wrapper, detour_destination, error)) {
          set_error(request, error.c_str());
          return operation.finish(-1);
        }
        p->variadic_forwarder = true;
      }
      if (!tracer::native_detour::prepare_entry_jump(
              target_patch, p->trampoline, p->mapping_size, detour_destination,
              {p->request.installed, p->request.patch_size}, error)) {
        set_error(request, error.c_str());
        return operation.finish(-1);
      }
      p->entry_jump_size = target_patch.jump_size;
    }
    p->request.jit_compile_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - compile_started).count();
    if (specification.reclaimable() && p->request.context_required) {
      set_error(request, "reclaimable observers cannot schedule shared-context work");
      return operation.finish(-1);
    }
    p->reclaimable = specification.reclaimable();
    operation.detail("wrapper=0x%llx relay=0x%llx jit_compile_ns=%llu",
                     static_cast<unsigned long long>(
                         reinterpret_cast<std::uintptr_t>(p->wrapper)),
                     static_cast<unsigned long long>(p->request.relay_address),
                     static_cast<unsigned long long>(p->request.jit_compile_ns));
    operation.enter("prepare_context");
    if (p->request.context_required && !prepare_context_pthread(a, *p, error)) {
      set_error(request, error.c_str());
      return operation.finish(-1);
    }
    operation.enter("create_event_socket");
    p->delivery.socket_fd =
        ::socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (p->delivery.socket_fd < 0) {
      const int error_number = errno;
      set_error(request, "could not create event socket");
      return operation.finish(-1, error_number);
    }
    operation.enter("connect_event_socket");
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (std::strlen(request->event_socket) >= sizeof(addr.sun_path)) {
      set_error(request, "event socket path is too long");
      return operation.finish(-1);
    }
    std::strncpy(addr.sun_path, request->event_socket,
                 sizeof(addr.sun_path) - 1);
    if (::connect(p->delivery.socket_fd, reinterpret_cast<sockaddr *>(&addr),
                  sizeof(addr))) {
      const int error_number = errno;
      set_error(request, "could not connect event socket");
      return operation.finish(-1, error_number);
    }
    operation.detail("socket=%s", request->event_socket);
    operation.enter("enable_context");
    if (p->request.context_required) {
      if (!a.pthread_detour || !a.pthread_detour->enable_propagation()) {
        set_error(request,
                  "native pthread_create propagation adapter is unavailable");
        return operation.finish(-1);
      }
      p->scheduler_counted = true;
      ++a.context_probe_count;
    }
    operation.enter("publish_probe");
    p->session_id = request->session_id;
    a.pid = ::getpid();
    a.transport.activate();
    p->transport_active = true;
    p->active.store(true, std::memory_order_seq_cst);
    Probe *installed_probe = p.get();
    slot->next = a.published_slots.load(std::memory_order_relaxed);
    slot->probe.store(installed_probe, std::memory_order_relaxed);
    slot->id.store(installed_probe->request.probe_id, std::memory_order_relaxed);
    auto *published = slot.get();
    a.probes.push_back(std::move(p));
    a.probe_slots.push_back(std::move(slot));
    a.published_slots.store(published, std::memory_order_release);
    ++a.retained;
    tracer_agent_capacity_v1.store(tracer::kAgentCapacityTag |
                                      std::min<std::size_t>(a.retained, UINT32_MAX),
                                  std::memory_order_relaxed);
    operation.detail("slot=%zu retained=%zu session=%s", a.retained - 1, a.retained,
                     installed_probe->session_id.c_str());
    if (agent_log_allows(Level::Trace)) {
      const auto &published = installed_probe->request;
      const auto dump = [&](const char *name, const std::uint8_t *bytes) {
        // Each record carries at most 32 bytes so a long patch is never cut.
        for (std::size_t offset = 0; offset < published.patch_size;
             offset += 32) {
          const auto count =
              std::min<std::size_t>(32, published.patch_size - offset);
          char hex[32 * 3 + 1];
          format_hex(hex, bytes + offset, count);
          if (offset)
            operation.note(Level::Trace, "bytes", "%s+%zu=%s", name, offset, hex);
          else
            operation.note(Level::Trace, "bytes", "%s=%s", name, hex);
        }
      };
      if (published.patch_size <= tracer::kMaxPatchBytes) {
        dump("original", published.original);
        dump("installed", published.installed);
      }
    }
    *request = installed_probe->request;
    return operation.finish(0);
  } catch (const std::exception &e) {
    set_error(request, e.what());
    return operation.finish(-1);
  } catch (...) {
    set_error(request, "unexpected exception while adding probe");
    return operation.finish(-1);
  }
}

extern "C" int tracer_agent_reclaim_v1(std::uint64_t id, TracerRetiredCode *code,
                                        unsigned release) {
  try {
    std::lock_guard<std::mutex> lock(agent_mutex);
    auto *owner = agent.load(std::memory_order_acquire);
    if (!owner || !code)
      return -1;
    for (std::size_t i = 0; i < owner->probes.size(); ++i) {
      auto *probe = owner->probes[i].get();
      if (!probe || probe->request.probe_id != id)
        continue;
      if (!probe->reclaimable || probe->active.load() || probe->transport_active ||
          probe->in_flight.load() != Probe::admission_closed ||
          probe->delivery.pending.load())
        return -1;
      if (!release) {
        const void *entries[] = {probe->wrapper,
            reinterpret_cast<const void *>(probe->callback), probe->trampoline};
        for (std::size_t j = 0; j < 3; ++j) {
          std::uintptr_t begin = 0, end = 0;
          int protection = 0;
          if (!entries[j] || !mapping(entries[j], begin, end, protection) ||
              !(protection & PROT_EXEC))
            return -1;
          code->ranges[j] = {begin, end};
        }
      } else {
        owner->probe_slots[i]->id.store(0, std::memory_order_release);
        owner->probe_slots[i]->probe.store(nullptr, std::memory_order_release);
        owner->probes[i].reset();
        --owner->retained;
        tracer_agent_capacity_v1.store(tracer::kAgentCapacityTag |
                                          std::min<std::size_t>(owner->retained, UINT32_MAX),
                                       std::memory_order_relaxed);
      }
      return 0;
    }
  } catch (...) {
  }
  return -1;
}

extern "C" int tracer_agent_validate(tracer::ValidationRequest *request) {
  tracer::context_scheduler::ScopedPthreadBypass compiler_threads;
  const auto set_error = [&](const std::string &message) {
    if (!request)
      return;
    std::strncpy(request->error, message.c_str(), sizeof(request->error) - 1);
    request->error[sizeof(request->error) - 1] = '\0';
  };
  try {
    if (!request || request->version != tracer::kBootstrapVersion ||
        request->level > 1) {
      set_error("invalid native validation request");
      return -1;
    }
    request->error[0] = '\0';
    const auto spec_length = ::strnlen(request->spec_path,
                                       sizeof(request->spec_path));
    const auto report_length = ::strnlen(request->report_path,
                                         sizeof(request->report_path));
    if (spec_length == 0 || spec_length == sizeof(request->spec_path) ||
        report_length == 0 || report_length == sizeof(request->report_path)) {
      set_error("validation spec/report path is missing or too long");
      return -1;
    }
    std::ifstream input(request->spec_path, std::ios::binary);
    if (!input) {
      set_error("cannot open serialized validation ProbeSpec");
      return -1;
    }
    clangtracer::v1::ProbeSpec spec;
    if (!spec.ParseFromIstream(&input) || !spec.has_signature() || spec.callback_source().empty() ||
        spec.symbol().empty() || !spec.has_debug_info()) {
      set_error("validation ProbeSpec is incomplete or malformed");
      return -1;
    }

    clangtracer::v1::ValidationReport report;
    report.set_level(static_cast<clangtracer::v1::ValidateProbeRequest::Level>(
        request->level));
    tracer::Request native_request{};
    native_request.version = tracer::kBootstrapVersion;
    native_request.probe_id = std::numeric_limits<std::uint64_t>::max();
    native_request.kind = static_cast<tracer::CallableKind>(
        spec.signature().kind());
    native_request.result = static_cast<tracer::ValueType>(
        spec.signature().result());
    native_request.argument_count = static_cast<std::uint32_t>(
        spec.signature().arguments_size());
    native_request.original_entry = spec.debug_info().runtime_entry();
    std::strncpy(native_request.symbol, spec.symbol().c_str(),
                 sizeof(native_request.symbol) - 1);
    std::strncpy(native_request.session_id, "validation",
                 sizeof(native_request.session_id) - 1);
    std::strncpy(native_request.source_path, request->spec_path,
                 sizeof(native_request.source_path) - 1);
    std::strncpy(native_request.event_socket, "/tmp/clang-tracer-validation",
                 sizeof(native_request.event_socket) - 1);
    std::vector<tracer::ValueType> signature_arguments;
    for (const auto type : spec.signature().arguments()) {
      signature_arguments.push_back(static_cast<tracer::ValueType>(type));
    }
    native_request.arguments = signature_arguments.data();
    if (tracer::prepared::is_typed_version(spec.prepared().format_version())) {
      for (const auto &option : spec.prepared().adapter_toolchain_options())
        if (::tracer::compat::starts_with(option, "--driver-mode="))
          report.add_effective_options(option);
      if (spec.has_header_context())
        for (const auto &option : spec.header_context().compiler_arguments())
          report.add_effective_options(option);
    }
    for (const auto &option : spec.compile_options())
      report.add_effective_options(option);
    for (const auto &option : spec.extra_compile_arguments())
      report.add_effective_options(option);

    std::string error;
    Probe prepared;
    prepared.request = native_request;
    prepared.trampoline = nullptr;
    prepared.session_id = "validation";
    const bool syntax_only = request->level == 1;
    bool success = false;
    {
      std::lock_guard<std::mutex> lock(agent_mutex);
      const auto listed_options = spec.compile_options_size();
      success = compile_probe(prepared, spec, error, false, syntax_only,
                              &report);
      // Options the automatic standard-header selection added (ABI macro, language mode).
      for (auto i = listed_options; i < spec.compile_options_size(); ++i)
        report.add_effective_options(spec.compile_options(i));
    }
    if (success && spec.extra_compile_arguments_checked())
      report.set_extra_compile_arguments_applied(true);
    if (!success && !error.empty()) {
      bool represented = false;
      for (const auto &diagnostic : report.diagnostics()) {
        if (diagnostic.severity() == "error" &&
            (diagnostic.message() == error ||
             error.find(diagnostic.message()) != std::string::npos)) {
          represented = true;
          break;
        }
      }
      if (!represented)
        append_failure(report, syntax_only ? "syntax" : "compile", error);
    }
    report.set_success(success);
    std::string report_bytes;
    if (!report.SerializeToString(&report_bytes) || report_bytes.size() > 1024 * 1024) {
      set_error("cannot serialize native validation report");
      return -1;
    }
    const std::string report_path(request->report_path, report_length);
    const std::string temporary_path =
        report_path + ".tmp." + std::to_string(::getpid()) + "." +
        std::to_string(validation_report_sequence.fetch_add(
            1, std::memory_order_relaxed));
    const int fd = ::open(temporary_path.c_str(),
                          O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
                          0600);
    if (fd < 0) {
      set_error("cannot create private validation report file");
      return -1;
    }
    bool write_ok = true;
    std::size_t written = 0;
    while (written < report_bytes.size()) {
      const auto count = ::write(fd, report_bytes.data() + written,
                                 report_bytes.size() - written);
      if (count < 0 && errno == EINTR)
        continue;
      if (count <= 0) {
        write_ok = false;
        break;
      }
      written += static_cast<std::size_t>(count);
    }
    if (write_ok && ::fsync(fd) != 0)
      write_ok = false;
    if (::close(fd) != 0)
      write_ok = false;
    if (!write_ok || ::rename(temporary_path.c_str(), report_path.c_str()) != 0) {
      ::unlink(temporary_path.c_str());
      set_error("cannot atomically publish native validation report");
      return -1;
    }
    return 0;
  } catch (const std::exception &exception) {
    set_error(exception.what());
    return -1;
  } catch (...) {
    set_error("unexpected failure during native validation");
    return -1;
  }
}

extern "C" int tracer_agent_stop(tracer::Request *request) {
  AgentOperation operation("stop", request);
  try {
    if (!request || request->version != tracer::kBootstrapVersion) {
      return operation.finish(-1);
    }
    std::lock_guard<std::mutex> lock(agent_mutex);
    auto *current = agent.load(std::memory_order_acquire);
    if (!current) {
      set_error(request, "agent is not initialized");
      return operation.finish(-1);
    }
    Agent &a = *current;
    auto *p = find_probe(request->probe_id);
    if (!p) {
      set_error(request, "probe_id is unknown");
      return operation.finish(-1);
    }
    if (std::strncmp(request->session_id, p->request.session_id,
                     sizeof(request->session_id)) != 0) {
      set_error(request, "probe session id does not match");
      return operation.finish(-1);
    }
    operation.enter("verify_original_patch");
    void *entry = reinterpret_cast<void *>(p->request.patch_address);
    std::uintptr_t lo = 0, hi = 0;
    int prot = 0;
    if (!mapping(entry, lo, hi, prot) ||
        p->request.patch_size > hi - reinterpret_cast<std::uintptr_t>(entry) ||
        (prot & (PROT_READ | PROT_EXEC)) != (PROT_READ | PROT_EXEC)) {
      set_error(request, "remove requires the original executable mapping");
      return operation.finish(-2);
    }
    const bool restored = std::memcmp(entry, p->request.original, p->request.patch_size) == 0;
    const bool superseded = !restored && std::any_of(
        a.probes.begin(), a.probes.end(), [&](const auto &other) {
      return other && other.get() != p && other->active.load() &&
             other->session_id == p->session_id &&
             other->request.patch_address == p->request.patch_address &&
             other->request.original_entry == p->request.original_entry &&
             other->request.patch_size == p->request.patch_size &&
             std::memcmp(other->request.original, p->request.original, p->request.patch_size) == 0 &&
             std::memcmp(entry, other->request.installed, p->request.patch_size) == 0;
    });
    if (!restored && !superseded) {
      set_error(request, "remove requires original bytes or a verified successor detour");
      return operation.finish(-2);
    }
    operation.detail("restored=%d superseded=%d patch_address=0x%llx patch_size=%u",
                     restored ? 1 : 0, superseded ? 1 : 0,
                     static_cast<unsigned long long>(p->request.patch_address),
                     p->request.patch_size);
    // Closing admission and testing for invocations must be one operation.
    // A delayed wrapper entrant after closure can still run its trampoline.
    std::uint64_t state = 0;
    if (!p->in_flight.compare_exchange_strong(state, Probe::admission_closed) &&
        !(state & Probe::admission_closed)) {
      *request = p->request;
      set_error(request, "probe invocations are in flight; remove can be retried");
      operation.retry();
      return 1;
    }
    operation.enter("drain");
    p->active.exchange(false, std::memory_order_seq_cst);
    if (p->scheduler_counted) {
      p->scheduler_counted = false;
      if (a.context_probe_count != 0)
        --a.context_probe_count;
      if (a.context_probe_count == 0 && a.pthread_detour)
        a.pthread_detour->disable_propagation();
    }
    const bool first_drain = tracer::EventTransport::begin_drain(p->delivery);
    const auto scheduler_tasks =
        a.pthread_detour ? a.pthread_detour->outstanding_tasks() : 0;
    p->request.pthread_create_active_tasks =
        static_cast<std::uint32_t>(std::min<std::size_t>(
            scheduler_tasks, std::numeric_limits<std::uint32_t>::max()));
    if (p->invocations_in_flight() ||
        p->delivery.pending.load(std::memory_order_acquire) ||
        (a.context_probe_count == 0 && scheduler_tasks != 0)) {
      p->request.event_drops =
          p->delivery.dropped.load(std::memory_order_relaxed);
      p->request.callback_errors =
          p->callback_errors.load(std::memory_order_relaxed);
      *request = p->request;
      set_error(request, "probe callbacks, accepted pthread tasks or queued "
                         "events are in flight; "
                         "remove can be retried");
      if (first_drain)
        operation.waiting(request->callback_errors, request->event_drops);
      operation.retry(request->callback_errors, request->event_drops);
      return 1;
    }
    if (p->transport_active) {
      agent.load(std::memory_order_acquire)->transport.deactivate();
      p->transport_active = false;
    }
    p->request.events_sent = p->delivery.sent.load(std::memory_order_relaxed);
    p->request.event_drops =
        p->delivery.dropped.load(std::memory_order_relaxed);
    p->request.callback_errors =
        p->callback_errors.load(std::memory_order_relaxed);
    *request = p->request;
    operation.detail(
        "invocations_in_flight=%llu events_pending=%llu events_sent=%llu "
        "event_drops=%llu callback_errors=%llu",
        static_cast<unsigned long long>(p->invocations_in_flight()),
        static_cast<unsigned long long>(
            p->delivery.pending.load(std::memory_order_acquire)),
        static_cast<unsigned long long>(request->events_sent),
        static_cast<unsigned long long>(request->event_drops),
        static_cast<unsigned long long>(request->callback_errors));
    return operation.finish(
        0, p->delivery.last_send_errno.load(std::memory_order_relaxed),
        request->callback_errors, request->event_drops);
  } catch (const std::exception &e) {
    set_error(request, e.what());
    return operation.finish(-1);
  } catch (...) {
    set_error(request, "unexpected exception while removing probe");
    return operation.finish(-1);
  }
}

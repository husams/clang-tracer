#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include <clang/AST/ASTImporter.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/Version.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Interpreter/Interpreter.h>
#include <clang/Lex/Lexer.h>
#include <clang/Parse/ParseAST.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <dlfcn.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCDisassembler/MCDisassembler.h>
#include <llvm/MC/MCInst.h>
#include <llvm/MC/MCInstrInfo.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/MC/MCTargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "callback_bridge.hpp"
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
#include "native_stack.hpp"
#include "runtime_identity.hpp"
#include "runtime_paths.hpp"
#include "tracer.pb.h"
#include "tracer/compat.hpp"
#include "tracer/compiler_options.hpp"
#include "tracer/protocol.hpp"
#include "tracer/receiver_abi.hpp"

#if !defined(__linux__) || !defined(__x86_64__)
#error "clang-tracer's native agent currently supports Linux x86_64 only"
#endif

namespace {
constexpr std::size_t kJumpBytes = 14;
constexpr std::size_t kMaxDecodedBytes = tracer::kMaxPatchBytes - kJumpBytes;
thread_local unsigned callback_depth = 0;

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
  tracer::Request request{};
  std::shared_ptr<OrcErrorCapture> orc_errors;
  std::unique_ptr<clang::Interpreter> interpreter;
  std::unique_ptr<clang::ASTUnit> receiver_ast;
  DebugRecords debug_records;
  void *trampoline = MAP_FAILED;
  std::size_t mapping_size = 0;
  std::atomic<std::uint64_t> sequence{0};
  tracer::EventDelivery delivery;
  std::atomic<std::uint64_t> callback_errors{0};
  std::atomic<std::uint64_t> call_ids{0};
  std::atomic<std::uint64_t> in_flight{0};
  std::atomic<bool> callback_enabled{true};
  std::atomic<bool> active{false};
  std::string session_id;
  long long (*callback)(const tracer::TraceContext *, void *, bool *) = nullptr;
  std::vector<std::string> argument_names;
  bool class_callback = false;
  bool has_pre = false;
  bool has_post = false;
  bool transport_active = false;
  bool scheduler_counted = false;
  void *wrapper = nullptr;
  std::vector<std::string> external_link_candidates;
  std::shared_ptr<const tracer::ContextActivationGeneration> shared_generation;
  std::shared_ptr<tracer::context_runtime::SharedSourceRuntime> shared_runtime;
  std::shared_ptr<tracer::context_delivery::ContextAgentDelivery>
      context_delivery;

  ~Probe() {
    if (delivery.socket_fd >= 0) {
      ::close(delivery.socket_fd);
    }
    if (trampoline != MAP_FAILED) {
      ::munmap(trampoline, mapping_size);
    }
  }
};

struct Agent {
  std::array<std::unique_ptr<Probe>, tracer::kMaxProbes> probes{};
  std::array<std::atomic<Probe *>, tracer::kMaxProbes> probe_slots{};
  std::size_t retained = 0;
  pid_t pid = 0;
  tracer::ContextRegistry context_registry;
  std::unique_ptr<tracer::context_engine::ContextCapturePool> context_capture;
  unsigned next_context_hook = 0;
  tracer::EventTransport transport{[] { ++callback_depth; }};
  std::shared_ptr<tracer::context_engine::PropagationLedger> propagation_ledger;
  std::unique_ptr<tracer::context_scheduler::ContextPthreadDetour>
      pthread_detour;
  void *pthread_trampoline = MAP_FAILED;
  std::size_t pthread_trampoline_size = 0;
  tracer::Request pthread_patch{};
  std::size_t context_probe_count = 0;

  Agent() {
    for (auto &slot : probe_slots) {
      slot.store(nullptr, std::memory_order_relaxed);
    }
  }
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
    const auto &source_manager = info.getSourceManager();
    const auto location = info.getLocation();
    if (location.isValid()) {
      const auto presumed = source_manager.getPresumedLoc(location);
      if (presumed.isValid()) {
        diagnostic.file = presumed.getFilename();
        diagnostic.line = presumed.getLine();
        diagnostic.column = presumed.getColumn();
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
  const std::vector<CapturedDiagnostic> &diagnostics() const {
    return diagnostics_;
  }

  void reset_capture() {
    message_.clear();
    diagnostics_.clear();
    had_error_ = false;
  }

private:
  clang::DiagnosticConsumer *forward_;
  std::string message_;
  std::vector<CapturedDiagnostic> diagnostics_;
  bool had_error_ = false;
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

void append_failure(clangtracer::v1::ValidationReport &report,
                    const char *stage, const std::string &message) {
  if (message.empty())
    return;
  auto *diagnostic = report.add_diagnostics();
  diagnostic->set_stage(stage);
  diagnostic->set_severity("error");
  diagnostic->set_message(message);
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
extern "C" unsigned long long tracer_next_call_id(unsigned long long) noexcept;
extern "C" void tracer_capture_stack(const tracer::TraceContext *, unsigned,
                                     bool) noexcept;
extern "C" void tracer_capture_entry_stack(const tracer::TraceContext *,
                                           unsigned) noexcept;
extern "C" long long tracer_callback_dispatch(unsigned long long,
                                              const tracer::TraceContext *,
                                              void *) noexcept;
extern "C" bool tracer_callback_lifecycle_enter(unsigned long long) noexcept;
extern "C" void tracer_callback_cleanup_enter() noexcept;
extern "C" void tracer_callback_lifecycle_leave() noexcept;
extern "C" void tracer_callback_failure(unsigned long long) noexcept;
extern "C" void tracer_event_dispatch(unsigned long long,
                                      const tracer::TraceContext *,
                                      long long) noexcept;
extern "C" void tracer_context_check_entry();
extern "C" void *tracer_shared_enter(unsigned long long,
                                     const tracer::TraceContext *);
extern "C" void tracer_shared_leave(void *) noexcept;
extern "C" void tracer_shared_release(void *) noexcept;
extern "C" void tracer_shared_dispatch(void *, const tracer::TraceContext *);
extern "C" void tracer_shared_original_enter(void *);
extern "C" void tracer_shared_original_leave(void *) noexcept;
extern "C" void tracer_shared_resume(void *);

std::mutex agent_mutex;
std::atomic<Agent *> agent{nullptr};
std::atomic<std::uint64_t> validation_report_sequence{0};

Probe *find_probe(std::uint64_t id) noexcept {
  Agent *current = agent.load(std::memory_order_acquire);
  if (!current) {
    return nullptr;
  }
  for (auto &slot : current->probe_slots) {
    Probe *probe = slot.load(std::memory_order_acquire);
    if (probe && probe->request.probe_id == id) {
      return probe;
    }
  }
  return nullptr;
}

struct SharedInvocationToken {
  Probe *probe = nullptr;
  tracer::TraceContext raw{};
  tracer::StackTrace entry_stack{};
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
std::array<SharedInvocationToken, 256> shared_invocations;
static_assert(sizeof(shared_invocations) <=
              tracer::context_engine::kCaptureInvocationWorkspaceBytes);

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
  for (const auto &token : shared_invocations) {
    if (&token.raw != context ||
        !token.occupied.load(std::memory_order_acquire))
      continue;
    copy_shared_value(context->stack, token.entry_stack);
    auto &stack = context->stack;
    const auto count = std::min<unsigned>(depth, token.entry_stack.count);
    stack.requested_depth = depth;
    stack.application_only = application_only;
    stack.captured_depth = count;
    stack.hidden_frames = 0;
    stack.count = 0;
    if (count < token.entry_stack.count) {
      stack.status = tracer::StackStatus::Truncated;
      constexpr char detail[] = "entry stack exceeds requested depth";
      for (std::size_t i = 0; i < sizeof(detail); ++i)
        stack.detail[i] = detail[i];
    }
    for (unsigned i = 0; i < count; ++i) {
      if (application_only && !token.entry_stack.frames[i].application_frame) {
        ++stack.hidden_frames;
        continue;
      }
      auto *frame = reinterpret_cast<volatile unsigned char *>(
          &stack.frames[stack.count++]);
      const auto *source = reinterpret_cast<const volatile unsigned char *>(
          &token.entry_stack.frames[i]);
      for (std::size_t j = 0; j < sizeof(tracer::StackFrame); ++j)
        frame[j] = source[j];
    }
    for (unsigned i = stack.count; i < tracer::kMaxStackDepth; ++i) {
      auto *frame =
          reinterpret_cast<volatile unsigned char *>(&stack.frames[i]);
      for (std::size_t j = 0; j < sizeof(tracer::StackFrame); ++j)
        frame[j] = 0;
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
                                     const tracer::TraceContext *raw) {
  auto *probe = find_probe(id);
  if (!probe || !raw || callback_depth || !probe->shared_runtime ||
      !probe->active.load(std::memory_order_acquire) ||
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
  for (auto &candidate : shared_invocations) {
    bool free = false;
    if (candidate.occupied.compare_exchange_strong(free, true,
                                                   std::memory_order_acq_rel)) {
      token = &candidate;
      break;
    }
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
  token->raw.named_count = 0;
  for (unsigned i = 0;
       i < token->raw.argument_count && i < tracer::kMaxArguments; ++i)
    token->raw.argument_names[i] = i < probe->argument_names.size()
                                       ? probe->argument_names[i].c_str()
                                       : nullptr;
  ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &token->cancel_state);
  token->cancel_disabled = true;
  ++callback_depth;
  if (probe->shared_generation->requires_stack()) {
    tracer::capture_native_stack(token->raw, tracer::kMaxStackDepth, false);
    copy_shared_value(token->entry_stack, token->raw.stack);
    copy_shared_value(token->raw.stack, raw->stack);
  }
  token->raw.stack_capture = &shared_capture_stack;
  try {
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
  token->raw.stack_capture = &shared_capture_stack;
  token->raw.named_count = 0;
  for (unsigned i = 0;
       i < token->raw.argument_count && i < tracer::kMaxArguments; ++i)
    token->raw.argument_names[i] = i < token->probe->argument_names.size()
                                       ? token->probe->argument_names[i].c_str()
                                       : nullptr;
  ++callback_depth;
  const auto entry = raw->phase == tracer::TracePhase::Pre
                         ? token->probe->shared_generation->pre_entry()
                         : token->probe->shared_generation->post_entry();
  if (!invoke_shared(*token, entry)) {
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
  if (token && token->finished)
    token->occupied.store(false, std::memory_order_release);
}

std::uint64_t address_value(auto address) {
  if constexpr (requires { address.getValue(); }) {
    return address.getValue();
  } else {
    return static_cast<std::uint64_t>(address);
  }
}

void set_error(tracer::Request *request, const char *text) noexcept {
  if (request) {
    std::snprintf(request->error, sizeof(request->error), "%s", text);
  }
}

constexpr std::size_t kMaxSerializedProbeSpec = 256 * 1024;
constexpr std::size_t kMaxCallbackSource = 64 * 1024;
constexpr std::size_t kMaxReceiverFields = 128;

bool read_probe_spec(const tracer::Request &request,
                     clangtracer::v1::ProbeSpec &spec, std::string &error) {
  std::ifstream input(request.source_path, std::ios::binary);
  if (!input) {
    error = "cannot open serialized probe specification";
    return false;
  }
  std::string bytes(kMaxSerializedProbeSpec + 1, '\0');
  input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  const auto count = static_cast<std::size_t>(input.gcount());
  if (count > kMaxSerializedProbeSpec) {
    error = "serialized probe specification exceeds 256 KiB";
    return false;
  }
  bytes.resize(count);
  if (!spec.ParseFromArray(bytes.data(), static_cast<int>(bytes.size()))) {
    error = "cannot parse serialized probe specification";
    return false;
  }
  if (spec.symbol() != request.symbol || !spec.has_signature() ||
      static_cast<unsigned>(spec.signature().kind()) !=
          static_cast<unsigned>(request.kind) ||
      static_cast<unsigned>(spec.signature().result()) !=
          static_cast<unsigned>(request.result) ||
      spec.signature().arguments_size() !=
          static_cast<int>(request.argument_count) ||
      request.argument_count > tracer::kMaxArguments) {
    error =
        "serialized probe specification does not match the prepared request";
    return false;
  }
  for (unsigned i = 0; i < request.argument_count; ++i) {
    if (static_cast<unsigned>(spec.signature().arguments(static_cast<int>(
            i))) != static_cast<unsigned>(request.arguments[i])) {
      error = "serialized probe signature does not match the prepared request";
      return false;
    }
  }
  if (spec.callback_source().size() > kMaxCallbackSource) {
    error = "callback source exceeds 64 KiB";
    return false;
  }
  if (spec.stack_depth() > tracer::kMaxStackDepth) {
    error = "stack depth must be between 0 and 64";
    return false;
  }
  if (spec.has_receiver()) {
    const auto &receiver = spec.receiver();
    if (receiver.ByteSizeLong() > kMaxCallbackSource) {
      error = "receiver compilation context exceeds 64 KiB";
      return false;
    }
    if (receiver.translation_unit().empty() ||
        receiver.working_directory().empty() ||
        receiver.compiler_arguments().empty() ||
        receiver.record_name().empty() ||
        receiver.fields_size() > static_cast<int>(kMaxReceiverFields)) {
      error =
          "receiver compilation context is incomplete or exceeds its limits";
      return false;
    }
    std::size_t arguments_size = 0;
    for (const auto &argument : receiver.compiler_arguments()) {
      arguments_size += argument.size();
      if (arguments_size > kMaxCallbackSource) {
        error = "receiver compiler arguments exceed 64 KiB";
        return false;
      }
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

bool validate_receiver_layout(clang::ASTContext &context,
                              clang::CXXRecordDecl *record,
                              const clangtracer::v1::ReceiverContext &receiver,
                              std::string &error) {
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
  std::string result;
  bool pending_space = false;
  for (const unsigned char c : text) {
    if (std::isspace(c)) {
      pending_space = true;
      continue;
    }
    const bool punctuation = c == ':' || c == '<' || c == '>' || c == ',' ||
                             c == '*' || c == '&';
    if (pending_space && !result.empty() && !punctuation &&
        result.back() != ':' && result.back() != '<' &&
        result.back() != ',' && result.back() != '*' &&
        result.back() != '&')
      result.push_back(' ');
    result.push_back(static_cast<char>(c));
    pending_space = false;
  }
  return result;
}

bool standard_library_record_name(llvm::StringRef name) {
  return ::tracer::compat::starts_with(name, "std::") ||
         ::tracer::compat::starts_with(name, "__gnu_cxx::") ||
         ::tracer::compat::starts_with(name, "__cxxabiv1::");
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

bool debug_record_identity_matches(clang::ASTContext &context,
                                   const clang::CXXRecordDecl *record,
                                   llvm::StringRef expected,
                                   std::string *mismatch = nullptr) {
  clang::PrintingPolicy policy(context.getLangOpts());
  policy.SuppressTagKeyword = true;
  policy.SuppressDefaultTemplateArgs = false;
  policy.SuppressInlineNamespace = false;
  const auto actual = context.getRecordType(record).getAsString(policy);
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
    default:
      return false;
    }
  }
  return false; // A pointer chain without its terminal type is incomplete.
}

DebugRecord construct_debug_record(clang::ASTContext &context,
                                   const clangtracer::v1::DebugInfo &info) {
  class Finder : public clang::RecursiveASTVisitor<Finder> {
  public:
    clang::ASTContext *context = nullptr;
    std::string selected;
    clang::CXXRecordDecl *found = nullptr;
    std::string candidate_mismatch;
    bool shouldVisitTemplateInstantiations() const { return true; }
    bool VisitCXXRecordDecl(clang::CXXRecordDecl *d) {
      if (!d->isCompleteDefinition() || d->isDependentType())
        return true;
      if (d->getQualifiedNameAsString() == selected &&
          !llvm::isa<clang::ClassTemplateSpecializationDecl>(d)) {
        found = d;
        return true;
      }
      std::string mismatch;
      if (debug_record_identity_matches(*context, d, selected, &mismatch))
        found = d;
      else if (candidate_mismatch.empty() &&
               ::tracer::compat::contains(d->getQualifiedNameAsString(),
                                          "basic_string"))
        candidate_mismatch = std::move(mismatch);
      return true;
    }
  } finder;
  finder.context = &context;
  finder.selected = info.record_name();
  finder.TraverseDecl(context.getTranslationUnitDecl());
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
  if (record && info.record_is_opaque()) {
    if (!record->isCompleteDefinition())
      throw std::runtime_error("header record is incomplete for opaque DWARF type");
    const auto &layout = context.getASTRecordLayout(record);
    if ((info.record_size() &&
         static_cast<std::uint64_t>(layout.getSize().getQuantity()) !=
             info.record_size()) ||
        (info.record_alignment() &&
         layout.getAlignment().getQuantity() != info.record_alignment()))
      throw std::runtime_error(
          "header record size/alignment does not match opaque DWARF type");
    clangtracer::v1::ReceiverContext metadata;
    metadata.set_record_name(info.record_name());
    metadata.set_size(info.record_size());
    metadata.set_alignment(info.record_alignment());
    metadata.set_is_volatile(info.receiver_volatile());
    return {record, std::move(metadata), true};
  }
  if (!record && info.record_is_opaque()) {
    clang::DeclContext *scope = context.getTranslationUnitDecl();
    llvm::StringRef remaining = info.record_name();
    while (::tracer::compat::contains(remaining, "::")) {
      auto part = remaining.split("::");
      if (!debug_identifier(part.first))
        throw std::runtime_error("unsupported debug record scope");
      auto &id = context.Idents.get(part.first);
      clang::NamespaceDecl *ns = nullptr;
      for (auto *named : scope->lookup(&id))
        if ((ns = llvm::dyn_cast<clang::NamespaceDecl>(named)))
          break;
      if (!ns) {
        ns = clang::NamespaceDecl::Create(context, scope, false, {}, {}, &id,
                                          nullptr, false);
        scope->addDecl(ns);
      }
      scope = ns;
      remaining = part.second;
    }
    if (!debug_identifier(remaining))
      throw std::runtime_error("unsupported debug record name");
    record = clang::CXXRecordDecl::Create(
        context, clang::TagTypeKind::Struct, scope, {}, {},
        &context.Idents.get(remaining));
    scope->addDecl(record);
    clangtracer::v1::ReceiverContext metadata;
    metadata.set_record_name(info.record_name());
    metadata.set_size(info.record_size());
    metadata.set_alignment(info.record_alignment());
    metadata.set_is_volatile(info.receiver_volatile());
    return {record, std::move(metadata), true};
  }
  if (!record) {
    clang::DeclContext *scope = context.getTranslationUnitDecl();
    llvm::StringRef remaining = info.record_name();
    while (::tracer::compat::contains(remaining, "::")) {
      auto part = remaining.split("::");
      if (!debug_identifier(part.first))
        throw std::runtime_error("unsupported debug receiver scope");
      auto &id = context.Idents.get(part.first);
      clang::NamespaceDecl *ns = nullptr;
      for (auto *named : scope->lookup(&id))
        if ((ns = llvm::dyn_cast<clang::NamespaceDecl>(named)))
          break;
      if (!ns) {
        ns = clang::NamespaceDecl::Create(context, scope, false, {}, {}, &id,
                                          nullptr, false);
        scope->addDecl(ns);
      }
      scope = ns;
      remaining = part.second;
    }
    if (!debug_identifier(remaining))
      throw std::runtime_error("unsupported debug receiver name");
    record =
        clang::CXXRecordDecl::Create(context, clang::TagTypeKind::Class, scope,
                                     {}, {}, &context.Idents.get(remaining));
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

std::string recovered_record_declaration(
    const clangtracer::v1::DebugInfo &info) {
  if (info.record_name().empty())
    return {};
  std::string result;
  std::vector<std::string> namespaces;
  llvm::StringRef name(info.record_name());
  while (::tracer::compat::contains(name, "::")) {
    auto part = name.split("::");
    if (!debug_identifier(part.first))
      throw std::runtime_error("unsupported recovered record namespace");
    namespaces.push_back(part.first.str());
    name = part.second;
  }
  if (!debug_identifier(name))
    throw std::runtime_error("unsupported recovered record name");
  for (const auto &part : namespaces)
    result += "namespace " + part + " { ";
  if (info.record_is_opaque()) {
    result += "class " + name.str() + "; ";
  } else {
    result += "class";
    if (info.record_alignment() > 1)
      result += " alignas(" + std::to_string(info.record_alignment()) + ")";
    result += " " + name.str();
    result += " { ";
    std::string access;
    for (const auto &field : info.fields()) {
      if (!debug_identifier(field.name()))
        throw std::runtime_error("unsupported recovered field name");
      if (field.access() != access) {
        access = field.access();
        result += access + ": ";
      }
      result += field.builtin_type() + " " + field.name() + "; ";
    }
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
  for (std::size_t i = 0; i < namespaces.size(); ++i)
    result += "} ";
  return result;
}

std::string recovered_record_declarations(
    const clangtracer::v1::ProbeSpec &spec) {
  const auto &info = spec.debug_info();
  std::string declarations;
  std::unordered_set<std::string> seen;
  auto append = [&](const clangtracer::v1::DebugInfo &record) {
    if (!record.record_name().empty() &&
        seen.insert(record.record_name()).second)
      declarations += recovered_record_declaration(record);
  };
  if (!info.record_name().empty())
    append(info);
  for (const auto &argument : info.arguments())
    if (argument.has_object_type())
      append(argument.object_type());
  if (info.has_result_object_type())
    append(info.result_object_type());
  return declarations;
}

bool ast_has_debug_record(clang::ASTContext &context,
                          const clangtracer::v1::DebugInfo &info) {
  class Finder : public clang::RecursiveASTVisitor<Finder> {
  public:
    Finder(clang::ASTContext &context, llvm::StringRef expected)
        : context(context), expected(expected) {}
    bool VisitCXXRecordDecl(clang::CXXRecordDecl *record) {
      if (debug_record_identity_matches(context, record, expected))
        found = true;
      return !found;
    }
    clang::ASTContext &context;
    llvm::StringRef expected;
    bool found = false;
  } finder(context, info.record_name());
  finder.TraverseDecl(context.getTranslationUnitDecl());
  return finder.found;
}

std::string missing_recovered_record_declarations(
    clang::ASTContext &context,
    const clangtracer::v1::ProbeSpec &spec) {
  std::string declarations;
  std::unordered_set<std::string> seen;
  const auto append = [&](const clangtracer::v1::DebugInfo &record) {
    if (record.record_name().empty() ||
        !seen.insert(record.record_name()).second ||
        ast_has_debug_record(context, record))
      return;
    declarations += recovered_record_declaration(record);
  };
  const auto &info = spec.debug_info();
  if (!info.record_name().empty())
    append(info);
  for (const auto &argument : info.arguments())
    if (argument.has_object_type())
      append(argument.object_type());
  if (info.has_result_object_type())
    append(info.result_object_type());
  return declarations;
}

std::uint64_t validate_debug_entry(const clangtracer::v1::ProbeSpec &spec) {
  const auto &info = spec.debug_info();
  tracer::client::DebugLookupOptions lookup;
  lookup.module = info.module();
  const auto checked =
      tracer::client::resolve_debug_callable(::getpid(), info.selector(), lookup);
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

bool build_debug_receiver(clangtracer::v1::ProbeSpec &spec,
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
    const bool headers_available = spec.has_header_context() ||
                                   spec.include_headers_size() != 0 ||
                                   !callback_includes.empty();
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
    if (spec.include_headers_size() > 16)
      throw std::runtime_error("at most 16 optional includes");
    for (const auto &header : spec.include_headers()) {
      if (header.empty() || header.size() > 4095 || header.front() != '/' ||
          header.find_first_of("\"\r\n") != std::string::npos ||
          header.find('\0') != std::string::npos)
        throw std::runtime_error("invalid optional include path");
      headers += "#include \"" + header + "\"\n";
    }
    if (!spec.has_header_context() && spec.include_headers().empty() &&
        callback_includes.empty())
      headers += recovered_record_declarations(spec);
    if (!callback_includes.empty())
      headers = callback_includes + headers;
    DiagnosticCapture capture(nullptr);
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
      arguments.insert(arguments.end(), {"-x", "c++", "-fsyntax-only"});
      clang::tooling::FixedCompilationDatabase database(directory, arguments);
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
      clang::tooling::ClangTool tool(
          database, {input}, std::make_shared<clang::PCHContainerOperations>(),
          private_fs);
      tool.mapVirtualFile(input, headers);
      tool.setDiagnosticConsumer(&capture);
      tool.appendArgumentsAdjuster(
          clang::tooling::getClangStripOutputAdjuster());
      tool.appendArgumentsAdjuster(
          clang::tooling::getClangStripDependencyFileAdjuster());
      std::vector<std::unique_ptr<clang::ASTUnit>> units;
      if (tool.buildASTs(units) != 0 || units.size() != 1)
        throw std::runtime_error("cannot parse optional headers: " +
                                 capture.message());
      output = std::move(units.front());
    } else {
      auto arguments = tracer::runtime::jit_arguments();
      arguments.insert(arguments.end(),
                       {"-std=c++20", "-x", "c++",
                        std::string("-resource-dir=") +
                            tracer::runtime::clang_resource_dir()});
      arguments.insert(arguments.end(), spec.compile_options().begin(),
                       spec.compile_options().end());
      output = clang::tooling::buildASTFromCodeWithArgs(
          headers, arguments, "clang-tracer-debug-types.cc", "clang-tracer",
          std::make_shared<clang::PCHContainerOperations>(),
          clang::tooling::getClangStripOutputAdjuster(), {}, &capture);
    }
    if (!output || capture.had_error())
      throw std::runtime_error("cannot create debug type AST: " +
                               capture.message());
    auto &context = output->getASTContext();
    const auto &triple = context.getTargetInfo().getTriple();
    if (triple.getArch() != llvm::Triple::x86_64 || !triple.isOSLinux() ||
        context.getTypeSize(context.VoidPtrTy) != 64)
      throw std::runtime_error("header target does not match Linux x86_64");
    bool target_has_std_string = false;
    bool target_uses_cxx11_string_abi =
        spec.symbol().find("__cxx11") != std::string::npos;
    const auto note_standard_type = [&](const auto &record) {
      const auto &record_name = record.record_name();
      target_has_std_string |= record_name.find("basic_string") !=
                               std::string::npos;
      target_uses_cxx11_string_abi |=
          record_name.find("__cxx11") != std::string::npos;
    };
    note_standard_type(info);
    for (const auto &argument : info.arguments())
      if (argument.has_object_type())
        note_standard_type(argument.object_type());
    if (info.has_result_object_type())
      note_standard_type(info.result_object_type());
    if (target_has_std_string) {
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
              "libstdc++ ABI mismatch: target DWARF string linkage name and "
              "_GLIBCXX_USE_CXX11_ABI selected by headers/options disagree");
      }
    }
    if (!info.record_name().empty())
      records.push_back(construct_debug_record(context, info));
    for (const auto &argument : info.arguments()) {
      if (!argument.has_object_type())
        continue;
      const auto &object = argument.object_type();
      bool duplicate = false;
      for (const auto &previous : records)
        if (previous.layout.record_name() == object.record_name())
          duplicate = true;
      auto checked_record = construct_debug_record(context, object);
      if (!duplicate)
        records.push_back(std::move(checked_record));
    }
    if (info.has_result_object_type()) {
      const auto &object = info.result_object_type();
      bool duplicate = false;
      for (const auto &previous : records)
        if (previous.layout.record_name() == object.record_name())
          duplicate = true;
      auto checked_record = construct_debug_record(context, object);
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
    return false;
  }
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
  DiagnosticCapture diagnostics(nullptr);
  tool.setDiagnosticConsumer(&diagnostics);
  tool.appendArgumentsAdjuster(clang::tooling::getClangStripOutputAdjuster());
  std::vector<std::unique_ptr<clang::ASTUnit>> units;
  if (tool.buildASTs(units) != 0 || diagnostics.had_error() ||
      units.size() != 1) {
    error = diagnostics.message();
    if (error.empty()) {
      error = "cannot parse receiver translation unit with its frozen compiler "
              "context";
    }
    return false;
  }
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
                                !callback_includes.empty() ||
                                !forced_include_source.empty();
  if (!spec.has_debug_info())
    return true;
  const auto validate = [&](const clangtracer::v1::DebugInfo &record,
                            const char *role,
                            bool allow_nontrivial_result) {
    const auto convention = record.record_calling_convention();
    if (header_available)
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

void fill_value(std::string &code, unsigned i, tracer::ValueType type) {
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
  case tracer::ValueType::Object:
    code += "ctx.arguments[" + std::to_string(i) +
            "].unsigned_value=reinterpret_cast<uintptr_t>(&a" +
            std::to_string(i) + ");\n";
    break;
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

bool initialize_llvm(std::string &error) {
  static std::once_flag once;
  static std::string init_error;
  std::call_once(once, [] {
    if (llvm::InitializeNativeTarget() ||
        llvm::InitializeNativeTargetAsmPrinter() ||
        llvm::InitializeNativeTargetDisassembler()) {
      init_error = "LLVM native target initialization failed";
    }
  });
  if (!init_error.empty()) {
    error = init_error;
    return false;
  }
  return true;
}

struct Decoder {
  std::unique_ptr<llvm::MCRegisterInfo> regs;
  std::unique_ptr<llvm::MCAsmInfo> asm_info;
  std::unique_ptr<llvm::MCSubtargetInfo> sub;
  std::unique_ptr<llvm::MCInstrInfo> ins;
  std::unique_ptr<llvm::MCContext> context;
  std::unique_ptr<llvm::MCDisassembler> dis;
};

bool make_decoder(Decoder &d, std::string &error) {
  if (!initialize_llvm(error)) {
    return false;
  }
  auto triple = llvm::sys::getDefaultTargetTriple();
  std::string lookup_error;
  auto *target = llvm::TargetRegistry::lookupTarget(triple, lookup_error);
  if (!target) {
    error = "LLVM target lookup failed: " + lookup_error;
    return false;
  }
  llvm::MCTargetOptions options;
  llvm::Triple parsed(triple);
  d.regs.reset(target->createMCRegInfo(triple));
  d.ins.reset(target->createMCInstrInfo());
  d.sub.reset(target->createMCSubtargetInfo(triple, "generic", ""));
  if (!d.regs || !d.ins || !d.sub) {
    error = "LLVM MC metadata unavailable";
    return false;
  }
  d.asm_info.reset(target->createMCAsmInfo(*d.regs, triple, options));
  if (!d.asm_info) {
    error = "LLVM MC assembly metadata unavailable";
    return false;
  }
  d.context = std::make_unique<llvm::MCContext>(parsed, d.asm_info.get(),
                                                d.regs.get(), d.sub.get());
  d.dis.reset(target->createMCDisassembler(*d.sub, *d.context));
  if (!d.dis) {
    error = "LLVM MC disassembler unavailable";
    return false;
  }
  return true;
}

bool safe_instruction(const llvm::MCInst &inst, const llvm::MCInstrInfo &ins,
                      const llvm::MCRegisterInfo &regs) {
  const auto &desc = ins.get(inst.getOpcode());
  if (desc.isBranch() || desc.isCall() || desc.isReturn() ||
      desc.isIndirectBranch()) {
    return false;
  }
  const std::string name = ins.getName(inst.getOpcode()).str();
  static constexpr std::string_view allowed[] = {
      "PUSH64r",   "POP64r",    "MOV64rr",    "MOV32rr",   "MOV16rr",
      "MOV8rr",    "MOV32ri",   "MOV64rm",    "MOV32rm",   "MOV16rm",
      "MOV8rm",    "MOV64mr",   "MOV32mr",    "MOV16mr",   "MOV8mr",
      "MOVSS",     "MOVSD",     "MOVAPS",     "MOVUPS",    "LEA64r",
      "LEA32r",    "SUB64ri8",  "SUB64ri32",  "ADD64ri8",  "ADD64ri32",
      "ADD32ri8",  "ADD32ri",   "IMUL32rri8", "IMUL32rri", "IMUL32rmi8",
      "IMUL32rmi", "XOR64rr",   "XOR32rr",    "TEST64rr",  "TEST32rr",
      "CMP64rr",   "CMP32rr",   "AND8i8",     "AND8ri",    "AND64ri8",
      "AND64ri32", "XOR8i8",    "XOR8ri",     "NOP",
      "NOOP",      "MOV64mi32", "MOV32mi",    "MOV16mi",   "MOV8mi"};
  bool known = false;
  for (auto p : allowed) {
    if (name == p || (name.size() > p.size() && ::tracer::compat::starts_with(name, p))) {
      known = true;
      break;
    }
  }
  if (!known) {
    return false;
  }
  for (unsigned i = 0; i < inst.getNumOperands(); ++i) {
    if (inst.getOperand(i).isReg()) {
      auto r = inst.getOperand(i).getReg();
      const char *n = r ? regs.getName(r) : nullptr;
      if (n && (!std::strcmp(n, "RIP") || !std::strcmp(n, "EIP"))) {
        return false;
      }
    }
  }
  return true;
}

bool decode_patch(const std::uint8_t *entry, tracer::Request &req,
                  std::string &error) {
  std::uintptr_t lo = 0, hi = 0;
  int prot = 0;
  if (!mapping(entry, lo, hi, prot) ||
      hi - reinterpret_cast<std::uintptr_t>(entry) < kJumpBytes) {
    error = "target entry has insufficient mapped bytes";
    return false;
  }
  if (entry[0] == 0xf3 && entry[1] == 0x0f && entry[2] == 0x1e &&
      entry[3] == 0xfa) {
    error = "target entry has an ENDBR64 CET landing instruction";
    return false;
  }
  Decoder d;
  if (!make_decoder(d, error)) {
    return false;
  }
  const auto address = reinterpret_cast<std::uintptr_t>(entry);
  std::size_t used = 0;
  while (used < kJumpBytes) {
    llvm::MCInst inst;
    std::uint64_t size = 0;
    auto status = d.dis->getInstruction(
        inst, size,
        llvm::ArrayRef<std::uint8_t>(
            entry + used, std::min<std::size_t>(kMaxDecodedBytes - used,
                                                hi - address - used)),
        address + used, llvm::nulls());
    if (status != llvm::MCDisassembler::Success || !size ||
        !safe_instruction(inst, *d.ins, *d.regs)) {
      error = "target entry contains an unsupported or PC-relative instruction";
      return false;
    }
    used += size;
    if (used > kMaxDecodedBytes) {
      error = "target entry exceeds safe patch window";
      return false;
    }
  }
  req.patch_size = static_cast<std::uint32_t>(used);
  std::memcpy(req.original, entry, used);
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
      !S_ISREG(module_status.st_mode) || module_status.st_ino == 0) {
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
  if (!decode_patch(patch_entry, patch, error))
    return false;
  patch.pthread_create_patch_size = patch.patch_size;
  std::memcpy(patch.pthread_create_original, patch.original, patch.patch_size);
  const auto probe_begin = probe.request.original_entry;
  const auto probe_end = probe_begin + probe.request.patch_size;
  const auto scheduler_end = patch.pthread_create_entry + patch.patch_size;
  if (probe_begin < scheduler_end && patch.pthread_create_entry < probe_end) {
    error = "pthread_create patch overlaps the target probe";
    return false;
  }
  for (std::size_t i = 0; i < owner.retained; ++i) {
    const auto *existing = owner.probes[i].get();
    if (!existing || !existing->active.load(std::memory_order_acquire))
      continue;
    const auto existing_begin = existing->request.original_entry;
    const auto existing_end = existing_begin + existing->request.patch_size;
    if (existing_begin < scheduler_end &&
        patch.pthread_create_entry < existing_end) {
      error = "pthread_create patch overlaps an active target probe";
      return false;
    }
  }

  const long page_size = ::sysconf(_SC_PAGESIZE);
  if (page_size <= 0) {
    error = "cannot determine page size for pthread_create trampoline";
    return false;
  }
  const auto trampoline_size = static_cast<std::size_t>(page_size);
  void *trampoline = ::mmap(nullptr, trampoline_size, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (trampoline == MAP_FAILED) {
    error = "cannot allocate pthread_create trampoline";
    return false;
  }
  std::memcpy(trampoline, patch.pthread_create_original, patch.patch_size);
  add_jump(static_cast<std::uint8_t *>(trampoline) + patch.patch_size,
           patch_entry + patch.patch_size);
  if (::mprotect(trampoline, trampoline_size, PROT_READ | PROT_EXEC) != 0) {
    ::munmap(trampoline, trampoline_size);
    error = "cannot make pthread_create trampoline executable";
    return false;
  }

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
                         bool result_sret = false) {
  auto result_type = [&]() -> std::string {
    if (r.result != tracer::ValueType::Object)
      return type_name(r.result);
    if (!spec || !spec->has_debug_info() ||
        spec->debug_info().result_cpp_type().empty())
      throw std::runtime_error("missing object result ABI metadata");
    return spec->debug_info().result_cpp_type();
  };
  auto argument_type = [&](unsigned i) -> std::string {
    if (r.arguments[i] != tracer::ValueType::Object)
      return type_name(r.arguments[i]);
    if (!spec || !spec->has_debug_info() ||
        i >= static_cast<unsigned>(spec->debug_info().arguments_size()))
      throw std::runtime_error("missing object argument ABI metadata");
    const auto &argument = spec->debug_info().arguments(i);
    if (!argument.is_object_value() || !argument.has_object_type())
      throw std::runtime_error("invalid by-value argument metadata");
    return argument.cpp_type();
  };
  std::string code =
      "extern \"C\" long long tracer_callback_dispatch(unsigned long "
      "long,const tracer::TraceContext*,void*) noexcept;\nextern \"C\" void "
      "tracer_event_dispatch(unsigned long long,const "
      "tracer::TraceContext*,long long) noexcept;\nextern \"C\" void "
      "tracer_probe_enter(unsigned long long) noexcept;\nextern \"C\" void "
      "tracer_probe_leave(unsigned long long) noexcept;\nextern \"C\" "
      "unsigned long long tracer_next_call_id(unsigned long long) noexcept;\n";
  code +=
      "extern \"C\" void tracer_stack_enter(tracer::TraceContext*,void*) "
      "noexcept;\n"
      "extern \"C\" void tracer_stack_leave(tracer::TraceContext*) noexcept;\n"
      "extern \"C\" void tracer_capture_entry_stack(const "
      "tracer::TraceContext*,unsigned) noexcept;\n"
      "extern \"C\" void tracer_capture_stack(const "
      "tracer::TraceContext*,unsigned,bool) noexcept;\n";
  code += "extern \"C\" void tracer_context_check_entry();\n";
  if (shared_mode)
    code += "#include <bits/cxxabi_forced.h>\n"
            "extern \"C\" void* tracer_shared_enter(unsigned long long,const "
            "tracer::TraceContext*);\n"
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
    code += result_type() + " (*) (";
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
  code += ");\nextern \"C\" ";
  code += result_sret ? result_type() + "* clangtracer_wrapper(" + result_type() +
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
  code += "ctx.target_entry=" + std::to_string(r.original_entry) +
          "ULL; ctx.bridge_begin=reinterpret_cast<uintptr_t>(&&original_begin);"
          "ctx.bridge_end=reinterpret_cast<uintptr_t>(&&original_end);"
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
  for (unsigned i = 0; i < r.argument_count; ++i) {
    fill_value(code, i, r.arguments[i]);
  }
  if (spec && spec->stack_depth())
    code += "tracer_capture_entry_stack(&ctx," +
            std::to_string(spec->stack_depth()) + ");\n";
  if (shared_mode) {
    code += "struct SharedGuard { void* token=nullptr; ~SharedGuard() noexcept "
            "{ if(token){tracer_shared_leave(token); tracer_shared_release(token);} } } "
            "shared_guard;\n"
            "shared_guard.token=tracer_shared_enter(" +
            std::to_string(r.probe_id) +
            "ULL,&ctx);\ntry { tracer_shared_resume(shared_guard.token);\n";
  } else if (class_mode)
    code += "tracer_invocation_instance tracer_invocation(ctx);\n";
  const std::string instance = class_mode ? "&tracer_invocation" : "nullptr";
  if (has_pre) {
    if (shared_mode)
      code += "tracer_shared_dispatch(shared_guard.token,&ctx);\n";
    else
      code += "(void)tracer_callback_dispatch(" + std::to_string(r.probe_id) +
              "ULL,&ctx," + instance + ");\n";
  }
  code += "auto original=reinterpret_cast<Original>(" +
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
    call += "a" + std::to_string(i);
    comma = true;
  }
  call += ")";
  if (shared_mode && r.result != tracer::ValueType::Void)
    if (r.result != tracer::ValueType::Object)
      code += result_type() + " result;\n";
  if (shared_mode && r.result != tracer::ValueType::Object)
    code += "{ tracer_shared_original_enter(shared_guard.token); struct "
            "OriginalGuard { void* token; ~OriginalGuard() noexcept { "
            "tracer_shared_original_leave(token); } } "
            "original_guard{shared_guard.token};\n";
  if (r.result == tracer::ValueType::Void) {
    code += "original_begin: " + call +
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
              "original_begin: result=" + call + ";\noriginal_end: ;\n";
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
          "ctx.result.unsigned_value=reinterpret_cast<uintptr_t>(&result);\n";
      break;
    default:
      break;
    }
  }
  if (shared_mode && r.result != tracer::ValueType::Object)
    code += "}\n";
  if (has_post) {
    if (shared_mode)
      code += "ctx.phase=tracer::TracePhase::Post; "
              "tracer_shared_dispatch(shared_guard.token,&ctx);\n";
    else
      code += "ctx.phase=tracer::TracePhase::Post; "
              "(void)tracer_callback_dispatch(" +
              std::to_string(r.probe_id) + "ULL,&ctx," + instance + ");\n";
  }
  if (shared_mode)
    code += "tracer_shared_leave(shared_guard.token); "
            "tracer_shared_resume(shared_guard.token); "
            "tracer_shared_release(shared_guard.token); "
            "shared_guard.token=nullptr;\n";
  if (result_sret) {
    if (class_mode)
      code += "tracer_invocation.destroy();\n";
    code += "tracer_stack_leave(&ctx); stack_guard.ctx=nullptr; "
            "tracer_probe_leave(" + std::to_string(r.probe_id) +
            "ULL); flight.id=0; return result_slot;\n";
  } else if (r.result == tracer::ValueType::Void) {
    code += "return;\n";
  } else {
    code += "return result;\n";
  }
  if (shared_mode)
    code += "} catch (const __cxxabiv1::__forced_unwind&) { throw; } catch "
            "(...) { tracer_shared_leave(shared_guard.token); "
            "tracer_shared_resume(shared_guard.token); "
            "tracer_shared_release(shared_guard.token); "
            "shared_guard.token=nullptr; throw; }\n";
  code += "}";
  return code;
}

struct CallbackShape {
  bool class_mode = false;
  bool has_pre = false;
  bool has_post = false;
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
              context.getRecordType(specialization).getAsString(policy);
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

bool debug_type_matches(clang::ASTContext &context, clang::QualType actual,
                        const std::string &spelling,
                        const clangtracer::v1::DebugInfo *object,
                        bool is_reference, bool object_value) {
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
    if (!debug_record_identity_matches(context, record,
                                       object->record_name()))
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
    error = "Interceptor template argument must be a function signature";
    return false;
  }
  const auto &info = spec.debug_info();
  if (function->getNumParams() != info.arguments_size()) {
    error = "Interceptor signature argument count does not match target";
    return false;
  }
  const auto has_multilevel_record_pointer = [](const std::string &type,
                                                 bool has_record) {
    return has_record &&
           std::count(type.begin(), type.end(), '*') > 1;
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
                          result_info.result_is_object_value())) {
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
            argument.is_reference(), argument.is_object_value())) {
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
};

std::string make_class_bridge(const CallbackShape &shape);

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
    if (shape.class_mode) {
      result_.bridge = make_class_bridge(shape);
    } else {
      result_.bridge = tracer::make_legacy_callback_bridge();
    }
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
                       const clangtracer::v1::ProbeSpec *spec = nullptr)
      : records_(records), donor_context_(donor_context),
        donor_files_(donor_files), donor_record_(donor_record), shape_(shape),
        receiver_context_(receiver_context), target_record_(target_record),
        validation_error_(validation_error), spec_(spec) {}

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
    if (donor_record_) {
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
    if (target_record_ && donor_record_ && selected_imported_record) {
      // Skip imported receiver method bodies while checking callback and
      // included helper code for ODR uses of the selected target.
      visitor_ = std::make_unique<ForbiddenReceiverUseVisitor>(
          selected_imported_record, importer.declarations, imported_targets);
    }
    clang::ParseAST(compiler.getSema());
    if (diagnostic_capture_ && diagnostic_capture_->had_error()) {
      *validation_error_ = diagnostic_capture_->message();
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

private:
  class ForbiddenReceiverUseVisitor final
      : public clang::RecursiveASTVisitor<ForbiddenReceiverUseVisitor> {
  public:
    ForbiddenReceiverUseVisitor(
        clang::CXXRecordDecl *target,
        const std::unordered_set<const clang::Decl *> &imported,
        const std::vector<clang::CXXRecordDecl *> &targets)
        : imported_(imported) {
      targets_.insert(target->getCanonicalDecl());
      for (auto *t : targets)
        targets_.insert(t->getCanonicalDecl());
    }
    bool is_target(const clang::CXXRecordDecl *record) const {
      return record && targets_.count(record->getCanonicalDecl());
    }

    bool shouldVisitTemplateInstantiations() const { return true; }

    bool shouldVisitImplicitCode() const { return true; }

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
            method && is_target(method->getParent())) {
          error_ = "callback cannot call methods on the imported receiver";
        }
      }
      return true;
    }

    bool VisitMemberExpr(clang::MemberExpr *expression) {
      if (imported_.count(expression->getMemberDecl()->getCanonicalDecl()) !=
              0 &&
          llvm::isa<clang::CXXMethodDecl>(expression->getMemberDecl())) {
        error_ = "callback cannot call or take the address of imported target "
                 "methods";
      }
      if (const auto *method =
              llvm::dyn_cast<clang::CXXMethodDecl>(expression->getMemberDecl());
          method && is_target(method->getParent())) {
        error_ = "callback cannot call or take the address of imported "
                 "receiver methods";
      }
      return true;
    }

    bool VisitDeclRefExpr(clang::DeclRefExpr *expression) {
      if (imported_.count(expression->getDecl()->getCanonicalDecl()) != 0) {
        if (const auto *function =
                llvm::dyn_cast<clang::FunctionDecl>(expression->getDecl());
            function && function->getDefinition()) {
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
          method && is_target(method->getParent())) {
        error_ = "callback cannot call or take the address of imported "
                 "receiver methods";
      } else if (const auto *variable =
                     llvm::dyn_cast<clang::VarDecl>(expression->getDecl());
                 variable && variable->getDeclContext()->isRecord() &&
                 is_target(llvm::dyn_cast<clang::CXXRecordDecl>(
                     variable->getDeclContext()))) {
        error_ =
            "callback cannot odr-use imported receiver static data members";
      }
      return true;
    }

    bool VisitCXXConstructExpr(clang::CXXConstructExpr *expression) {
      if (auto *record = expression->getType()->getAsCXXRecordDecl();
          record && is_target(record)) {
        error_ =
            "callback cannot construct objects of the imported receiver type";
      }
      return true;
    }

    bool VisitInitListExpr(clang::InitListExpr *expression) {
      if (auto *record = expression->getType()->getAsCXXRecordDecl();
          record && is_target(record)) {
        error_ =
            "callback cannot construct objects of the imported receiver type";
      }
      return true;
    }

    bool VisitCXXNewExpr(clang::CXXNewExpr *expression) {
      if (auto *record = expression->getAllocatedType()->getAsCXXRecordDecl();
          record && is_target(record)) {
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
            record && is_target(record)) {
          error_ =
              "callback cannot destroy objects of the imported receiver type";
        }
      }
      return true;
    }

    const std::string &error() const { return error_; }

  private:
    std::unordered_set<const clang::CXXRecordDecl *> targets_;
    const std::unordered_set<const clang::Decl *> &imported_;
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
  for (const auto &argument : spec.debug_info().arguments()) {
    // Preserve the full DWARF pointer chain. Multi-level pointers to records
    // are rejected by signature validation until their CV chain is modeled.
    std::string type = "typename ReadOnlyObservationType<" +
                       argument.cpp_type() + ">::type";
    if (argument.is_object_value()) {
      type = "const " + argument.cpp_type() + "&";
    }
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
  code += "using ResolvedSignature=" + info.result_cpp_type() + "(";
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
                             std::string &source, std::string &error) {
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
      const auto &path = arguments[i];
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
                          const std::string &signature_assert,
                          std::string &error, bool run_initializers,
                          clangtracer::v1::ValidationReport *report,
                          bool result_sret, bool syntax_only) {
  tracer::ContextActivationRequest activation;
  const auto [callback_includes, callback_body] =
      split_callback_includes(specification.callback_source());
  std::string requested_header_source;
  for (const auto &header : specification.include_headers())
    requested_header_source += "#include \"" + header + "\"\n";
  const bool has_real_headers = specification.has_header_context() ||
                                specification.include_headers_size() != 0 ||
                                !callback_includes.empty() ||
                                !forced_include_source.empty();
  activation.source = forced_include_source + callback_includes +
                      requested_header_source +
                      (has_real_headers
                           ? std::string()
                           : recovered_record_declarations(specification)) +
                      argument_aliases(specification) +
                      interceptor_template_prelude(specification.callback_source()) +
                      callback_body + "\n" + signature_assert;
  activation.compiler_arguments = arguments;
  if (!specification.callback_package_path().empty())
    activation.file_name = specification.callback_package_path();
  std::copy_n(p.request.source_lineage_id, activation.source_lineage_id.size(),
              activation.source_lineage_id.begin());
  if (!run_initializers) {
    const auto preflight =
        tracer::preflight_context_source(activation, syntax_only);
    if (!preflight) {
      error = preflight.error;
      return false;
    }
    const auto generated_wrapper =
        make_wrapper(p.request,
                     reinterpret_cast<std::uintptr_t>(p.trampoline), true,
                     true, &specification, false, true, result_sret);
    const auto complete_source = wrapper_prelude + generated_wrapper;
    if (syntax_only) {
      auto syntax_arguments = arguments;
      syntax_arguments.push_back("-fsyntax-only");
      DiagnosticCapture capture(nullptr);
      auto ast = clang::tooling::buildASTFromCodeWithArgs(
          complete_source, syntax_arguments, "clang-tracer-shared-wrapper.cc",
          "clang-tracer", std::make_shared<clang::PCHContainerOperations>(),
          clang::tooling::getClangStripOutputAdjuster(), {}, &capture);
      if (report)
        append_diagnostics(*report, capture, "syntax");
      if (!ast || capture.had_error()) {
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
  if (!owner ||
      owner->next_context_hook >= tracer::context_engine::kCaptureHooks) {
    error = "shared-context generation capacity exceeded";
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
    if (!owner->context_capture)
      owner->context_capture =
          std::make_unique<tracer::context_engine::ContextCapturePool>();
    p.context_delivery =
        tracer::context_delivery::ContextAgentDelivery::acquire(p.request);
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
    p.shared_runtime = std::make_shared<tracer::context_runtime::SharedSourceRuntime>(
        generation.generation->scopes(), *owner->context_capture, std::move(metadata),
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
                   true, true, &specification, false, true, result_sret);
  if (!parse_and_materialize(p, *p.interpreter,
                             wrapper_prelude + generated_wrapper, error)) {
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
  bind("tracer_event_dispatch", &tracer_event_dispatch);
  bind("tracer_probe_enter", &tracer_probe_enter);
  bind("tracer_probe_leave", &tracer_probe_leave);
  bind("tracer_next_call_id", &tracer_next_call_id);
  bind("tracer_stack_enter", &tracer_stack_enter);
  bind("tracer_stack_leave", &tracer_stack_leave);
  bind("tracer_capture_stack", &tracer_capture_stack);
  bind("tracer_capture_entry_stack", &tracer_capture_entry_stack);
  auto bind_debug_methods = [&](const clangtracer::v1::DebugInfo &info) {
    for (const auto &method : info.methods()) {
      if (method.is_virtual() || method.is_variadic() ||
          method.calling_convention() != llvm::dwarf::DW_CC_normal) {
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
  return true;
}

bool compile_probe(Probe &p, clangtracer::v1::ProbeSpec &specification,
                   std::string &error, bool run_initializers = true,
                   bool syntax_only = false,
                   clangtracer::v1::ValidationReport *report = nullptr,
                   const VerifiedDebugEntry *verified_entry = nullptr) {
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
  if (!validate_options(specification.compile_options().begin(),
                        specification.compile_options().end(),
                        "probe options") ||
      (specification.has_header_context() &&
       !validate_options(
           specification.header_context().compiler_arguments().begin(),
           specification.header_context().compiler_arguments().end(),
           "header context", true)) ||
      (specification.has_receiver() &&
       (specification.receiver().compiler_arguments_size() == 0 ||
        !validate_options(
            std::next(specification.receiver().compiler_arguments().begin()),
            specification.receiver().compiler_arguments().end(),
            "receiver context", true)))) {
    return false;
  }
  const std::string &source = specification.callback_source();
  const auto [callback_includes, callback_body] =
      split_callback_includes(source);
  std::vector<std::string> user_options(
      specification.compile_options().begin(),
      specification.compile_options().end());
  std::string forced_include_source;
  if (!extract_forced_includes(user_options, forced_include_source, error))
    return false;
  const std::string type_header_prefix =
      forced_include_source + callback_includes;
  std::string requested_header_source;
  for (const auto &header : specification.include_headers())
    requested_header_source += "#include \"" + header + "\"\n";
  const std::string callback_header_source =
      callback_includes + requested_header_source;
  const bool callback_headers_available =
      specification.has_header_context() ||
      specification.include_headers_size() != 0 ||
      !callback_includes.empty() || !forced_include_source.empty();
  const std::string callback_type_declarations =
      callback_headers_available ? std::string()
                                 : recovered_record_declarations(specification);
  if (specification.has_debug_info() &&
      !interceptor_template_prelude(source).empty()) {
    const auto has_multiple_pointers = [](const std::string &type) {
      return std::count(type.begin(), type.end(), '*') > 1;
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
  if (!validate_object_abi_evidence(specification, callback_includes,
                                    forced_include_source, error))
    return false;
  std::optional<VerifiedDebugEntry> local_verified_entry;
  if (specification.has_debug_info() && !verified_entry) {
    try {
      const auto runtime_entry = validate_debug_entry(specification);
      local_verified_entry =
          VerifiedDebugEntry{&specification, runtime_entry};
      verified_entry = &*local_verified_entry;
    } catch (const std::exception &exception) {
      error = exception.what();
      return false;
    }
  }
  p.argument_names.assign(specification.argument_names().begin(),
                          specification.argument_names().end());
  clang::CXXRecordDecl *receiver_record = nullptr;
  if (!build_and_validate_receiver_ast(specification, p.request, p.receiver_ast,
                                       receiver_record, error,
                                       p.debug_records, type_header_prefix,
                                       verified_entry)) {
    return false;
  }
  bool result_sret = false;
  if (!determine_object_result_sret(specification, p.debug_records,
                                    result_sret, error))
    return false;
  if (!initialize_llvm(error)) {
    return false;
  }
  const std::string resource_dir_argument =
      std::string("-resource-dir=") + tracer::runtime::clang_resource_dir();
  auto compiler_arguments = tracer::runtime::jit_arguments();
  compiler_arguments.insert(
      compiler_arguments.end(),
      {"-std=c++20", "-x", "c++", "-fPIC", "-O0", resource_dir_argument});
  compiler_arguments.insert(compiler_arguments.end(), user_options.begin(),
                            user_options.end());
  auto interpreter_arguments = compiler_arguments;
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
    auto created = clang::Interpreter::create(std::move(*compiler));
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
      created = clang::Interpreter::create(std::move(*fallback_compiler));
    }
    if (!created) {
      error = llvm::toString(created.takeError());
      return false;
    }
    p.interpreter = std::move(*created);
  }
  if (!capture_orc_errors(p, error))
    return false;
  if (!bind_probe_symbols(*p.interpreter, specification, error)) {
    return false;
  }
  const std::string prelude =
      std::string(tracer::kEmbeddedProtocolHeader) +
      "\nusing int32_t=std::int32_t; using int64_t=std::int64_t; "
      "using uint32_t=std::uint32_t; using uint64_t=std::uint64_t; "
      "using uintptr_t=std::uintptr_t;\n"
      "#define CLANG_TRACER_CALLBACK_TYPES_DEFINED 1\n";
  const std::string context_prelude =
      std::string(tracer::kEmbeddedContextHeader) +
      "\n#define CLANG_TRACER_CONTEXT_TYPES_DEFINED 1\n";
  const std::string user_source = forced_include_source + prelude + context_prelude +
                                  callback_header_source +
                                  callback_type_declarations +
                                  tracer::kEmbeddedCallbackHeader +
                                  argument_aliases(specification) +
                                  interceptor_template_prelude(source) +
                                  "#line 1 \"callback_source\"\n" +
                                  callback_body;
  /*
   * A typed receiver preflight imports the same real Clang declaration into a
   * temporary syntax-only translation unit. This keeps callback validation
   * ahead of Interpreter::ParseAndExecute without copying the class text.
   */
  auto syntax_args = interpreter_arguments;
  syntax_args.push_back("-fsyntax-only");
  auto syntax_pch_args = syntax_args;
  if (syntax_pch)
    tracer::callback_pch::append_syntax_args(syntax_pch_args, *syntax_pch);
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
  CallbackShape shape;
  std::string bridge;
  std::string complete_source;
  std::string baseline_error;
  DiagnosticCapture user_capture(nullptr);
  auto user_ast =
      build_callback_ast(user_source, "clang-tracer-callback.cc", user_capture);
  bool shape_valid = false;
  if (user_ast) {
    const auto context_declarations =
        tracer::analyze_context_declarations(user_ast->getASTContext());
    if (context_declarations.requires_context) {
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
      return compile_shared_probe(
          p, specification, interpreter_arguments,
          forced_include_source + prelude + context_prelude +
              tracer::kEmbeddedCallbackHeader,
          forced_include_source,
          signature_assert, error, run_initializers, report, result_sret,
          syntax_only);
    }
    shape_valid = inspect_callback_shape(user_ast->getASTContext(), shape,
                                         baseline_error);
  } else {
    baseline_error = user_capture.message();
  }
  if (!user_capture.message().empty() && baseline_error.empty()) {
    baseline_error = user_capture.message();
  }

  bool baseline_preflight_succeeded = false;
  DiagnosticCapture complete_capture(nullptr);
  if (shape_valid) {
    if (!validate_specialized_signature(user_ast->getASTContext(), shape,
                                        specification, error))
      return false;
    if (shape.class_mode) {
      bridge = make_class_bridge(shape);
    } else {
      bridge = tracer::make_legacy_callback_bridge();
    }
    const std::string generated = make_wrapper(
        p.request, reinterpret_cast<std::uintptr_t>(p.trampoline),
        shape.has_pre, shape.has_post, &specification, shape.class_mode,
        false, result_sret);
    complete_source = user_source + "\n" +
                      specialized_signature_assert(user_ast->getASTContext(),
                                                   shape) +
                      bridge + "\n" + generated;
    auto complete_ast = build_callback_ast(
        complete_source, "clang-tracer-callback.cc", complete_capture);
    baseline_preflight_succeeded =
        complete_ast && !complete_capture.had_error();
    if (!baseline_preflight_succeeded) {
      baseline_error = complete_capture.message();
      if (baseline_error.empty()) {
        baseline_error = "callback source failed Clang syntax validation";
      }
    }
  }

  if (baseline_preflight_succeeded && report) {
    append_diagnostics(*report, complete_capture, "wrapper");
  }

  if (!baseline_preflight_succeeded) {
    if (!p.receiver_ast) {
      if (report) {
        append_diagnostics(*report, user_capture, "syntax");
        append_diagnostics(*report, complete_capture, "wrapper");
      }
      error = baseline_error.empty()
                  ? "callback source failed Clang syntax validation"
                  : baseline_error;
      return false;
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
      if (auto err = importer.ImportDefinition(entry.decl)) {
        error = llvm::toString(std::move(err));
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
    if (receiver_record) {
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
        return false;
      }
      if (!create_receiver_alias(p.interpreter->getASTContext(),
                                 imported_receiver, specification.receiver())) {
        error = "cannot create typed receiver alias in interpreter AST";
        return false;
      }
    }

    std::string typed_user_source =
        prelude + context_prelude +
        (receiver_record && p.request.kind == tracer::CallableKind::Method
             ? "#define CLANG_TRACER_HAS_TYPED_RECEIVER 1\n"
             : "") +
        "#define CLANG_TRACER_FIELD_INFERENCE 1\n" +
        callback_header_source +
        (callback_headers_available
             ? std::string()
             : missing_recovered_record_declarations(
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
          receiver_record, &import_error, &p.debug_records, &specification);
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
    const std::string generated = make_wrapper(
        p.request, reinterpret_cast<std::uintptr_t>(p.trampoline),
        shape.has_pre, shape.has_post, &specification, shape.class_mode,
        false, result_sret);
    complete_source = typed_user_source + "\n" +
                      specialized_signature_assert(user_ast->getASTContext(),
                                                   shape) +
                      bridge + "\n" + generated;
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
          receiver_record, &import_error, &p.debug_records, &specification);
    };
    const bool complete_syntax_ok =
        run_callback_tool(make_final_action, complete_source, [&] {
          return final_shape.valid && import_error.empty();
        });
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

clangtracer::v1::Value to_proto_value(const tracer::TraceValue &v) {
  clangtracer::v1::Value out;
  out.set_type(static_cast<clangtracer::v1::ValueType>(v.type));
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

extern "C" unsigned long long
tracer_next_call_id(unsigned long long id) noexcept {
  if (auto *p = find_probe(id)) {
    return p->call_ids.fetch_add(1, std::memory_order_relaxed) + 1;
  }
  return 0;
}

extern "C" long long tracer_callback_dispatch(unsigned long long id,
                                              const tracer::TraceContext *ctx,
                                              void *instance) noexcept {
  auto *p = find_probe(id);
  if (!p || !ctx || !p->active.load(std::memory_order_seq_cst) ||
      !p->callback_enabled.load(std::memory_order_relaxed) || callback_depth) {
    return 0;
  }
  const int saved_errno = errno;
  ++callback_depth;
  long long value = 0;
  bool succeeded = false;
  tracer::TraceContext invocation = *ctx;
  invocation.named_count = 0;
  for (unsigned i = 0;
       i < invocation.argument_count && i < tracer::kMaxArguments; ++i)
    invocation.argument_names[i] =
        i < p->argument_names.size() ? p->argument_names[i].c_str() : nullptr;
  try {
    value = p->callback(&invocation, instance, &succeeded);
  } catch (...) {
    succeeded = false;
    // Class adapters disable only their failed invocation. Legacy callbacks
    // retain their existing probe-wide failure behavior.
    if (!p->class_callback)
      p->callback_enabled.store(false, std::memory_order_relaxed);
    p->callback_errors.fetch_add(1, std::memory_order_relaxed);
  }
  --callback_depth;
  if (succeeded) {
    tracer_event_dispatch(id, &invocation, value);
  }
  errno = saved_errno;
  return value;
}

extern "C" bool
tracer_callback_lifecycle_enter(unsigned long long id) noexcept {
  auto *p = find_probe(id);
  if (callback_depth || !p || !p->active.load(std::memory_order_seq_cst) ||
      !p->callback_enabled.load(std::memory_order_relaxed))
    return false;
  ++callback_depth;
  return true;
}
extern "C" void tracer_callback_cleanup_enter() noexcept { ++callback_depth; }
extern "C" void tracer_callback_lifecycle_leave() noexcept { --callback_depth; }
extern "C" void tracer_callback_failure(unsigned long long id) noexcept {
  if (auto *p = find_probe(id))
    p->callback_errors.fetch_add(1, std::memory_order_relaxed);
}

extern "C" void tracer_capture_stack(const tracer::TraceContext *ctx,
                                     unsigned depth,
                                     bool application_only) noexcept {
  if (!ctx)
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

extern "C" void tracer_event_dispatch(unsigned long long id,
                                      const tracer::TraceContext *ctx,
                                      long long cb) noexcept {
  const int saved_errno = errno;
  auto *p = find_probe(id);
  if (callback_depth || !p || !ctx ||
      !p->active.load(std::memory_order_seq_cst) || p->delivery.socket_fd < 0) {
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
    event.set_phase(ctx->phase == tracer::TracePhase::Pre
                        ? clangtracer::v1::PRE
                        : clangtracer::v1::POST);
    event.set_call_id(ctx->call_id);
    event.set_pid(static_cast<std::uint32_t>(::getpid()));
    event.set_tid(static_cast<std::uint32_t>(::syscall(SYS_gettid)));
    event.set_probe_sequence(sequence);
    timespec now{};
    if (::clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
      event.set_monotonic_time_ns(
          static_cast<std::uint64_t>(now.tv_sec) * 1000000000ULL + now.tv_nsec);
    }
    event.set_object_address(ctx->object_address);
    for (std::uint32_t i = 0;
         i < ctx->argument_count && i < tracer::kMaxArguments; ++i) {
      *event.add_arguments() = to_proto_value(ctx->arguments[i]);
    }
    if (ctx->named_count > 16) {
      throw std::invalid_argument("invalid named value count");
    }
    for (std::uint32_t i = 0; i < ctx->named_count; ++i) {
      const auto &value = ctx->named_values[i];
      const auto length = ::strnlen(value.name, sizeof(value.name));
      if (length == 0 || length == sizeof(value.name) ||
          (value.kind != 1 && value.kind != 2)) {
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
    event.set_callback_value(cb);
    if (ctx->stack.status != tracer::StackStatus::Disabled)
      tracer::serialize_native_stack(ctx->stack, *event.mutable_stack());
    if (ctx->phase == tracer::TracePhase::Post) {
      *event.mutable_result() = to_proto_value(ctx->result);
    }
    event.set_agent_dropped_events(
        p->delivery.dropped.load(std::memory_order_relaxed));
    const auto size = event.ByteSizeLong();
    if (size <= tracer::EventTransport::kMaxEventBytes) {
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
  if (r.argument_count > tracer::kMaxArguments) {
    error = "at most eight explicit arguments are supported";
    return false;
  }
  if (!valid_type(r.result, true)) {
    error = "unsupported result type";
    return false;
  }
  for (unsigned i = 0; i < r.argument_count; ++i) {
    if (!valid_type(r.arguments[i], false)) {
      error = "unsupported argument type";
      return false;
    }
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

extern "C" int tracer_agent_start(tracer::Request *request) {
  try {
    tracer::context_scheduler::ScopedPthreadBypass compiler_threads;
    if (!request || request->version != tracer::kBootstrapVersion) {
      return -1;
    }
    request->error[0] = '\0';
    std::string error;
    if (!validate_request(*request, error)) {
      set_error(request, error.c_str());
      return -1;
    }
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
      return -1;
    }
    if (find_probe(request->probe_id)) {
      set_error(request, "probe_id has already been used in this process");
      return -1;
    }
    if (a.retained >= tracer::kMaxProbes) {
      set_error(request, "the process has reached its 32 retained-probe limit");
      return -1;
    }
    // A durable collector can reconnect while old probes are still active.
    // Keep their shared metadata alive; refresh a new session only after the
    // previous callback cohort has drained.
    bool native_cache_quiescent = true;
    for (std::size_t i = 0; i < a.retained; ++i) {
      const auto *prior = a.probes[i].get();
      if (prior &&
          (prior->active.load(std::memory_order_seq_cst) ||
           prior->in_flight.load(std::memory_order_seq_cst) ||
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
    if (!read_probe_spec(p->request, specification, error)) {
      set_error(request, error.c_str());
      return -1;
    }
    // Named tracing uses the target's own independently re-read DWARF and ELF
    // mappings. No dynamic symbol lookup is involved, including for local and
    // hidden functions. Raw-symbol requests retain their explicit dlsym path.
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
      return -1;
    }
    p->request.original_entry = reinterpret_cast<std::uintptr_t>(entry);
    int prot = 0;
    std::uintptr_t map_lo = 0, map_hi = 0;
    if (!mapping(entry, map_lo, map_hi, prot) || (prot & PROT_EXEC) == 0) {
      set_error(request, "target symbol is not in an executable mapping");
      return -1;
    }
    Probe *replaced = nullptr;
    for (std::size_t i = 0; i < a.retained; ++i) {
      auto *other = a.probes[i].get();
      if (other && other->active.load() &&
          other->request.original_entry == p->request.original_entry) {
        if (other->session_id != request->session_id || replaced ||
            std::memcmp(entry, other->request.installed,
                        other->request.patch_size) != 0) {
          set_error(request,
                    "target symbol address is already hooked by another probe");
          return -1;
        }
        replaced = other;
      }
    }
    if (replaced) {
      // Compile the new callback before touching the live hook. Decode neither
      // its detour nor its wrapper: the trampoline needs the original prologue.
      p->request.replaced_probe_id = replaced->request.probe_id;
      p->request.patch_size = replaced->request.patch_size;
      std::memcpy(p->request.original, replaced->request.original,
                  p->request.patch_size);
    } else if (!decode_patch(static_cast<const std::uint8_t *>(entry), p->request,
                             error)) {
      set_error(request, error.c_str());
      return -1;
    }
    if (specification.has_debug_info() &&
        p->request.patch_size > specification.debug_info().entry_size()) {
      set_error(request, "safe patch window exceeds the DWARF function range");
      return -1;
    }
    for (std::size_t i = 0; i < a.retained; ++i) {
      auto *other = a.probes[i].get();
      if (!other || other == replaced || !other->active.load()) {
        continue;
      }
      const auto x = other->request.original_entry,
                 y = p->request.original_entry;
      const auto xe = x + other->request.patch_size,
                 ye = y + p->request.patch_size;
      if (x < ye && y < xe) {
        set_error(request, "target patch overlaps an already hooked function");
        return -1;
      }
    }
    long page_size = ::sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
      set_error(request, "could not determine page size");
      return -1;
    }
    p->mapping_size = static_cast<std::size_t>(page_size);
    p->trampoline = ::mmap(nullptr, p->mapping_size, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p->trampoline == MAP_FAILED) {
      set_error(request, "could not allocate trampoline");
      return -1;
    }
    std::memcpy(p->trampoline, p->request.original, p->request.patch_size);
    add_jump(static_cast<std::uint8_t *>(p->trampoline) + p->request.patch_size,
             static_cast<std::uint8_t *>(entry) + p->request.patch_size);
    if (::mprotect(p->trampoline, p->mapping_size, PROT_READ | PROT_EXEC)) {
      set_error(request, "could not make trampoline executable");
      return -1;
    }
    if (!compile_probe(
            *p, specification, error, true, false, nullptr,
            verified_debug_entry ? &*verified_debug_entry : nullptr)) {
      set_error(request, error.c_str());
      return -1;
    }
    if (p->request.context_required && !prepare_context_pthread(a, *p, error)) {
      set_error(request, error.c_str());
      return -1;
    }
    p->delivery.socket_fd =
        ::socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (p->delivery.socket_fd < 0) {
      set_error(request, "could not create event socket");
      return -1;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (std::strlen(request->event_socket) >= sizeof(addr.sun_path)) {
      set_error(request, "event socket path is too long");
      return -1;
    }
    std::strncpy(addr.sun_path, request->event_socket,
                 sizeof(addr.sun_path) - 1);
    if (::connect(p->delivery.socket_fd, reinterpret_cast<sockaddr *>(&addr),
                  sizeof(addr))) {
      set_error(request, "could not connect event socket");
      return -1;
    }
    if (p->request.context_required) {
      if (!a.pthread_detour || !a.pthread_detour->enable_propagation()) {
        set_error(request,
                  "native pthread_create propagation adapter is unavailable");
        return -1;
      }
      p->scheduler_counted = true;
      ++a.context_probe_count;
    }
    p->session_id = request->session_id;
    a.pid = ::getpid();
    const auto slot = a.retained;
    std::array<std::uint8_t, tracer::kMaxPatchBytes> patch{};
    add_jump(patch.data(), p->wrapper);
    std::fill(patch.begin() + kJumpBytes, patch.begin() + p->request.patch_size,
              0x90);
    std::memcpy(p->request.installed, patch.data(), p->request.patch_size);
    a.transport.activate();
    p->transport_active = true;
    p->active.store(true, std::memory_order_seq_cst);
    a.probes[slot] = std::move(p);
    Probe *installed_probe = a.probes[slot].get();
    a.probe_slots[slot].store(installed_probe, std::memory_order_release);
    ++a.retained;
    *request = installed_probe->request;
    return 0;
  } catch (const std::exception &e) {
    set_error(request, e.what());
    return -1;
  } catch (...) {
    set_error(request, "unexpected exception while adding probe");
    return -1;
  }
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
    std::string serialized(kMaxSerializedProbeSpec + 1, '\0');
    input.read(serialized.data(), static_cast<std::streamsize>(serialized.size()));
    const auto serialized_size = static_cast<std::size_t>(input.gcount());
    if (serialized_size > kMaxSerializedProbeSpec) {
      set_error("serialized validation ProbeSpec exceeds 256 KiB");
      return -1;
    }
    serialized.resize(serialized_size);
    clangtracer::v1::ProbeSpec spec;
    if (!spec.ParseFromArray(serialized.data(),
                             static_cast<int>(serialized.size())) ||
        !spec.has_signature() || spec.callback_source().empty() ||
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
    for (int i = 0; i < spec.signature().arguments_size(); ++i)
      native_request.arguments[i] = static_cast<tracer::ValueType>(
          spec.signature().arguments(i));
    for (const auto &option : spec.compile_options())
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
      success = compile_probe(prepared, spec, error, false, syntax_only,
                              &report);
    }
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
  try {
    if (!request || request->version != tracer::kBootstrapVersion) {
      return -1;
    }
    std::lock_guard<std::mutex> lock(agent_mutex);
    auto *current = agent.load(std::memory_order_acquire);
    if (!current) {
      set_error(request, "agent is not initialized");
      return -1;
    }
    Agent &a = *current;
    auto *p = find_probe(request->probe_id);
    if (!p) {
      set_error(request, "probe_id is unknown");
      return -1;
    }
    if (std::strncmp(request->session_id, p->request.session_id,
                     sizeof(request->session_id)) != 0) {
      set_error(request, "probe session id does not match");
      return -1;
    }
    void *entry = reinterpret_cast<void *>(p->request.original_entry);
    std::uintptr_t lo = 0, hi = 0;
    int prot = 0;
    if (!mapping(entry, lo, hi, prot) ||
        reinterpret_cast<std::uintptr_t>(entry) + p->request.patch_size > hi ||
        !(prot & PROT_EXEC)) {
      set_error(request, "remove requires the original executable mapping");
      return -2;
    }
    if (std::memcmp(entry, p->request.original, p->request.patch_size) != 0) {
      set_error(request, "remove requires the caller to restore original "
                         "bytes first");
      return -2;
    }
    p->active.exchange(false, std::memory_order_seq_cst);
    if (p->scheduler_counted) {
      p->scheduler_counted = false;
      if (a.context_probe_count != 0)
        --a.context_probe_count;
      if (a.context_probe_count == 0 && a.pthread_detour)
        a.pthread_detour->disable_propagation();
    }
    tracer::EventTransport::begin_drain(p->delivery);
    const auto scheduler_tasks =
        a.pthread_detour ? a.pthread_detour->outstanding_tasks() : 0;
    p->request.pthread_create_active_tasks =
        static_cast<std::uint32_t>(std::min<std::size_t>(
            scheduler_tasks, std::numeric_limits<std::uint32_t>::max()));
    if (p->in_flight.load(std::memory_order_seq_cst) ||
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
    return 0;
  } catch (const std::exception &e) {
    set_error(request, e.what());
    return -1;
  } catch (...) {
    set_error(request, "unexpected exception while removing probe");
    return -1;
  }
}

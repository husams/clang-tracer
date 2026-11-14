#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <functional>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include <sys/types.h>

#include "admission.hpp"
#include "tracer/compat.hpp"

namespace tracer::object_launch { class Child; }

namespace tracer::injector {

struct CodePatch {
  std::uintptr_t address = 0;
  const void *expected = nullptr;
  const void *replacement = nullptr;
  std::size_t size = 0;
};

struct CodeRange {
  std::uintptr_t address = 0;
  std::size_t size = 0;
  // A saved return address may equal the end if a call ends there.
  bool return_at_end = true;
};

// The caller owns the ptrace stop and has excluded other executing threads.
// Used by the object watch for a one-byte, word-preserving breakpoint patch.
void write_stopped_code_byte(pid_t tid, std::uintptr_t address, unsigned char value);

class IdlePointUnavailable : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class InitialAdmissionUnavailable final : public IdlePointUnavailable {
public:
  using IdlePointUnavailable::IdlePointUnavailable;
};

// A deliberately narrow Linux x86_64 ptrace controller. attach() admits a
// sleeping task or a CPU-busy task within a validated application call chain.
// Admission does not certify arbitrary loader-lock state.
class Process final {
public:
  Process() noexcept = default;
  ~Process();

  Process(const Process &) = delete;
  Process &operator=(const Process &) = delete;
  Process(Process &&other) noexcept;
  Process &operator=(Process &&other) noexcept;

  void attach(pid_t process_id,
              ::tracer::compat::span<const ApplicationModule> application_modules = {});
  // A stopped task suffices for code restoration. This mode cannot run
  // remote calls or syscalls, and does not require a loader rendezvous.
  void attach_for_patch(pid_t process_id);
  void attach_at_application_point(pid_t process_id, std::uintptr_t entry, std::size_t size);
  void adopt_executable_entry(object_launch::Child &child,
                              std::chrono::steady_clock::time_point deadline,
                              bool loader_ready = false, std::string_view startup_module = {});
  void detach();

  // Prevent normal detach/resume after the caller detects an unsafe target
  // state.
  void mark_unsafe() noexcept;

  [[nodiscard]] bool attached() const noexcept { return tid_ > 0; }

  [[nodiscard]] bool unsafe() const noexcept { return poisoned_; }

  [[nodiscard]] pid_t pid() const noexcept { return process_id_; }

  [[nodiscard]] std::uintptr_t instruction_pointer() const;

  [[nodiscard]] std::uintptr_t remote_allocate(std::size_t size);
  void remote_read(std::uintptr_t address, void *destination,
                   std::size_t size) const;
  void remote_write(std::uintptr_t address, const void *source,
                    std::size_t size);
  // Atomically validate and commit a code patch while all peer threads are
  // stopped. Returns false if a peer or the leader is executing inside the
  // patch (the patch entry address itself is safe).
  [[nodiscard]] bool patch_code(std::uintptr_t address, const void *expected,
                                const void *replacement, std::size_t size);
  // Validates and commits the complete disjoint patch set while all target
  // threads are stopped. A precondition/active-instruction failure changes no
  // bytes; a write failure rolls back every range before resuming threads.
  // The optional commit hook runs after verification and before peers resume.
  // It must not throw.
  [[nodiscard]] bool patch_code_batch(::tracer::compat::span<const CodePatch> patches,
                                     const std::function<void()> &validate_mappings = {},
                                     const std::function<void()> &on_commit = {},
                                     ::tracer::compat::span<const CodeRange> drained_ranges = {});
  void remote_free(std::uintptr_t address, std::size_t size);

  [[nodiscard]] std::uintptr_t
  remote_call(std::uintptr_t function,
              const std::array<std::uintptr_t, 6> &arguments = {},
              std::size_t argument_count = 0);

  [[nodiscard]] std::uintptr_t resolve_remote_symbol(
      std::string_view name, const CodePatch *owned_patch = nullptr);

private:
  struct PeerFreeze;
  pid_t process_id_ = -1;
  pid_t tid_ = -1;
  std::vector<std::pair<pid_t, int>> traced_peers_;
  int pending_signal_ = 0;
  std::uintptr_t scratch_stack_ = 0;
  std::size_t scratch_stack_size_ = 0;
  std::uintptr_t call_page_ = 0;
  std::size_t call_page_size_ = 0;
  bool poisoned_ = false;
  bool tracee_stopped_ = false;
  bool patch_only_ = false;

  void attach_impl(pid_t process_id,
                   ::tracer::compat::span<const ApplicationModule> application_modules,
                   bool patch_only, std::uintptr_t rendezvous = 0, std::size_t rendezvous_size = 0);
  void ensure_attached() const;
  void ensure_usable() const;
  void poison_process() noexcept;
  [[nodiscard]] std::uintptr_t
  remote_syscall(long number, const std::array<std::uintptr_t, 6> &arguments);
  [[nodiscard]] std::uintptr_t
  remote_call_raw(std::uintptr_t function,
                  const std::array<std::uintptr_t, 6> &arguments,
                  std::size_t argument_count);
  void preserve_poisoned_stop();
};

} // namespace tracer::injector

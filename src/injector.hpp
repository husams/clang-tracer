#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "admission.hpp"

#include <sys/types.h>

namespace tracer::injector {

class IdlePointUnavailable : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

// A deliberately narrow Linux x86_64 ptrace controller. attach() admits a
// sleeping leader or a CPU-busy leader within a validated application call
// chain. Admission does not certify arbitrary loader-lock state.
class Process final {
public:
  Process() noexcept = default;
  ~Process();

  Process(const Process &) = delete;
  Process &operator=(const Process &) = delete;
  Process(Process &&other) noexcept;
  Process &operator=(Process &&other) noexcept;

  void attach(pid_t process_id,
              std::span<const ApplicationModule> application_modules = {});
  void detach();

  // Prevent normal detach/resume after the caller detects an unsafe target
  // state.
  void mark_unsafe() noexcept;

  [[nodiscard]] bool attached() const noexcept { return tid_ > 0; }

  [[nodiscard]] bool unsafe() const noexcept { return poisoned_; }

  [[nodiscard]] pid_t pid() const noexcept { return tid_; }

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
  void remote_free(std::uintptr_t address, std::size_t size);

  [[nodiscard]] std::uintptr_t
  remote_call(std::uintptr_t function,
              const std::array<std::uintptr_t, 6> &arguments = {},
              std::size_t argument_count = 0);

  [[nodiscard]] std::uintptr_t resolve_remote_symbol(std::string_view name);

private:
  struct PeerFreeze;
  pid_t tid_ = -1;
  std::vector<std::pair<pid_t, int>> traced_peers_;
  int pending_signal_ = 0;
  std::uintptr_t scratch_stack_ = 0;
  std::size_t scratch_stack_size_ = 0;
  std::uintptr_t call_page_ = 0;
  std::size_t call_page_size_ = 0;
  bool poisoned_ = false;
  bool tracee_stopped_ = false;

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

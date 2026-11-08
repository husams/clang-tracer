#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <sys/types.h>
#include <sys/user.h>

#include "tracer/compat.hpp"

namespace tracer::injector {

struct RuntimeEntry {
  std::uintptr_t address = 0;
  std::array<unsigned char, 32> bytes{};
};

// Resolve only bootstrap and scheduler entry points in the target's glibc.
// The caller must compare these file bytes to the stopped target before use.
RuntimeEntry target_runtime_entry(pid_t pid, std::string_view name);
std::vector<std::string> target_runtime_companions(
    pid_t pid, const std::vector<std::string> &agent_dependencies);

namespace detail {
struct ApprovedModuleIdentity;
}

class ApplicationModule final {
public:
  ApplicationModule() noexcept = default;
  [[nodiscard]] const std::string &path() const noexcept;

private:
  explicit ApplicationModule(
      std::shared_ptr<const detail::ApprovedModuleIdentity> identity);
  std::shared_ptr<const detail::ApprovedModuleIdentity> identity_;

  friend std::vector<ApplicationModule> discover_application_modules(pid_t);
  friend std::size_t
  require_application_rendezvous(pid_t, const user_regs_struct&,
                                 ::tracer::compat::span<const ApplicationModule>);
  friend void validate_application_modules(pid_t, ::tracer::compat::span<const ApplicationModule>);
};

// Snapshot canonical mapped application paths once per session. Returned
// values retain the ELF path and load bias for later mapping validation.
std::vector<ApplicationModule> discover_application_modules(pid_t pid);

// Revalidate the session's module paths and ELF mappings after each target stop.
void validate_application_modules(pid_t pid,
                                  ::tracer::compat::span<const ApplicationModule> modules);

// Admit a stopped x86_64 process when its DWARF CFI chain stays in the main
// executable or this session's discovered modules, then reaches libc startup.
// Returns the application-frame count; uncertainty throws IdlePointUnavailable.
std::size_t
require_application_rendezvous(pid_t pid, const user_regs_struct& regs,
                               ::tracer::compat::span<const ApplicationModule> modules = {});

} // namespace tracer::injector

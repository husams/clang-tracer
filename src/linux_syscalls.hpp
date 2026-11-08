#pragma once

#include <cstddef>

#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>

namespace tracer::linux_api {
// Some old SDK libcs export ENOSYS stubs despite a capable running kernel.
// These are the Linux x86-64 UAPI numbers; native tracing already requires
// x86-64 and a kernel supporting the seize/interrupt and remote-memory APIs.
#ifdef SYS_process_vm_readv
inline constexpr long kProcessVmReadv = SYS_process_vm_readv;
#else
inline constexpr long kProcessVmReadv = 310;
#endif
#ifdef SYS_process_vm_writev
inline constexpr long kProcessVmWritev = SYS_process_vm_writev;
#else
inline constexpr long kProcessVmWritev = 311;
#endif
inline ssize_t process_vm_readv(pid_t pid, const iovec* local, std::size_t local_count,
                                const iovec* remote, std::size_t remote_count,
                                unsigned long flags) noexcept {
  return ::syscall(kProcessVmReadv, pid, local, local_count, remote, remote_count, flags);
}

inline ssize_t process_vm_writev(pid_t pid, const iovec* local, std::size_t local_count,
                                 const iovec* remote, std::size_t remote_count,
                                 unsigned long flags) noexcept {
  return ::syscall(kProcessVmWritev, pid, local, local_count, remote, remote_count, flags);
}
} // namespace tracer::linux_api

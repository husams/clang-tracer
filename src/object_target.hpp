#pragma once

#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include <sys/ptrace.h>
#if defined(__aarch64__)
#include <asm/ptrace.h>
#include <elf.h>
#include <sys/uio.h>
#else
#include <sys/user.h>
#endif

namespace tracer::object_target {
using Address = std::uint64_t;

inline void control(__ptrace_request request, pid_t tid, void *data = nullptr) {
  if (::ptrace(request, tid, nullptr, data) == -1)
    throw std::system_error(errno, std::generic_category(), "object watch ptrace");
}

#if defined(__aarch64__)
using Registers = user_pt_regs;
inline constexpr std::string_view trap{"\0\0\x20\xd4", 4}; // BRK #0
inline constexpr std::string_view system_call{"\x01\0\0\xd4", 4}; // SVC #0
inline constexpr Address return_stack_delta = 0;
inline Address pc(const Registers &regs) { return regs.pc; }
inline Address sp(const Registers &regs) { return regs.sp; }
inline Address fp(const Registers &regs) { return regs.regs[29]; }
inline Address argument(const Registers &regs, unsigned index) { return regs.regs[index]; }
inline Address result(const Registers &regs) { return regs.regs[0]; }
inline bool zero(const Registers &regs) { return regs.pstate & (1ULL << 30); }
inline Address trap_pc(const Registers &regs) { return regs.pc; }
template <typename Read>
Address return_pc(const Registers &regs, Read &&) { return regs.regs[30]; }

inline Registers read_registers(pid_t tid) {
  Registers regs{};
  iovec buffer{&regs, sizeof(regs)};
  if (::ptrace(PTRACE_GETREGSET, tid, NT_PRSTATUS, &buffer) == -1 ||
      buffer.iov_len != sizeof(regs))
    throw std::runtime_error("cannot read AArch64 object watch registers");
  return regs;
}

inline void set_pc(pid_t tid, Registers regs, Address address) {
  regs.pc = address;
  iovec buffer{&regs, sizeof(regs)};
  if (::ptrace(PTRACE_SETREGSET, tid, NT_PRSTATUS, &buffer) == -1)
    throw std::system_error(errno, std::generic_category(), "set AArch64 object watch PC");
}

inline Address reg(const Registers &regs, std::string_view name, Address) {
  if (name.empty() || name == "XZR" || name == "WZR")
    return 0;
  if (name == "SP" || name == "WSP")
    return name == "SP" ? regs.sp : static_cast<std::uint32_t>(regs.sp);
  if (name == "FP")
    return fp(regs);
  if (name == "LR")
    return regs.regs[30];
  unsigned index = 31;
  if (name.size() >= 2 && (name[0] == 'X' || name[0] == 'W')) {
    const auto end = name.data() + name.size();
    const auto parsed = std::from_chars(name.data() + 1, end, index);
    if (parsed.ec == std::errc{} && parsed.ptr == end && index < 31)
      return name[0] == 'X' ? regs.regs[index] : static_cast<std::uint32_t>(regs.regs[index]);
  }
  throw std::runtime_error("unsupported AArch64 register " + std::string(name));
}
#else
using Registers = user_regs_struct;
inline constexpr std::string_view trap{"\xcc", 1};
inline constexpr std::string_view system_call{"\x0f\x05", 2};
inline constexpr Address return_stack_delta = sizeof(Address);
inline Address pc(const Registers &regs) { return regs.rip; }
inline Address sp(const Registers &regs) { return regs.rsp; }
inline Address fp(const Registers &regs) { return regs.rbp; }
inline Address argument(const Registers &regs, unsigned index) { return index ? regs.rsi : regs.rdi; }
inline Address result(const Registers &regs) { return regs.rax; }
inline bool zero(const Registers &regs) { return regs.eflags & (1UL << 6); }
inline Address trap_pc(const Registers &regs) { return regs.rip - 1; }
template <typename Read>
Address return_pc(const Registers &regs, Read &&read) { return read(regs.rsp); }

inline Registers read_registers(pid_t tid) {
  Registers regs{};
  control(PTRACE_GETREGS, tid, &regs);
  return regs;
}

inline void set_pc(pid_t tid, Registers regs, Address address) {
  regs.rip = address;
  control(PTRACE_SETREGS, tid, &regs);
}

inline Address reg(const Registers &regs, std::string_view name, Address next) {
  using Register = decltype(regs.rax) Registers::*;
  struct NamedRegister {
    const char *full, *low;
    Register member;
  };
  static constexpr NamedRegister registers[]{
      {"RAX", "EAX", &Registers::rax}, {"RCX", "ECX", &Registers::rcx},
      {"RDX", "EDX", &Registers::rdx}, {"RBX", "EBX", &Registers::rbx},
      {"RSP", "ESP", &Registers::rsp}, {"RBP", "EBP", &Registers::rbp},
      {"RSI", "ESI", &Registers::rsi}, {"RDI", "EDI", &Registers::rdi},
      {"R8", "R8D", &Registers::r8}, {"R9", "R9D", &Registers::r9},
      {"R10", "R10D", &Registers::r10}, {"R11", "R11D", &Registers::r11},
      {"R12", "R12D", &Registers::r12}, {"R13", "R13D", &Registers::r13},
      {"R14", "R14D", &Registers::r14}, {"R15", "R15D", &Registers::r15}};
  if (name.empty())
    return 0;
  if (name == "RIP")
    return next;
  for (const auto &[full, low, member] : registers)
    if (name == full || name == low)
      return regs.*member;
  throw std::runtime_error("unsupported x86-64 register " + std::string(name));
}
#endif

inline void write_stopped_code(pid_t tid, Address address, std::string_view bytes) {
  constexpr auto mask = sizeof(long) - 1;
  const auto start = address & ~mask;
  const auto offset = address - start;
  if (bytes.empty() || bytes.size() > sizeof(long) - offset)
    throw std::runtime_error("object watch patch crosses a ptrace word");
  errno = 0;
  long word = ::ptrace(PTRACE_PEEKTEXT, tid, start, nullptr);
  if (word == -1 && errno)
    throw std::system_error(errno, std::generic_category(), "read object watch code word");
  std::memcpy(reinterpret_cast<char *>(&word) + offset, bytes.data(), bytes.size());
  if (::ptrace(PTRACE_POKETEXT, tid, start, word) == -1)
    throw std::system_error(errno, std::generic_category(), "write object watch code word");
}
} // namespace tracer::object_target

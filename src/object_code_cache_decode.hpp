#pragma once

#include "native_entry.hpp"
#include "object_code_cache_protocol.hpp"

#include <charconv>
#include <cstring>
#include <iterator>
#include <set>
#include <stdexcept>
#include <vector>

namespace tracer::object_cache {
using NativeFlow = native_entry::Flow;

#if defined(__x86_64__)
struct X86IndirectCall {
  std::size_t opcode_offset;
  unsigned reg;
  bool rip_relative;
};

inline std::optional<X86IndirectCall> x86_indirect_call(const Instruction &instruction,
                                                       unsigned operation = 0x10) {
  const auto &bytes = instruction.bytes;
  const bool prefixed = (bytes[0] & 0xf0) == 0x40;
  const std::size_t offset = prefixed ? 1 : 0;
  const auto rex = prefixed ? bytes[0] : 0;
  if (instruction.size < offset + 2 || bytes[offset] != 0xff ||
      (bytes[offset + 1] & 0x38) != operation || (rex & 4))
    return std::nullopt;
  const auto modrm = bytes[offset + 1];
  const auto rm = modrm & 7;
  if (modrm >= 0xc0) {
    static constexpr unsigned registers[] = {rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi};
    return X86IndirectCall{offset, (rex & 1) ? r8 + rm : registers[rm], false};
  }
  if (rm == 4 && (instruction.size < offset + 3 ||
                  ((bytes[offset + 2] & 7) == 4 && !(rex & 1))))
    return std::nullopt; // The generated operand load temporarily borrows RSP.
  const bool rip_relative = modrm < 0x40 && rm == 5;
  return X86IndirectCall{offset, 0, rip_relative};
}
#endif

inline unsigned register_index(std::string_view name, bool address) {
#if defined(__aarch64__)
  if (name.empty() || name == "WZR" || name == "XZR")
    return 0;
  if (address && name == "SP")
    return rsp;
  if (address && name == "FP")
    return 30;
  if (address && name == "LR")
    return 31;
  if (name.front() == (address ? 'X' : 'W')) {
    unsigned index = 0;
    const auto end = name.data() + name.size();
    const auto parsed = std::from_chars(name.data() + 1, end, index);
    if (parsed.ec == std::errc{} && parsed.ptr == end && index < 31)
      return index + 1;
  }
#else
  static constexpr std::string_view full[] = {"", "RAX", "RCX", "RDX", "RBX", "RBP",
      "RSI", "RDI", "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15", "RSP"};
  static constexpr std::string_view low[] = {"", "EAX", "ECX", "EDX", "EBX", "EBP",
      "ESI", "EDI", "R8D", "R9D", "R10D", "R11D", "R12D", "R13D", "R14D", "R15D", "ESP"};
  for (unsigned i = 0; i < std::size(full); ++i)
    if (name == (address ? full[i] : low[i]))
      return i;
#endif
  throw std::runtime_error("unsupported code-cache register " + std::string(name));
}

inline void instrument_store(Instruction &out, const native_entry::ScalarStore &store) {
  using Kind = native_entry::ScalarStore::Kind;
  out.hook = store.kind == Kind::move ? Hook::move :
      store.kind == Kind::exchange_add ? Hook::xadd : Hook::cas;
  out.base = register_index(store.base, true);
  out.index = register_index(store.index, true);
  out.value = register_index(store.value_register, false);
  out.result = register_index(store.result_register, false);
  if (out.hook == Hook::xadd && out.value == rsp)
    throw std::runtime_error("code cache cannot instrument XADD into the stack pointer");
  out.scale = store.scale;
  out.displacement = store.displacement;
  out.immediate = store.immediate.value_or(0);
}

template <typename Transform>
std::vector<Instruction> decode_leaf(Address start, std::string_view bytes,
                                     bool allow_rip_relative, Transform transform, bool follow_tail = false) {
  const auto decoded = native_entry::scan(start, bytes);
  if (decoded.decode_failed || !decoded.diagnostic.empty() ||
      decoded.instructions.empty())
    throw std::runtime_error("cannot verify a complete code-cache function");
  std::set<Address> starts;
  for (const auto &inst : decoded.instructions)
    starts.insert(inst.address);
  std::vector<Instruction> result;
  Address previous_end = 0;
  bool has_return = false;
  for (const auto &inst : decoded.instructions) {
    if (inst.address < previous_end ||
        (inst.falls_through && !starts.count(inst.address + inst.size)))
      throw std::runtime_error("code-cache function has an unverified instruction boundary");
    previous_end = inst.address + inst.size;
    const auto original = bytes.substr(inst.address - start, inst.size);
    Instruction out;
    out.pc = inst.address;
    out.size = inst.size;
    std::memcpy(out.bytes.data(), original.data(), original.size());
#if defined(__aarch64__)
    std::uint32_t opcode = 0;
    if (inst.size != sizeof(opcode))
      throw std::runtime_error("invalid AArch64 instruction width");
    std::memcpy(&opcode, out.bytes.data(), sizeof(opcode));
#else
    const auto opcode = out.bytes[0];
#endif
    switch (inst.flow) {
    case NativeFlow::ordinary:
      if ((!inst.cache_copyable && !(allow_rip_relative && inst.cache_rip_relative)) ||
          !inst.falls_through)
        throw std::runtime_error("unsupported code-cache instruction: " + inst.text);
      out.rip_relative = inst.cache_rip_relative;
      break;
    case NativeFlow::returns:
#if defined(__aarch64__)
      if (opcode != 0xd65f03c0U && opcode != 0xd65f0bffU && opcode != 0xd65f0fffU)
        throw std::runtime_error("AArch64 code cache requires a return using the link register");
#else
      if (opcode != 0xc3 || inst.size != 1)
        throw std::runtime_error("code cache requires an unprefixed near return");
#endif
      has_return = true;
      break;
    case NativeFlow::call:
      throw std::runtime_error("code cache requires a leaf function: " + inst.text);
    case NativeFlow::branch:
      if (!inst.target || !starts.count(*inst.target)) {
        if (!follow_tail || inst.falls_through ||
            (inst.target && *inst.target >= start && *inst.target - start < bytes.size()))
          throw std::runtime_error("code cache requires a verified in-function branch: " + inst.text);
        if (inst.target) {
#if defined(__aarch64__)
          if ((opcode & 0xfc000000U) != 0x14000000U)
#else
          if (!((opcode == 0xe9 && inst.size == 5) || (opcode == 0xeb && inst.size == 2)))
#endif
            throw std::runtime_error("unsupported direct tail branch: " + inst.text);
          out.target = *inst.target;
        } else {
#if defined(__aarch64__)
          if (opcode != 0xd61f0200U && opcode != 0xd61f0220U)
            throw std::runtime_error("AArch64 indirect tail branch requires an ABI veneer register");
          out.value = ((opcode >> 5) & 31) + 1;
#else
          const auto operand = x86_indirect_call(out, 0x20);
          if (!operand || !operand->reg)
            throw std::runtime_error("x86-64 tail branch requires a verified register target");
          out.value = operand->reg;
#endif
        }
        out.flow = Flow::runtime_tail;
        has_return = true; // The target inherits the existing application return.
        break;
      }
      out.target = *inst.target;
#if defined(__aarch64__)
      if ((opcode & 0xfc000000U) == 0x14000000U) {
        out.flow = Flow::jump;
      } else if ((opcode & 0xff000010U) == 0x54000000U || // B.cond
                 (opcode & 0x7e000000U) == 0x34000000U || // CBZ/CBNZ
                 (opcode & 0x7e000000U) == 0x36000000U) { // TBZ/TBNZ
        out.flow = Flow::conditional;
      } else {
        throw std::runtime_error("unsupported AArch64 code-cache branch: " + inst.text);
      }
#else
      if ((opcode == 0xe9 && inst.size == 5) || (opcode == 0xeb && inst.size == 2)) {
        out.flow = Flow::jump;
      } else if ((opcode >= 0x70 && opcode <= 0x7f && inst.size == 2) ||
                 (opcode == 0x0f && out.bytes[1] >= 0x80 && out.bytes[1] <= 0x8f &&
                  inst.size == 6)) {
        out.flow = Flow::conditional;
        out.condition = out.bytes[inst.size == 2 ? 0 : 1] & 0xf;
      } else {
        throw std::runtime_error("unsupported code-cache branch: " + inst.text);
      }
#endif
      break;
    }
    transform(inst, out);
    result.push_back(out);
  }
  if (!has_return)
    throw std::runtime_error("code-cache function has no verified near return");
  return result;
}

} // namespace tracer::object_cache

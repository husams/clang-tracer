#include "native_detour.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>

#include <llvm/ADT/SmallVector.h>
#include <llvm/MC/MCAsmBackend.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCCodeEmitter.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCDisassembler/MCDisassembler.h>
#include <llvm/MC/MCExpr.h>
#include <llvm/MC/MCFixup.h>
#include <llvm/MC/MCInst.h>
#include <llvm/MC/MCInstrAnalysis.h>
#include <llvm/MC/MCInstrInfo.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/MC/MCTargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <sys/mman.h>
#include <unistd.h>

namespace tracer::native_detour {
namespace {
using Wide = __int128_t;
constexpr auto kMinDisp = std::numeric_limits<std::int32_t>::min();
constexpr auto kMaxDisp = std::numeric_limits<std::int32_t>::max();
constexpr auto kRelayOffset = kMaxPatchBytes;

void write_absolute_jump(std::uint8_t *bytes, std::uintptr_t destination) {
  bytes[0] = 0xff;
  bytes[1] = 0x25;
  std::memset(bytes + 2, 0, 4);
  std::memcpy(bytes + 6, &destination, sizeof(destination));
}

bool valid_jump_size(std::size_t size) {
  return size == kJumpBytes || size == kRelativeJumpBytes;
}

bool fail(std::string &error, const char *message) {
  error = message;
  return false;
}

struct Decoder {
  std::unique_ptr<llvm::MCRegisterInfo> regs;
  std::unique_ptr<llvm::MCAsmInfo> asm_info;
  std::unique_ptr<llvm::MCSubtargetInfo> sub;
  std::unique_ptr<llvm::MCInstrInfo> ins;
  std::unique_ptr<llvm::MCContext> context;
  std::unique_ptr<llvm::MCDisassembler> dis;
  std::unique_ptr<llvm::MCInstrAnalysis> analysis;
  std::unique_ptr<llvm::MCCodeEmitter> emitter;
  std::unique_ptr<llvm::MCAsmBackend> backend;
};

bool make_decoder(Decoder &d, std::string &error) {
  if (!initialize_llvm(error)) {
    return false;
  }
  auto triple = llvm::sys::getDefaultTargetTriple();
  std::string lookup_error;
  auto *target = llvm::TargetRegistry::lookupTarget(triple, lookup_error);
  if (!target) {
    return fail(error, "LLVM native target lookup failed");
  }
  llvm::MCTargetOptions options;
  llvm::Triple parsed(triple);
  d.regs.reset(target->createMCRegInfo(triple));
  d.ins.reset(target->createMCInstrInfo());
  d.sub.reset(target->createMCSubtargetInfo(triple, "generic", ""));
  if (!d.regs || !d.ins || !d.sub) {
    return fail(error, "LLVM MC metadata unavailable");
  }
  d.asm_info.reset(target->createMCAsmInfo(*d.regs, triple, options));
  if (!d.asm_info) {
    return fail(error, "LLVM MC assembly metadata unavailable");
  }
  d.context = std::make_unique<llvm::MCContext>(parsed, d.asm_info.get(),
                                                d.regs.get(), d.sub.get());
  d.dis.reset(target->createMCDisassembler(*d.sub, *d.context));
  d.analysis.reset(target->createMCInstrAnalysis(d.ins.get()));
  d.emitter.reset(target->createMCCodeEmitter(*d.ins, *d.context));
  d.backend.reset(target->createMCAsmBackend(*d.sub, *d.regs, options));
  if (!d.dis || !d.analysis || !d.emitter || !d.backend) {
    return fail(error, "LLVM MC relocation metadata unavailable");
  }
  return true;
}

bool allowed_instruction(const llvm::MCInst &inst,
                         const llvm::MCInstrInfo &ins) {
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
      "ADD32ri8",  "ADD32ri",   "ADD32rr",    "IMUL32rri8", "IMUL32rri", "IMUL32rmi8",
      "IMUL32rmi", "XOR64rr",   "XOR32rr",    "TEST64rr",  "TEST32rr",
      "CMP64rr",   "CMP32rr",   "AND8i8",     "AND8ri",    "AND64ri8",   "AND64ri32", "XOR8i8", "XOR8ri", "NOP",
      "NOOP",      "MOV64mi32", "MOV32mi",    "MOV16mi",   "MOV8mi"};
  for (auto prefix : allowed) {
    if (name.starts_with(prefix)) {
      return true;
    }
  }
  // GCC compares spilled pointers with this instruction at -O0. A
  // RIP-relative form still needs a supported memory width to be relocated.
  return name == "CMP64mi8";
}

std::optional<std::size_t> memory_access_size(const llvm::MCInst &inst,
                                              const llvm::MCInstrInfo &ins) {
  const auto &desc = ins.get(inst.getOpcode());
  // LEA and memory-form NOP do not access the computed memory address.
  if (!desc.mayLoad() && !desc.mayStore())
    return 0;
  const std::string_view name = ins.getName(inst.getOpcode());
  struct Width {
    std::string_view prefix;
    std::size_t bytes;
  };
  static constexpr Width widths[] = {
      {"MOV64rm", 8},  {"MOV64mr", 8}, {"MOV64mi32", 8}, {"MOV32rm", 4},
      {"MOV32mr", 4},  {"MOV32mi", 4}, {"MOV16rm", 2},   {"MOV16mr", 2},
      {"MOV16mi", 2},  {"MOV8rm", 1},  {"MOV8mr", 1},    {"MOV8mi", 1},
      {"MOVSS", 4},    {"MOVSD", 8},   {"MOVAPS", 16},   {"MOVUPS", 16},
      {"IMUL32rmi", 4}};
  for (auto width : widths)
    if (name.starts_with(width.prefix))
      return width.bytes;
  return std::nullopt;
}

// Use the encoder's fixup location, not a guessed "last four bytes": stores
// with an immediate have additional bytes after the RIP displacement.
bool record_relocation(Decoder &d, llvm::MCInst inst,
                       std::span<const std::uint8_t> bytes, std::size_t offset,
                       std::size_t size, Patch &patch, std::string &error) {
  std::optional<unsigned> rip;
  for (unsigned i = 0; i < inst.getNumOperands(); ++i) {
    if (!inst.getOperand(i).isReg() || !inst.getOperand(i).getReg()) {
      continue;
    }
    const std::string_view name = d.regs->getName(inst.getOperand(i).getReg());
    if (name == "EIP") {
      return fail(error,
                  "target entry contains unsupported EIP-relative addressing");
    }
    if (name == "RIP") {
      if (rip) {
        return fail(error, "target entry has ambiguous RIP-relative operands");
      }
      rip = i;
    }
  }
  if (!rip) {
    return true;
  }
  const auto access_size = memory_access_size(inst, *d.ins);
  if (!access_size)
    return fail(error, "target RIP-relative memory width is unsupported");
  // X86 MC memory operands are base, scale, index, displacement, segment.
  const auto operand = *rip + 3;
  if (operand >= inst.getNumOperands() || !inst.getOperand(operand).isImm()) {
    return fail(error, "target RIP-relative displacement is unavailable");
  }
  const auto displacement = inst.getOperand(operand).getImm();
  const Wide target = Wide(patch.entry) + offset + size + displacement;
  const auto evaluated = d.analysis->evaluateMemoryOperandAddress(
      inst, d.sub.get(), patch.entry + offset, size);
  if (!evaluated || target < 0 ||
      target > std::numeric_limits<std::uintptr_t>::max() ||
      *evaluated != static_cast<std::uintptr_t>(target)) {
    return fail(error,
                "target entry contains unsupported RIP-relative addressing");
  }
  inst.getOperand(operand) = llvm::MCOperand::createExpr(
      llvm::MCSymbolRefExpr::create(d.context->createTempSymbol(), *d.context));
  llvm::SmallVector<char, 16> encoded;
  llvm::SmallVector<llvm::MCFixup, 1> fixups;
  d.emitter->encodeInstruction(inst, encoded, fixups, *d.sub);
  if (encoded.size() != size || fixups.size() != 1 ||
      !fixups.front().isPCRel()) {
    return fail(error, "target RIP-relative instruction cannot be relocated");
  }
  const auto &fixup = fixups.front();
  const auto info = d.backend->getFixupKindInfo(fixup.getKind());
  const auto field = fixup.getOffset();
  if (info.TargetOffset != 0 || info.TargetSize != 32 || field > size ||
      size - field < sizeof(std::int32_t)) {
    return fail(error,
                "target RIP-relative displacement is not a checked disp32");
  }
  std::int32_t original = 0;
  std::memcpy(&original, bytes.data() + offset + field, sizeof(original));
  if (original != displacement) {
    return fail(error, "target RIP-relative displacement encoding disagrees");
  }
  for (std::size_t i = 0; i < size; ++i) {
    if ((i < field || i >= field + sizeof(original)) &&
        static_cast<std::uint8_t>(encoded[i]) != bytes[offset + i]) {
      return fail(error, "target RIP-relative instruction does not round-trip");
    }
  }
  patch.relocations.push_back({offset + field, offset + size,
                               static_cast<std::uintptr_t>(target),
                               *access_size});
  return true;
}

void *allocate(const Patch &patch, std::size_t size, std::string &error) {
  if (patch.relocations.empty() && patch.jump_size == kJumpBytes) {
    return ::mmap(nullptr, size, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  }

  // Intersect the signed-disp32 constraints for every referenced address.
  Wide low = size,
       high = Wide(std::numeric_limits<std::uintptr_t>::max()) - size;
  for (const auto &r : patch.relocations) {
    const Wide origin = Wide(r.target) - r.instruction_end;
    low = std::max(low, origin - kMaxDisp);
    high = std::min(high, origin - kMinDisp);
  }
  if (patch.jump_size == kRelativeJumpBytes) {
    const Wide origin = Wide(patch.entry) + kRelativeJumpBytes - kRelayOffset;
    low = std::max(low, origin + kMinDisp);
    high = std::min(high, origin + kMaxDisp);
  }
  low = (low + size - 1) / size * size;
  high = high / size * size;
  if (low > high) {
    fail(error, "no common address range for near trampoline");
    return MAP_FAILED;
  }

  struct Candidate {
    std::uintptr_t address, distance;
  };

  std::vector<Candidate> candidates;
  const auto preferred = patch.entry / size * size;
  const auto add_gap = [&](Wide begin, Wide end) {
    Wide first = std::max(low, (begin + size - 1) / size * size);
    Wide last = std::min(high, (end - size) / size * size);
    if (end < size || first > last) {
      return;
    }
    const auto address =
        static_cast<std::uintptr_t>(std::clamp(Wide(preferred), first, last));
    const auto distance =
        address > preferred ? address - preferred : preferred - address;
    candidates.push_back({address, distance});
  };
  std::ifstream maps("/proc/self/maps");
  if (!maps) {
    fail(error, "cannot read mappings for near trampoline allocation");
    return MAP_FAILED;
  }
  std::string line;
  Wide previous = 0;
  while (std::getline(maps, line)) {
    unsigned long long begin = 0, end = 0;
    if (std::sscanf(line.c_str(), "%llx-%llx", &begin, &end) != 2 ||
        Wide(begin) < previous || end < begin) {
      fail(error, "invalid mappings for near trampoline allocation");
      return MAP_FAILED;
    }
    add_gap(previous, begin);
    previous = end;
  }
  if (!maps.eof()) {
    fail(error, "incomplete mappings for near trampoline allocation");
    return MAP_FAILED;
  }
  add_gap(previous, Wide(std::numeric_limits<std::uintptr_t>::max()));
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate &a, const Candidate &b) {
              return a.distance < b.distance;
            });
  for (const auto &candidate : candidates) {
    // A hint never replaces an existing mapping. Races and kernels ignoring
    // hints are harmless: check the actual returned address before writing.
    void *memory =
        ::mmap(reinterpret_cast<void *>(candidate.address), size,
               PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
      continue;
    }
    const Wide address = reinterpret_cast<std::uintptr_t>(memory);
    if (address >= low && address <= high) {
      return memory;
    }
    ::munmap(memory, size);
  }
  fail(error, "could not allocate a trampoline in the required address range");
  return MAP_FAILED;
}
} // namespace

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
    return fail(error, init_error.c_str());
  }
  return true;
}

bool decode(std::span<const std::uint8_t> bytes, std::uintptr_t entry,
            Patch &patch, std::string &error, std::size_t jump_size) {
  bytes = bytes.first(std::min(bytes.size(), kMaxDecodedBytes));
  if (!valid_jump_size(jump_size) || bytes.size() < jump_size ||
      Wide(entry) + bytes.size() > std::numeric_limits<std::uintptr_t>::max()) {
    return fail(error, "target entry has insufficient mapped bytes");
  }
  if (bytes[0] == 0xf3 && bytes[1] == 0x0f && bytes[2] == 0x1e &&
      bytes[3] == 0xfa) {
    return fail(error, "target entry has an ENDBR64 CET landing instruction");
  }
  Decoder decoder;
  if (!make_decoder(decoder, error)) {
    return false;
  }
  Patch pending;
  pending.entry = entry;
  pending.jump_size = jump_size;
  while (pending.size < jump_size) {
    llvm::MCInst inst;
    std::uint64_t size = 0;
    const auto status = decoder.dis->getInstruction(
        inst, size,
        llvm::ArrayRef<std::uint8_t>(bytes.data() + pending.size,
                                     bytes.size() - pending.size),
        entry + pending.size, llvm::nulls());
    if (status != llvm::MCDisassembler::Success || !size ||
        size > bytes.size() - pending.size ||
        !allowed_instruction(inst, *decoder.ins)) {
      return fail(
          error,
          "target entry contains an unsupported or PC-relative instruction");
    }
    if (!record_relocation(decoder, inst, bytes, pending.size, size, pending,
                           error)) {
      return false;
    }
    pending.size += size;
  }
  for (const auto &r : pending.relocations) {
    // Check the entire access, including a load/store starting before entry.
    if (r.access_size && Wide(r.target) + r.access_size > Wide(entry) &&
        Wide(r.target) < Wide(entry) + pending.size) {
      return fail(error,
                  "RIP-relative operand overlaps the target patch window");
    }
  }
  std::copy_n(bytes.begin(), pending.size, pending.original.begin());
  patch = std::move(pending);
  return true;
}

bool relocate(const Patch &patch, std::uintptr_t destination,
              std::span<std::uint8_t> output, std::string &error) {
  if (!valid_jump_size(patch.jump_size) || patch.size < patch.jump_size ||
      patch.size > kMaxDecodedBytes ||
      output.size() < patch.size ||
      Wide(destination) + patch.size >
          std::numeric_limits<std::uintptr_t>::max()) {
    return fail(error, "invalid trampoline relocation bounds");
  }
  auto bytes = patch.original;
  for (const auto &r : patch.relocations) {
    if (r.instruction_end > patch.size ||
        r.displacement_offset > r.instruction_end ||
        r.instruction_end - r.displacement_offset < sizeof(std::int32_t)) {
      return fail(error, "invalid trampoline displacement bounds");
    }
    const Wide displacement = Wide(r.target) - destination - r.instruction_end;
    if (displacement < kMinDisp || displacement > kMaxDisp) {
      return fail(error,
                  "RIP-relative trampoline displacement is out of range");
    }
    const auto encoded = static_cast<std::int32_t>(displacement);
    std::memcpy(bytes.data() + r.displacement_offset, &encoded,
                sizeof(encoded));
  }
  std::copy_n(bytes.begin(), patch.size, output.begin());
  return true;
}

bool create_trampoline(const Patch &patch, void *&address,
                       std::size_t &mapping_size, std::string &error) {
  const long page = ::sysconf(_SC_PAGESIZE);
  if (page < static_cast<long>(kRelayOffset + kJumpBytes) ||
      !valid_jump_size(patch.jump_size) || patch.size < patch.jump_size ||
      patch.size > kMaxDecodedBytes ||
      Wide(patch.entry) + patch.size >
          std::numeric_limits<std::uintptr_t>::max()) {
    return fail(error, "invalid trampoline allocation bounds");
  }
  const auto size = static_cast<std::size_t>(page);
  void *memory = allocate(patch, size, error);
  if (memory == MAP_FAILED) {
    if (patch.relocations.empty() && patch.jump_size == kJumpBytes) {
      fail(error, "could not allocate trampoline");
    }
    return false;
  }
  auto *bytes = static_cast<std::uint8_t *>(memory);
  if (!relocate(patch, reinterpret_cast<std::uintptr_t>(memory),
                {bytes, patch.size}, error)) {
    ::munmap(memory, size);
    return false;
  }
  // Register-neutral absolute indirect jump back after the copied prologue.
  write_absolute_jump(bytes + patch.size, patch.entry + patch.size);
  if (::mprotect(memory, size, PROT_READ | PROT_EXEC) != 0) {
    ::munmap(memory, size);
    return fail(error, "could not make trampoline executable");
  }
  address = memory;
  mapping_size = size;
  return true;
}
bool prepare_entry_jump(const Patch &patch, void *trampoline,
                        std::size_t mapping_size, const void *destination,
                        std::span<std::uint8_t> output, std::string &error) {
  if (!valid_jump_size(patch.jump_size) || patch.size < patch.jump_size ||
      output.size() < patch.size)
    return fail(error, "invalid entry jump bounds");
  const auto target = reinterpret_cast<std::uintptr_t>(destination);
  if (patch.jump_size == kRelativeJumpBytes) {
    if (!trampoline || trampoline == MAP_FAILED ||
        mapping_size < kRelayOffset + kJumpBytes)
      return fail(error, "invalid trampoline relay bounds");
    auto *relay = static_cast<std::uint8_t *>(trampoline) + kRelayOffset;
    const Wide displacement = Wide(reinterpret_cast<std::uintptr_t>(relay)) -
                              patch.entry - kRelativeJumpBytes;
    if (displacement < kMinDisp || displacement > kMaxDisp)
      return fail(error, "entry relay displacement is out of range");
    // The new trampoline is private until the server installs the entry jump.
    if (::mprotect(trampoline, mapping_size, PROT_READ | PROT_WRITE) != 0)
      return fail(error, "could not make the entry relay writable");
    write_absolute_jump(relay, target);
    if (::mprotect(trampoline, mapping_size, PROT_READ | PROT_EXEC) != 0)
      return fail(error, "could not make the entry relay executable");
    const auto encoded = static_cast<std::int32_t>(displacement);
    output[0] = 0xe9;
    std::memcpy(output.data() + 1, &encoded, sizeof(encoded));
  } else {
    write_absolute_jump(output.data(), target);
  }
  std::fill(output.begin() + patch.jump_size, output.begin() + patch.size, 0x90);
  return true;
}

bool decode_getter(std::string_view bytes, std::size_t &trap_offset, std::string &error) {
  trap_offset = bytes.starts_with("\xf3\x0f\x1e\xfa") ? 4 : 0;
  bytes.remove_prefix(trap_offset);
  // MOV r/m32,EAX with RDI as the base, followed by a plain near RET.
  if ((bytes.size() == 3 || bytes.size() == 4 || bytes.size() == 7) &&
      bytes.front() == '\x8b' && bytes.back() == '\xc3') {
    const auto operand = static_cast<unsigned char>(bytes[1]);
    if ((bytes.size() == 3 && operand == 0x07) ||
        (bytes.size() == 4 && operand == 0x47) ||
        (bytes.size() == 7 && operand == 0x87))
      return true;
  }
  return fail(error, "entry fallback requires a complete 32-bit receiver load and return");
}

bool create_getter_copy(std::string_view bytes, std::uintptr_t caller,
                        void *&address, std::size_t &size, std::string &error) {
  const long page = ::sysconf(_SC_PAGESIZE);
  if (page < static_cast<long>(kRelayOffset + kJumpBytes) || bytes.size() > kRelayOffset)
    return fail(error, "invalid getter copy allocation bounds");
  Patch placement;
  placement.entry = caller;
  placement.jump_size = caller ? kRelativeJumpBytes : kJumpBytes;
  void *memory = allocate(placement, static_cast<std::size_t>(page), error);
  if (memory == MAP_FAILED) {
    if (!caller)
      fail(error, "could not allocate executable getter copy");
    return false;
  }
  std::memcpy(memory, bytes.data(), bytes.size());
  if (::mprotect(memory, page, PROT_READ | PROT_EXEC) != 0) {
    ::munmap(memory, page);
    return fail(error, "could not make getter copy executable");
  }
  address = memory;
  size = page;
  return true;
}

bool prepare_call_relay(std::uintptr_t caller, void *copy, std::size_t size,
                        const void *wrapper, std::span<std::uint8_t> output,
                        std::uintptr_t &relay, std::string &error) {
  Patch placement;
  placement.entry = caller;
  placement.jump_size = placement.size = kRelativeJumpBytes;
  if (!prepare_entry_jump(placement, copy, size, wrapper, output, error))
    return false;
  output[0] = 0xe8;
  relay = reinterpret_cast<std::uintptr_t>(copy) + kRelayOffset;
  return true;
}

bool allocate_code_cache(std::uintptr_t caller, std::size_t bytes,
                         void *&address, std::size_t &size, std::string &error,
                         std::span<const RipRelocation> relocations) {
  const auto page = ::sysconf(_SC_PAGESIZE);
  if (page <= 0 || !bytes || bytes > static_cast<std::size_t>(INT32_MAX))
    return fail(error, "invalid code-cache allocation bounds");
  const auto aligned = (bytes + page - 1) / page * page;
  Patch placement;
  placement.entry = caller;
  placement.jump_size = kRelativeJumpBytes;
  placement.relocations.assign(relocations.begin(), relocations.end());
  auto *memory = allocate(placement, aligned, error);
  if (memory == MAP_FAILED)
    return false;
  address = memory;
  size = aligned;
  return true;
}

bool code_cache_rip_relocation(std::span<const std::uint8_t> bytes,
                               std::uintptr_t pc, RipRelocation &relocation,
                               std::string &error) {
  Decoder decoder;
  if (!make_decoder(decoder, error))
    return false;
  llvm::MCInst inst;
  std::uint64_t size = 0;
  Patch patch;
  patch.entry = pc;
  if (decoder.dis->getInstruction(inst, size, {bytes.data(), bytes.size()}, pc,
                                  llvm::nulls()) != llvm::MCDisassembler::Success ||
      size != bytes.size() || !allowed_instruction(inst, *decoder.ins))
    return fail(error, "unsupported RIP-relative code-cache instruction");
  if (!record_relocation(decoder, inst, bytes, 0, size, patch, error))
    return false;
  if (patch.relocations.size() != 1)
    return fail(error, "expected one verified RIP-relative operand");
  relocation = patch.relocations.front();
  return true;
}
} // namespace tracer::native_detour

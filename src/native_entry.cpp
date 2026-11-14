#include "native_entry.hpp"
#include "native_detour.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>
#include <set>

#include <llvm/ADT/ArrayRef.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCDisassembler/MCDisassembler.h>
#include <llvm/MC/MCInst.h>
#include <llvm/MC/MCInstPrinter.h>
#include <llvm/MC/MCInstrAnalysis.h>
#include <llvm/MC/MCInstrInfo.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/MC/MCTargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>

#include "tracer/compat.hpp"
#include "tracer/protocol.hpp"

namespace tracer::native_entry {
namespace {
constexpr std::size_t jump_bytes = 14;

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

constexpr auto max_decoded_bytes = kMaxPatchBytes - jump_bytes;

struct Decoder {
  std::unique_ptr<llvm::MCRegisterInfo> regs;
  std::unique_ptr<llvm::MCAsmInfo> asm_info;
  std::unique_ptr<llvm::MCSubtargetInfo> sub;
  std::unique_ptr<llvm::MCInstrInfo> ins;
  std::unique_ptr<llvm::MCContext> context;
  std::unique_ptr<llvm::MCDisassembler> dis;
  std::unique_ptr<llvm::MCInstPrinter> printer;
  std::unique_ptr<llvm::MCInstrAnalysis> analysis;
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
#if defined(__aarch64__)
  d.sub.reset(target->createMCSubtargetInfo(triple, "generic", "+lse,+pauth,+bti,+sve,+sve2,+sme,+sme2"));
#else
  d.sub.reset(target->createMCSubtargetInfo(triple, "generic", ""));
#endif
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
  d.printer.reset(target->createMCInstPrinter(parsed, d.asm_info->getAssemblerDialect(),
                                             *d.asm_info, *d.ins, *d.regs));
  d.analysis.reset(target->createMCInstrAnalysis(d.ins.get()));
  if (!d.printer || !d.analysis) {
    error = "LLVM MC instruction formatting or analysis unavailable";
    return false;
  }
  d.printer->setPrintBranchImmAsAddress(true);
  return true;
}

bool entry_instruction(const llvm::MCInst &inst, const llvm::MCInstrInfo &ins,
                      const llvm::MCRegisterInfo &regs, bool allow_rip = false,
                      bool allow_extended = true) {
  const auto &desc = ins.get(inst.getOpcode());
  if (desc.isBranch() || desc.isCall() || desc.isReturn() ||
      desc.isIndirectBranch()) {
    return false;
  }
  const std::string name = ins.getName(inst.getOpcode()).str();
  static constexpr std::string_view allowed[] = {
      "PUSH64r",   "POP64r",    "MOV64rr",    "MOV32rr",   "MOV16rr",
      "MOV8rr",    "MOV32ri",   "MOV64rm",   "MOV32rm",   "MOV16rm",
      "MOV8rm",    "MOV64mr",   "MOV32mr",    "MOV16mr",   "MOV8mr",
      "MOVSS",     "MOVSD",     "MOVAPS",     "MOVUPS",    "LEA64r",
      "LEA32r",    "SUB64ri8",  "SUB64ri32",  "ADD64ri8",  "ADD64ri32",
      "ADD32ri8",  "ADD32ri",   "ADD32rr",    "IMUL32rri8", "IMUL32rri", "IMUL32rmi8",
      "IMUL32rmi",
      "XOR64rr",   "XOR32rr",    "TEST64rr",  "TEST32rr",
      "CMP64rr",   "CMP32rr",   "AND8i8",     "AND8ri",    "AND64ri8",
      "AND64ri32", "XOR8i8",    "XOR8ri",     "NOP",
      "NOOP",      "MOV64mi32", "MOV32mi",    "MOV16mi",   "MOV8mi"};
  static constexpr std::string_view extended[] = {
      "MOV64ri", "LEA64_32r", "IMUL32rm", "IMUL32rr", "MOVSX64rm32", "MOVSX64rr32", "PXORrr"};
  const auto matches = [&](std::string_view prefix) {
    return ::tracer::compat::starts_with(name, prefix);
  };
  if (name != "CMP64mi8" &&
      !std::any_of(std::begin(allowed), std::end(allowed), matches) &&
      (!allow_extended || !std::any_of(std::begin(extended), std::end(extended), matches)))
    return false;
  for (unsigned i = 0; i < inst.getNumOperands(); ++i) {
    if (inst.getOperand(i).isReg()) {
      auto r = inst.getOperand(i).getReg();
      const char *n = r ? regs.getName(r) : nullptr;
      if (n && ((!allow_rip && !std::strcmp(n, "RIP")) || !std::strcmp(n, "EIP"))) {
        return false;
      }
    }
  }
  return true;
}

#if defined(__x86_64__)
bool integer_division(llvm::StringRef opcode) {
  if (opcode.consume_front("IDIV") || opcode.consume_front("DIV")) {
    if (!opcode.consume_back("r") && !opcode.consume_back("m"))
      return false;
    return opcode == "8" || opcode == "16" || opcode == "32" || opcode == "64";
  }
  return false;
}

bool copyable_instruction(const llvm::MCInst &inst, const Decoder &decoder,
                          bool changes_stack, bool locked, bool verified_store,
                          bool allow_rip) {
  const auto &desc = decoder.ins->get(inst.getOpcode());
  const auto opcode = decoder.ins->getName(inst.getOpcode());
  if (desc.isPseudo() || desc.isMetaInstruction() || desc.isTrap() ||
      desc.isTerminator() || desc.mayAffectControlFlow(inst, *decoder.regs) ||
      (locked && !verified_store))
    return false;
  if (opcode == "ENDBR64")
    return !locked;
  // LLVM marks integer division as having side effects because it can trap.
  // The cache restores the original PC for a synchronous divide fault.
  if ((desc.hasUnmodeledSideEffects() && !integer_division(opcode)) ||
      opcode == "STD" || ::tracer::compat::starts_with(opcode, "POPF"))
    return false;
  if (changes_stack && opcode != "LEAVE64" &&
      !entry_instruction(inst, *decoder.ins, *decoder.regs))
    return false;
  const auto restricted_register = [&](llvm::MCRegister reg) {
    if (!reg)
      return false;
    const llvm::StringRef name = decoder.regs->getName(reg);
    // x87 records the current instruction address. Segment and system
    // registers can affect control flow or the meaning of later operands.
    return (!allow_rip && name == "RIP") || name == "EIP" || name == "IP" ||
           name == "CS" || name == "DS" || name == "ES" || name == "FS" ||
           name == "GS" || name == "SS" || ::tracer::compat::starts_with(name, "ST") ||
           ::tracer::compat::starts_with(name, "FP") || ::tracer::compat::starts_with(name, "MM") ||
           ::tracer::compat::starts_with(name, "CR") || ::tracer::compat::starts_with(name, "DR");
  };
  for (const auto &operand : inst)
    if (operand.isReg() && restricted_register(operand.getReg()))
      return false;
  return std::none_of(desc.implicit_defs().begin(), desc.implicit_defs().end(), restricted_register) &&
         std::none_of(desc.implicit_uses().begin(), desc.implicit_uses().end(), restricted_register);
}
#endif

bool integer_arithmetic(llvm::StringRef opcode, llvm::StringRef suffix) {
  for (const llvm::StringRef op : {"ADD32", "SUB32", "AND32", "OR32", "XOR32", "INC32", "DEC32",
                                  "NEG32", "NOT32", "SHL32", "SHR32", "SAR32"})
    if (::tracer::compat::starts_with(opcode, op) && opcode.drop_front(op.size()) == suffix)
      return true;
  return false;
}

bool decode(const Decoder &decoder, llvm::MCInst &instruction,
            std::uint64_t &size, std::uint64_t address, std::string_view bytes) {
  return decoder.dis->getInstruction(
             instruction, size,
             {reinterpret_cast<const std::uint8_t *>(bytes.data()), bytes.size()},
             address, llvm::nulls()) == llvm::MCDisassembler::Success &&
         size && size <= bytes.size();
}

std::size_t patch_window(const Decoder &decoder, std::uint64_t address,
                         std::string_view bytes, std::string &error) {
  if (bytes.size() < jump_bytes) {
    error = "target entry has insufficient mapped or function bytes";
    return 0;
  }
  if (tracer::compat::starts_with(bytes, "\xf3\x0f\x1e\xfa")) {
    error = "target entry has an ENDBR64 CET landing instruction";
    return 0;
  }
  bytes = bytes.substr(0, max_decoded_bytes);
  std::size_t used = 0;
  while (used < jump_bytes) {
    llvm::MCInst instruction;
    std::uint64_t size = 0;
    if (!decode(decoder, instruction, size, address + used, bytes.substr(used)) ||
        !entry_instruction(instruction, *decoder.ins, *decoder.regs)) {
      error = "target entry contains an unsupported or PC-relative instruction";
      return 0;
    }
    used += size;
  }
  return used;
}
} // namespace

namespace {
std::optional<ScalarStore> store32(const Decoder &decoder, const llvm::MCInst &inst,
                                   std::string_view bytes) {
  using Kind = ScalarStore::Kind;
  const auto opcode = decoder.ins->getName(inst.getOpcode());
#if defined(__aarch64__)
  const auto reg = [&](unsigned operand) -> std::string {
    if (operand >= inst.getNumOperands() || !inst.getOperand(operand).isReg())
      return {};
    return decoder.regs->getName(inst.getOperand(operand).getReg());
  };
  if ((opcode == "STRWui" || opcode == "STURWi") &&
      inst.getNumOperands() == 3 && !reg(0).empty() && !reg(1).empty() &&
      inst.getOperand(2).isImm())
    return ScalarStore{reg(1), {}, 1,
                       inst.getOperand(2).getImm() * (opcode == "STRWui" ? 4 : 1), Kind::move,
                       reg(0), std::nullopt, {}};
  if ((opcode == "STRWpre" || opcode == "STRWpost") && bytes.size() == 4 &&
      inst.getNumOperands() == 4 && !reg(0).empty() && reg(0) == reg(2) &&
      !reg(1).empty() && inst.getOperand(3).isImm()) {
    const auto first = static_cast<unsigned char>(bytes[0]);
    const auto base = (first >> 5) | ((static_cast<unsigned char>(bytes[1]) & 3) << 3);
    if (base == (first & 31) && base != 31)
      return std::nullopt; // Store/writeback overlap is constrained unpredictable.
    return ScalarStore{reg(2), {}, 1,
        opcode == "STRWpre" ? inst.getOperand(3).getImm() : 0,
        Kind::move, reg(1), std::nullopt, {}};
  }
  if (opcode == "STRWroX" && inst.getNumOperands() == 5 &&
      !reg(0).empty() && !reg(1).empty() && !reg(2).empty() &&
      inst.getOperand(3).isImm() && inst.getOperand(4).isImm() &&
      (inst.getOperand(4).getImm() == 0 || inst.getOperand(4).getImm() == 1))
    return ScalarStore{reg(1), reg(2), inst.getOperand(4).getImm() ? 4U : 1U,
                       0, Kind::move, reg(0), std::nullopt, {}};
  if (opcode == "STLRW" && inst.getNumOperands() == 2 && !reg(0).empty() && !reg(1).empty())
    return ScalarStore{reg(1), {}, 1, 0, Kind::move, reg(0), std::nullopt, {}};
  const auto usable_result = !reg(0).empty() && reg(0) != "WZR";
  if ((opcode == "LDADDW" || opcode == "LDADDAW" || opcode == "LDADDLW" || opcode == "LDADDALW") &&
      inst.getNumOperands() == 3 && usable_result && !reg(1).empty() && !reg(2).empty())
    return ScalarStore{reg(2), {}, 1, 0, Kind::exchange_add, reg(1), std::nullopt, reg(0)};
  if ((opcode == "CASW" || opcode == "CASAW" || opcode == "CASLW" || opcode == "CASALW") &&
      inst.getNumOperands() == 4 && usable_result && reg(0) == reg(1) &&
      !reg(2).empty() && !reg(3).empty())
    return ScalarStore{reg(3), {}, 1, 0, Kind::compare_exchange, reg(2), std::nullopt, reg(0)};
  return std::nullopt;
#else
  const bool locked = !bytes.empty() && bytes.front() == '\xf0';
  if (locked)
    bytes.remove_prefix(1);
  if (!bytes.empty() && (static_cast<unsigned char>(bytes.front()) & 0xf0) == 0x40)
    bytes.remove_prefix(1);
  Kind kind = Kind::move;
  unsigned memory = 0;
  unsigned value = 5;
  if (locked && ::tracer::compat::starts_with(bytes, "\x0f\xc1") && opcode == "XADD32rm" &&
      inst.getNumOperands() == 7 && inst.getOperand(0).isReg() &&
      inst.getOperand(1).isReg() &&
      inst.getOperand(0).getReg() == inst.getOperand(1).getReg()) {
    kind = Kind::exchange_add;
    memory = 2;
    value = 0;
  } else if (locked && ::tracer::compat::starts_with(bytes, "\x0f\xb1") && opcode == "CMPXCHG32rm" &&
             inst.getNumOperands() == 6) {
    kind = Kind::compare_exchange;
  } else if ((inst.getNumOperands() == 5 && (integer_arithmetic(opcode, "m") ||
                (!locked && (integer_arithmetic(opcode, "m1") || integer_arithmetic(opcode, "mCL"))))) ||
             (inst.getNumOperands() == 6 && (integer_arithmetic(opcode, "mr") ||
                integer_arithmetic(opcode, "mi") || integer_arithmetic(opcode, "mi8")))) {
    kind = Kind::rmw;
  } else if (locked || bytes.empty() || inst.getNumOperands() != 6 ||
             !((bytes.front() == '\x89' && opcode == "MOV32mr") ||
               (bytes.front() == '\xc7' && opcode == "MOV32mi"))) {
    return std::nullopt;
  }
  const auto &base = inst.getOperand(memory);
  const auto &scale = inst.getOperand(memory + 1);
  const auto &index = inst.getOperand(memory + 2);
  const auto &disp = inst.getOperand(memory + 3);
  const auto &segment = inst.getOperand(memory + 4);
  if (!base.isReg() || !scale.isImm() || !index.isReg() || !disp.isImm() ||
      !segment.isReg() || segment.getReg() ||
      (scale.getImm() != 1 && scale.getImm() != 2 &&
       scale.getImm() != 4 && scale.getImm() != 8))
    return std::nullopt;
  auto register_name = [&](const auto &operand) -> std::string {
    return operand.getReg() ? decoder.regs->getName(operand.getReg()) : "";
  };
  ScalarStore store{register_name(base), register_name(index),
                    static_cast<unsigned>(scale.getImm()), disp.getImm(),
                    kind, {}, std::nullopt, {}};
  if (kind == Kind::rmw) {
    store.locked = locked;
    return store;
  }
  const auto &input = inst.getOperand(value);
  if (input.isReg() && input.getReg())
    store.value_register = register_name(input);
  else if (kind == Kind::move && input.isImm())
    store.immediate = static_cast<std::uint32_t>(input.getImm());
  else
    return std::nullopt;
  return store;
#endif
}

Preview inspect(std::uint64_t address, std::string_view bytes, bool entry,
                ::tracer::compat::span<const std::uint64_t> additional_entries = {}) {
  Preview result;
  Decoder decoder;
  if (!make_decoder(decoder, result.diagnostic))
    return result;
  if (entry) {
    bytes = bytes.substr(0, preview_bytes);
    result.patch_size = patch_window(decoder, address, bytes, result.diagnostic);
  }
  // Explore direct branches within the verified byte window. Calls retain
  // their return-site edge and their target is reported without following it.
  std::set<std::uint64_t> pending{address};
  for (const auto pc : additional_entries)
    if (pc >= address && pc - address < bytes.size())
      pending.insert(pc);
  std::set<std::uint64_t> visited;
  while (!pending.empty()) {
    const auto pc = *pending.begin();
    pending.erase(pending.begin());
    if (!visited.insert(pc).second)
      continue;
    const auto offset = pc - address;
    llvm::MCInst instruction;
    std::uint64_t size = 0;
    if (!decode(decoder, instruction, size, pc, bytes.substr(offset))) {
      result.decode_failed = true;
      continue;
    }
    const bool lock = decoder.ins->getName(instruction.getOpcode()) == "LOCK_PREFIX";
    if (lock) {
      // LLVM MC emits LOCK as a separate pseudo-instruction. The hardware
      // executes it with the following opcode, at the prefix's original PC.
      std::uint64_t body_size = 0;
      if (size != 1 || !decode(decoder, instruction, body_size, pc + size,
                               bytes.substr(offset + size)) ||
          decoder.ins->getName(instruction.getOpcode()) == "LOCK_PREFIX") {
        result.decode_failed = true;
        continue;
      }
      size += body_size;
    }

    const auto &analysis = *decoder.analysis;
    const bool is_call = analysis.isCall(instruction);
    const bool is_branch = analysis.isBranch(instruction);
    const bool returns = analysis.isReturn(instruction);
    const bool falls_through = !returns &&
        (is_call || analysis.isConditionalBranch(instruction) ||
         (!is_branch && !analysis.isTerminator(instruction)));
    std::optional<std::uint64_t> target;
    std::uint64_t destination = 0;
    if ((is_call || is_branch) &&
        analysis.evaluateBranch(instruction, pc, size, destination))
      target = destination;
    if (is_branch && !is_call && target &&
        *target >= address && *target - address < bytes.size())
      pending.insert(*target);
    if (falls_through && size < bytes.size() - offset)
      pending.insert(pc + size);

    std::string text;
    llvm::raw_string_ostream assembly(text);
    decoder.printer->printInst(&instruction, pc + size, "", *decoder.sub, assembly);
    const auto printed = llvm::StringRef(assembly.str()).trim();
    const auto text_prefix = lock ? "lock " : "";
    const auto &desc = decoder.ins->get(instruction.getOpcode());
    const auto opcode = decoder.ins->getName(instruction.getOpcode());
    bool changes_receiver = false, changes_stack = false;
    const auto check_definition = [&](unsigned reg) {
      const std::string_view name = decoder.regs->getName(reg);
#if defined(__aarch64__)
      changes_receiver |= name == "X0" || name == "W0";
      changes_stack |= name == "SP" || name == "WSP";
#else
      changes_receiver |= name == "RDI" || name == "EDI" || name == "DI" || name == "DIL";
      changes_stack |= name == "RSP" || name == "ESP" || name == "SP" || name == "SPL";
#endif
    };
    for (unsigned i = 0; i < std::min(desc.getNumDefs(), instruction.getNumOperands()); ++i)
      if (instruction.getOperand(i).isReg() && instruction.getOperand(i).getReg())
        check_definition(instruction.getOperand(i).getReg());
    for (auto reg : desc.implicit_defs())
      check_definition(reg);
    auto store = store32(decoder, instruction, bytes.substr(offset, size));
#if defined(__aarch64__)
    const bool frame_store = false;
    const bool architectural_hint = printed == "nop" || ::tracer::compat::starts_with(printed, "bti") ||
        printed == "paciasp" || printed == "pacibsp" || printed == "autiasp" || printed == "autibsp";
    const bool copyable = store.has_value() || opcode == "RET" || opcode == "RETAA" || opcode == "RETAB" ||
        opcode == "STPXpre" || opcode == "STPXi" ||
        opcode == "STRXpre" || opcode == "STRXui" || opcode == "STRWui" || opcode == "STURWi" ||
        opcode == "LDRXui" || opcode == "LDURXi" || opcode == "LDRWui" || opcode == "LDURWi" ||
        opcode == "LDPXi" || opcode == "STURXi" || opcode == "STPWpre" || opcode == "STPWi" ||
        opcode == "ADDXri" || opcode == "ADDWri" || opcode == "SUBXri" ||
        opcode == "ORRXrs" || opcode == "ORRWrs" ||
        opcode == "MOVZWi" || opcode == "MOVZXi" || opcode == "MOVNWi" || opcode == "MOVNXi" ||
        architectural_hint;
    const bool entry_copyable = (opcode == "ADR" || opcode == "ADRP") &&
        !changes_receiver && !changes_stack;
    const bool may_store = desc.mayStore() && !architectural_hint;
    const bool rip_relative = false;
#else
    const bool has_rip = std::any_of(instruction.begin(), instruction.end(), [&](const auto &operand) {
      return operand.isReg() && operand.getReg() &&
             llvm::StringRef(decoder.regs->getName(operand.getReg())) == "RIP";
    });
    const bool copyable = !has_rip &&
        copyable_instruction(instruction, decoder, changes_stack, lock, store.has_value(), false);
    const bool rip_relative = has_rip && !lock &&
        (entry_instruction(instruction, *decoder.ins, *decoder.regs, true) ||
         native_detour::scalar_fp_load_width(opcode)) &&
        copyable_instruction(instruction, decoder, changes_stack, false, false, true);
    const bool entry_copyable = copyable && !desc.mayLoad() && !desc.mayStore() &&
        !desc.mayRaiseFPException() && !integer_division(opcode) &&
        opcode != "ENDBR64" && opcode != "LEAVE64";
    const bool may_store = desc.mayStore();
    const auto register_name = [&](unsigned i) -> std::string_view {
      const auto &operand = instruction.getOperand(i);
      return operand.isReg() && operand.getReg() ? decoder.regs->getName(operand.getReg()) : "";
    };
    const bool frame_store = (opcode == "MOV64mr" || opcode == "MOV32mr") &&
        instruction.getNumOperands() == 6 && register_name(0) == "RBP" &&
        register_name(2).empty() && register_name(4).empty() &&
        instruction.getOperand(3).isImm() && instruction.getOperand(3).getImm() <=
            (opcode == "MOV64mr" ? -8 : -4);
#endif
    result.instructions.push_back({pc, static_cast<std::size_t>(size),
                                   text_prefix + printed.str(),
                                   is_call ? Flow::call : is_branch ? Flow::branch
                                             : returns ? Flow::returns : Flow::ordinary,
                                   target, falls_through,
                                   may_store, std::move(store), copyable,
                                   changes_receiver, changes_stack,
                                   entry_copyable,
                                   opcode == "CALL64r" || opcode == "CALL64m",
                                   rip_relative, frame_store});
  }
  std::sort(result.instructions.begin(), result.instructions.end(),
            [](const auto &a, const auto &b) { return a.address < b.address; });
  return result;
}
} // namespace

Preview preview(std::uint64_t address, std::string_view bytes) {
  return inspect(address, bytes, true);
}

Preview scan(std::uint64_t address, std::string_view bytes,
             ::tracer::compat::span<const std::uint64_t> additional_entries) {
  return inspect(address, bytes, false, additional_entries);
}

#if defined(__x86_64__)
bool compatible_patch(std::uint64_t address, std::string_view before, std::string_view after) {
  std::string error;
  Decoder decoder;
  if (!make_decoder(decoder, error))
    throw std::runtime_error(error);
  if (before.empty() || before.size() != after.size())
    return false;
  for (std::size_t offset = 0; offset < before.size();) {
    const auto width = [&](std::string_view bytes) -> std::uint64_t {
      llvm::MCInst instruction;
      std::uint64_t size = 0;
      if (!decode(decoder, instruction, size, address + offset, bytes.substr(offset)) ||
          !entry_instruction(instruction, *decoder.ins, *decoder.regs, false, false))
        return 0;
      return size;
    };
    const auto size = width(before);
    if (!size || width(after) != size)
      return false;
    offset += size;
  }
  return true;
}

std::size_t nop_prefix(std::uint64_t address, std::string_view bytes, std::size_t minimum) {
  Decoder decoder;
  std::string error;
  if (!make_decoder(decoder, error))
    return 0;
  std::size_t used = 0;
  while (used < minimum && used < bytes.size()) {
    llvm::MCInst inst;
    std::uint64_t size = 0;
    if (!decode(decoder, inst, size, address + used, bytes.substr(used)) ||
        !decoder.ins->getName(inst.getOpcode()).starts_with("NOOP"))
      return 0;
    used += size;
  }
  return used >= minimum ? used : 0;
}

bool forwards_receiver(std::uint64_t address, std::string_view prefix) {
  Decoder decoder;
  std::string error;
  if (!make_decoder(decoder, error))
    return false;
  // Track the entry receiver through register copies and a conventional
  // RBP-relative spill/reload before the first call. Reject other writes.
  std::set<std::string_view> receiver{"RDI"};
  std::set<std::int64_t> spills;
  bool pushed_rbp = false, frame = false;
  std::int64_t stack_size = 0;
  while (!prefix.empty()) {
    llvm::MCInst inst;
    std::uint64_t size = 0;
    if (!decode(decoder, inst, size, address, prefix))
      return false;
    address += size;
    prefix.remove_prefix(size);
    const auto opcode = decoder.ins->getName(inst.getOpcode());
    const auto reg = [&](unsigned i) -> std::string_view {
      return inst.getOperand(i).isReg() ? decoder.regs->getName(inst.getOperand(i).getReg()) : "";
    };
    const auto slot = [&](unsigned i) -> std::optional<std::int64_t> {
      if (reg(i) != "RBP" || inst.getOperand(i + 1).getImm() != 1 ||
          inst.getOperand(i + 2).getReg() || inst.getOperand(i + 4).getReg() ||
          !inst.getOperand(i + 3).isImm())
        return std::nullopt;
      const auto displacement = inst.getOperand(i + 3).getImm();
      if (displacement < -stack_size || displacement > -8 || displacement % 8 != 0)
        return std::nullopt;
      return displacement;
    };
    if (opcode == "ENDBR64" || ::tracer::compat::starts_with(opcode, "NOOP"))
      continue;
    if (opcode == "PUSH64r" && reg(0) == "RBP" && !pushed_rbp) {
      pushed_rbp = true;
    } else if (opcode == "MOV64rr" && reg(0) == "RBP" && reg(1) == "RSP" && pushed_rbp && !frame) {
      frame = true;
    } else if ((opcode == "SUB64ri8" || opcode == "SUB64ri32") && reg(0) == "RSP" &&
               reg(1) == "RSP" && frame && !stack_size && inst.getOperand(2).getImm() > 0) {
      stack_size = inst.getOperand(2).getImm();
    } else if (opcode == "MOV64rr" && reg(0) != "RBP" && reg(0) != "RSP") {
      if (receiver.count(reg(1)))
        receiver.insert(reg(0));
      else
        receiver.erase(reg(0));
    } else if (opcode == "MOV64mr" && frame && slot(0) && receiver.count(reg(5))) {
      spills.insert(*slot(0));
    } else if (opcode == "MOV64rm" && frame && slot(1) && reg(0) != "RBP" && reg(0) != "RSP") {
      if (spills.count(*slot(1)))
        receiver.insert(reg(0));
      else
        receiver.erase(reg(0));
    } else {
      return false;
    }
  }
  return receiver.count("RDI");
}
#endif
} // namespace tracer::native_entry

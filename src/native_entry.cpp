#include "native_entry.hpp"

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
        !safe_instruction(instruction, *decoder.ins, *decoder.regs)) {
      error = "target entry contains an unsupported or PC-relative instruction";
      return 0;
    }
    used += size;
  }
  return used;
}
} // namespace

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
          !safe_instruction(instruction, *decoder.ins, *decoder.regs))
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

Preview preview(std::uint64_t address, std::string_view bytes) {
  Preview result;
  Decoder decoder;
  if (!make_decoder(decoder, result.diagnostic))
    return result;
  bytes = bytes.substr(0, preview_bytes);
  result.patch_size = patch_window(decoder, address, bytes, result.diagnostic);
  // Explore direct branches within the verified byte window. Calls retain
  // their return-site edge and their target is reported without following it.
  std::set<std::uint64_t> pending{address};
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
    result.instructions.push_back({pc, static_cast<std::size_t>(size),
                                   llvm::StringRef(assembly.str()).trim().str(),
                                   is_call ? Flow::call : is_branch ? Flow::branch
                                             : returns ? Flow::returns : Flow::ordinary,
                                   target, falls_through});
  }
  std::sort(result.instructions.begin(), result.instructions.end(),
            [](const auto &a, const auto &b) { return a.address < b.address; });
  return result;
}
} // namespace tracer::native_entry

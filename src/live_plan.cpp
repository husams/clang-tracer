#include "live_plan.hpp"

#include "debug_resolver.hpp"
#include "live_compile.hpp"
#include "native_entry.hpp"
#include "tracer/protocol.hpp"

#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <google/protobuf/text_format.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/Object/BuildID.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/SHA256.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <deque>
#include <fstream>
#include <future>
#include <iterator>
#include <optional>
#include <set>
#include <stdexcept>
#include <thread>

namespace tracer::live {
namespace {
namespace api = clangtracer::v1;
namespace fs = std::filesystem;
using Elf = llvm::object::ELF64LEObjectFile;

std::string read_file(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input)
    throw std::runtime_error("cannot read build or source input: " + path.string());
  return {std::istreambuf_iterator<char>(input), {}};
}

std::string digest(llvm::StringRef bytes) {
  return llvm::toHex(llvm::SHA256::hash(llvm::arrayRefFromStringRef(bytes)), true);
}

std::string site(const Definition &definition) {
  return definition.path + ':' + std::to_string(definition.line);
}

std::string signature(const std::string &symbol, const Definition &definition) {
  return definition.type.empty() ? llvm::demangle(symbol) : definition.type;
}

struct Symbol {
  std::string name;
  std::uint64_t address;
  std::uint64_t size;
  unsigned type;
  bool local;
  std::string file;
  auto operator<=>(const Symbol &) const = default;
};

struct Image {
  std::string path;
  llvm::object::OwningBinary<llvm::object::Binary> binary;
  const Elf &elf;
  std::vector<Symbol> symbols;
  std::map<std::string, unsigned> file_markers;

  static llvm::object::OwningBinary<llvm::object::Binary> open(const std::string &path) {
    auto image = llvm::object::createBinary(path);
    if (!image)
      throw std::runtime_error(llvm::toString(image.takeError()));
    return std::move(*image);
  }

  Image(const std::string &module, const std::string &sha)
      : path(module), binary(open(module)), elf(as_elf(*binary.getBinary())) {
    if ((!sha.empty() && digest(elf.getMemoryBufferRef().getBuffer()) != sha) ||
        llvm::object::getBuildID(&elf).empty())
      throw std::runtime_error("build manifest does not identify the ELF build");
    const auto append = [&](const auto &symbol, const std::string &file) {
      const llvm::object::ELFSymbolRef native(symbol);
      const auto type = native.getELFType();
      if (type != llvm::ELF::STT_FUNC && type != llvm::ELF::STT_OBJECT &&
          type != llvm::ELF::STT_TLS && type != llvm::ELF::STT_GNU_IFUNC)
        return;
      auto flags = symbol.getFlags();
      auto address = symbol.getAddress();
      auto name = symbol.getName();
      if (!flags || !address || !name) {
        if (!flags) llvm::consumeError(flags.takeError());
        if (!address) llvm::consumeError(address.takeError());
        if (!name) llvm::consumeError(name.takeError());
        throw std::runtime_error("cannot read complete ELF symbol inventory");
      }
      const auto local = native.getBinding() == llvm::ELF::STB_LOCAL;
      if (*address && !name->empty() && !(*flags & llvm::object::SymbolRef::SF_Undefined))
        symbols.push_back({name->str(), *address, native.getSize(), type, local,
                           local ? file : std::string{}});
    };
    std::string file;
    for (const auto &item : elf.symbols()) {
      if (llvm::object::ELFSymbolRef(item).getELFType() == llvm::ELF::STT_FILE) {
        auto name = item.getName();
        if (!name)
          throw std::runtime_error(llvm::toString(name.takeError()));
        file = fs::path(name->str()).filename().string();
        ++file_markers[file];
      } else {
        append(item, file);
      }
    }
    for (const auto &item : elf.getDynamicSymbolIterators())
      append(item, {});
    std::sort(symbols.begin(), symbols.end());
    symbols.erase(std::unique(symbols.begin(), symbols.end()), symbols.end());
  }

  static const Elf &as_elf(const llvm::object::Binary &binary) {
    const auto *object = llvm::dyn_cast<Elf>(&binary);
    if (!object || object->getArch() != llvm::Triple::x86_64)
      throw std::runtime_error("build manifest requires a Linux x86-64 ELF");
    return *object;
  }

  const Symbol *find(const std::string &name, std::optional<bool> local = std::nullopt) const {
    const auto item = std::lower_bound(symbols.begin(), symbols.end(), name,
        [](const Symbol &symbol, const std::string &value) { return symbol.name < value; });
    const Symbol *match = nullptr;
    for (auto it = item; it != symbols.end() && it->name == name; ++it) {
      if (local && it->local != *local)
        continue;
      if (match)
        throw std::runtime_error("ambiguous duplicate ELF symbol: " + name);
      match = &*it;
    }
    return match;
  }

  const Symbol *find_local(const std::string &name, const std::string &unit) const {
    const auto file = fs::path(unit).filename().string();
    const auto marker = file_markers.find(file);
    if (marker == file_markers.end() || marker->second != 1)
      return nullptr;
    const auto first = std::lower_bound(symbols.begin(), symbols.end(), name,
        [](const Symbol &symbol, const std::string &value) { return symbol.name < value; });
    const Symbol *match = nullptr;
    for (auto it = first; it != symbols.end() && it->name == name; ++it) {
      if (!it->local || it->file != file)
        continue;
      if (match)
        return nullptr;
      match = &*it;
    }
    return match;
  }

  std::string_view code(const Symbol &symbol) const {
    auto headers = elf.getELFFile().program_headers();
    if (!headers)
      throw std::runtime_error("cannot read ELF program headers: " + llvm::toString(headers.takeError()));
    for (const auto &segment : *headers) {
      if (segment.p_type != llvm::ELF::PT_LOAD || !(segment.p_flags & llvm::ELF::PF_X) ||
          symbol.address < segment.p_vaddr || symbol.address - segment.p_vaddr >= segment.p_filesz)
        continue;
      const auto offset = symbol.address - segment.p_vaddr;
      if (symbol.size <= segment.p_filesz - offset)
        return elf.getMemoryBufferRef().getBuffer().substr(segment.p_offset + offset, symbol.size);
    }
    throw std::runtime_error("symbol is outside verified executable file bytes: " + symbol.name);
  }
};

bool within(const fs::path &root, const fs::path &path) {
  const auto relative = path.lexically_relative(root);
  return !relative.empty() && *relative.begin() != "..";
}

std::string resolve_path(const std::string &directory, const std::string &file) {
  if (!fs::path(directory).is_absolute())
    throw std::runtime_error("relative compilation directory: " + directory);
  return (fs::path(directory) / file).lexically_normal().string();
}

TranslationUnit profile(const clang::tooling::CompileCommand &command, const std::string &path) {
  const auto adjust = clang::tooling::combineAdjusters(
      clang::tooling::getClangStripOutputAdjuster(),
      clang::tooling::getClangStripDependencyFileAdjuster());
  auto arguments = adjust(command.CommandLine, command.Filename);
  if (arguments.empty())
    throw std::runtime_error("empty compile profile: " + path);
  const auto driver = fs::path(arguments.front()).filename().string();
  if (!driver.starts_with("clang++") && driver != "g++" && !driver.starts_with("g++-") &&
      !driver.ends_with("-linux-gnu-g++") && driver != "c++")
    throw std::runtime_error("unverified C++ compiler profile: " + driver);
  arguments.erase(arguments.begin());
  std::string optimization = "-O0";
  std::erase_if(arguments, [&](const std::string &argument) {
    if (argument.starts_with("-O"))
      optimization = argument;
    if (argument.starts_with("@") || argument.starts_with("-flto") ||
        argument.starts_with("-fplugin") || argument.starts_with("-fsanitize") ||
        argument.starts_with("-mabi") || argument.starts_with("-fabi-version") ||
        argument == "-Xclang" || argument == "-include-pch" || argument == "-ftime-trace" ||
        argument.starts_with("-fprofile"))
      throw std::runtime_error("unverified compile flag: " + argument);
    return argument == "-c" || argument == command.Filename ||
           (!argument.empty() && argument.front() != '-' &&
            resolve_path(command.Directory, argument) == path);
  });
  if (optimization != "-O0")
    throw std::runtime_error("optimized or folded callers require verified coverage and a rebuild");
  arguments.push_back("-fno-inline");
  return {path, command.Directory, std::move(arguments)};
}

struct Sources {
  fs::path root;
  std::vector<SourceFile> baseline;
  std::vector<SourceFile> current;
  std::map<std::string, std::string> saved;
  std::set<std::string> submitted;
};

Sources snapshot_inputs(const api::LiveBuildManifest &manifest,
                        const api::LiveUpdateRequest &request, const Revision &active,
                        const std::set<std::string> *managed_inputs) {
  Sources result;
  result.root = fs::canonical(manifest.source_root());
  const auto snapshot = fs::canonical(manifest.snapshot_root());
  auto add = [&](const std::string &path, std::string baseline) {
    if (result.saved.count(path))
      throw std::runtime_error("duplicate source snapshot input: " + path);
    const auto existing = active.sources.find(path);
    auto current = existing == active.sources.end() ? baseline : existing->second.contents;
    result.baseline.push_back({path, std::move(current)});
    result.saved.emplace(path, std::move(baseline));
  };
  for (const auto &input : manifest.inputs()) {
    const auto relative = fs::path(input.path()).lexically_normal();
    if (relative.empty() || relative.is_absolute() || *relative.begin() == "..")
      throw std::runtime_error("invalid path in build input inventory: " + input.path());
    auto original = read_file(snapshot / relative);
    if (digest(original) != input.sha256())
      throw std::runtime_error("build input checksum differs: " + input.path());
    add((result.root / relative).string(), std::move(original));
  }
  if (result.baseline.empty())
    throw std::runtime_error("build manifest has no original source inputs");
  std::map<std::string, std::string> overrides;
  for (const auto &source : request.sources()) {
    const auto path = fs::canonical(source.path()).string();
    if ((!within(result.root, path) && !(managed_inputs && managed_inputs->contains(path))) ||
        !result.submitted.insert(path).second)
      throw std::runtime_error("invalid or duplicate submitted source: " + path);
    if (read_file(path) != source.contents())
      throw Stale("saved source changed after submission: " + path);
    overrides.emplace(path, source.contents());
  }
  for (const auto &file : result.baseline) {
    auto current = overrides.find(file.path);
    result.current.push_back(current == overrides.end() ? file
                             : SourceFile{file.path, current->second});
    if (current != overrides.end())
      overrides.erase(current);
  }
  for (const auto &[path, source] : active.sources) {
    if (result.saved.count(path))
      continue;
    result.baseline.push_back(source);
    auto current = overrides.find(path);
    result.current.push_back(current == overrides.end() ? source
                             : SourceFile{path, current->second});
    if (current != overrides.end())
      overrides.erase(current);
  }
  for (auto &[path, contents] : overrides)
    result.current.push_back({path, std::move(contents)});
  return result;
}

struct Body {
  std::string name;
  std::string unit;
  Definition definition;
  bool inconsistent = false;
};

void collect(std::map<std::string, Body> &all, const std::string &path,
             const std::map<std::string, Definition> &definitions) {
  for (const auto &[name, definition] : definitions) {
    const auto key = definition.local ? local_import_name(path, name) : name;
    if (!all.contains(key))
      all.emplace(key, Body{name, path, definition});
  }
}

struct Change {
  Body before;
  std::optional<Definition> after;
};

bool same_definition(const Definition &left, const Definition &right) {
  return left.implementation == right.implementation && left.abi == right.abi &&
         left.local == right.local && left.alias_target == right.alias_target;
}

bool equivalent_inline_body(const Definition &before, const Definition &after) {
  // Clang can lower the same inline source with or without an unwind edge.
  // A different call dependency means the instantiation needs independent proof.
  if (!before.source_definition || before.source_definition != after.source_definition ||
      before.path != after.path || !before.discardable || !after.discardable ||
      before.local || after.local || before.abi != after.abi || before.callees != after.callees)
    return false;
  if (before.invokes != after.invokes)
    return !before.normal_flow.empty() && before.normal_flow == after.normal_flow;
  // A previously instantiated system function can also gain nounwind
  // without changing any instruction or exception edge in the body.
  return !before.has_source_abi && !after.has_source_abi && before.nounwind != after.nounwind &&
         before.attributes_without_nounwind == after.attributes_without_nounwind &&
         llvm::StringRef(before.implementation).split('\n').second ==
             llvm::StringRef(after.implementation).split('\n').second;
}

bool equivalent_body(const Definition &before, const Definition &after) {
  return same_definition(before, after) || equivalent_inline_body(before, after);
}

using ClosureScope = std::pair<std::string, std::string>;

ClosureScope closure_scope(std::string_view symbol, const std::map<std::string, Definition> &code) {
  // Clang's local lambda symbols embed the enclosing Itanium function name
  // and an ordinal such as `3$_0`. That ordinal can change within one body.
  const auto marker = symbol.find("$_");
  if (marker == std::string_view::npos)
    return {};
  auto end = marker + 2;
  while (end < symbol.size() && std::isdigit(static_cast<unsigned char>(symbol[end])))
    ++end;
  auto suffix = marker;
  while (suffix && std::isdigit(static_cast<unsigned char>(symbol[suffix - 1])))
    --suffix;
  while (suffix && std::string_view("rVKRO").find(symbol[suffix - 1]) != std::string_view::npos)
    --suffix;
  if (suffix && symbol[suffix - 1] == 'N')
    --suffix;
  if (!suffix || symbol[--suffix] != 'E')
    return {};
  for (auto start = symbol.find('Z', 2); start < suffix; start = symbol.find('Z', start + 1)) {
    auto owner = "_Z" + std::string(symbol.substr(start + 1, suffix - start - 1));
    if (code.contains(owner))
      return {std::move(owner), std::string(symbol.substr(marker, end - marker))};
  }
  return {};
}

bool closure_is_local_to(const llvm::Module &module, const std::string &symbol,
                         const std::string &owner, std::set<std::string> *route = nullptr) {
  const auto *entry = module.getFunction(symbol);
  if (!entry)
    return false;
  llvm::SmallVector<const llvm::Function *> pending{entry};
  llvm::SmallPtrSet<const llvm::Function *, 32> visited;
  // Templates and algorithms can introduce private forwarding functions.
  // Every route into the old operator must be a direct call under its owner.
  while (!pending.empty()) {
    const auto *function = pending.pop_back_val();
    if (function->getName() == owner || !visited.insert(function).second)
      continue;
    if (!function->hasLocalLinkage())
      return false;
    if (route)
      route->insert(function->getName().str());
    for (const auto &use : function->uses()) {
      const auto *call = llvm::dyn_cast<llvm::CallBase>(use.getUser());
      if (!call || &call->getCalledOperandUse() != &use)
        return false;
      pending.push_back(call->getFunction());
    }
  }
  return true;
}

bool closure_is_on_stack(const llvm::Function &function, llvm::StringRef owner) {
  const unsigned receiver = function.hasStructRetAttr() ? 1 : 0;
  if (function.arg_size() <= receiver || !function.getArg(receiver)->getType()->isPointerTy())
    return false;
  llvm::SmallVector<const llvm::Value *> pending{function.getArg(receiver)};
  llvm::SmallPtrSet<const llvm::Value *, 32> visited;
  bool found_stack = false;
  while (!pending.empty()) {
    const auto *value = pending.pop_back_val()->stripPointerCasts();
    if (!visited.insert(value).second)
      continue;
    if (llvm::isa<llvm::AllocaInst>(value)) {
      found_stack = true;
    } else if (const auto *field = llvm::dyn_cast<llvm::GEPOperator>(value)) {
      pending.push_back(field->getPointerOperand());
    } else if (const auto *argument = llvm::dyn_cast<llvm::Argument>(value)) {
      const auto *parent = argument->getParent();
      if (parent->getName() == owner || !parent->hasLocalLinkage())
        return false;
      for (const auto &use : parent->uses()) {
        const auto *call = llvm::dyn_cast<llvm::CallBase>(use.getUser());
        if (!call || &call->getCalledOperandUse() != &use || call->arg_size() <= argument->getArgNo())
          return false;
        pending.push_back(call->getArgOperand(argument->getArgNo()));
      }
    } else if (const auto *load = llvm::dyn_cast<llvm::LoadInst>(value)) {
      // Clang spills reference arguments to a local pointer slot at -O0.
      const auto *slot = llvm::dyn_cast<llvm::AllocaInst>(load->getPointerOperand());
      if (!slot || !slot->getAllocatedType()->isPointerTy())
        return false;
      for (const auto *use : slot->users()) {
        if (const auto *store = llvm::dyn_cast<llvm::StoreInst>(use);
            store && store->getPointerOperand() == slot && store->getValueOperand() != slot)
          pending.push_back(store->getValueOperand());
        else if (!llvm::isa<llvm::LoadInst>(use))
          return false;
      }
    } else if (const auto *call = llvm::dyn_cast<llvm::CallBase>(value)) {
      const auto *callee = call->getCalledFunction();
      if (!callee || callee->isDeclaration())
        return false;
      for (const auto &block : *callee)
        if (const auto *ret = llvm::dyn_cast<llvm::ReturnInst>(block.getTerminator())) {
          if (!ret->getReturnValue())
            return false;
          pending.push_back(ret->getReturnValue());
        }
    } else {
      return false;
    }
  }
  return found_stack;
}

using CallGraph = std::map<std::string, std::set<std::string>>;

void collect_calls(CallGraph &calls, const llvm::Module &module, const std::string &unit) {
  const auto key = [&](const llvm::GlobalValue &value) {
    return value.hasLocalLinkage() ? local_import_name(unit, value.getName().str())
                                   : value.getName().str();
  };
  for (const auto &function : module) {
    if (function.isDeclaration())
      continue;
    auto &edges = calls[key(function)];
    for (const auto &block : function)
      for (const auto &inst : block)
        if (const auto *call = llvm::dyn_cast<llvm::CallBase>(&inst))
          if (const auto *callee = llvm::dyn_cast<llvm::Function>(
                  call->getCalledOperand()->stripPointerCastsAndAliases());
              callee && !callee->isIntrinsic())
            edges.insert(key(*callee));
  }
  for (const auto &alias : module.aliases())
    if (const auto *target = alias.getAliaseeObject())
      calls[key(alias)].insert(key(*target));
}

bool enters_registry(const CallGraph &calls, const std::string &name) {
  constexpr llvm::StringLiteral access("_ZN6tracer4live7managed8Registry6accessEv");
  std::set<std::string> visited;
  std::vector<std::string> pending{name};
  while (!pending.empty()) {
    auto current = std::move(pending.back());
    pending.pop_back();
    if (!visited.insert(current).second)
      continue;
    if (current == access)
      return true;
    if (const auto found = calls.find(current); found != calls.end())
      pending.insert(pending.end(), found->second.begin(), found->second.end());
  }
  return false;
}

// A stored callback keeps the native entry address. Its implementation can
// change independently when the recorded initialization never invokes it.
bool lifecycle_calls(const llvm::Module &module, std::set<std::string> &called) {
  llvm::SmallVector<const llvm::Function *> pending;
  llvm::SmallPtrSet<const llvm::Function *, 32> visited;
  for (const auto *name : {"llvm.global_ctors", "llvm.global_dtors"}) {
    const auto *table = module.getNamedGlobal(name);
    if (!table)
      continue;
    if (!table->hasInitializer())
      return false;
    const auto *list = llvm::dyn_cast<llvm::ConstantArray>(table->getInitializer());
    if (!list)
      return false;
    for (const auto &item : list->operands()) {
      const auto *entry = llvm::dyn_cast<llvm::ConstantStruct>(item.get());
      if (!entry || entry->getNumOperands() != 3)
        return false;
      const auto *function = llvm::dyn_cast<llvm::Function>(entry->getOperand(1)->stripPointerCasts());
      if (!function)
        return false;
      pending.push_back(function);
    }
  }
  while (!pending.empty()) {
    const auto *function = pending.pop_back_val();
    if (!visited.insert(function).second)
      continue;
    const auto name = function->getName();
    called.insert(name.str());
    if (function->isDeclaration() && !function->isIntrinsic() && name != "__cxa_atexit" &&
        name != "__cxa_begin_catch" && name != "_ZSt9terminatev")
      return false;
    for (const auto &block : *function)
      for (const auto &instruction : block)
        if (const auto *call = llvm::dyn_cast<llvm::CallBase>(&instruction)) {
          const auto *callee = call->getCalledFunction();
          if (!callee)
            return false;
          pending.push_back(callee);
        }
  }
  return true;
}

bool unchanged_global_lifecycle(const llvm::Module &before, const llvm::Module &after,
    const std::map<std::string, std::string> &old_data,
    const std::map<std::string, std::string> &new_data,
    const std::map<std::string, Definition> &old_code,
    const std::map<std::string, Definition> &new_code,
    const std::set<std::string> &native_closures) {
  std::vector<const llvm::Value *> pending;
  for (const auto *name : {"llvm.global_ctors", "llvm.global_dtors"}) {
    const auto prior = old_data.find(name);
    const auto next = new_data.find(name);
    if (prior == old_data.end() && next == new_data.end())
      continue;
    if (prior == old_data.end() || next == new_data.end() || prior->second != next->second)
      return false;
    const auto *table = before.getNamedGlobal(name);
    if (!table->use_empty() || !after.getNamedGlobal(name)->use_empty())
      return false;
    pending.push_back(table->getInitializer());
  }
  if (pending.empty())
    return false;
  // Verify the lifecycle table and the code it references before leaving
  // initialization and destruction of native storage with the original ELF.
  llvm::SmallPtrSet<const llvm::Value *, 32> visited;
  std::optional<std::set<std::string>> lifecycle_entries;
  while (!pending.empty()) {
    const auto *value = pending.back();
    pending.pop_back();
    if (!visited.insert(value).second || llvm::isa<llvm::GlobalVariable>(value))
      continue;
    if (const auto *function = llvm::dyn_cast<llvm::Function>(value);
        function && !function->isDeclaration()) {
      const auto name = function->getName().str();
      const auto next = new_code.find(name);
      const auto &prior = old_code.at(name);
      if (next == new_code.end())
        return false;
      if (!equivalent_body(prior, next->second)) {
        if (!native_closures.contains(name) || prior.abi != next->second.abi)
          return false;
        if (!lifecycle_entries) {
          lifecycle_entries.emplace();
          if (!lifecycle_calls(before, *lifecycle_entries) ||
              !lifecycle_calls(after, *lifecycle_entries))
            return false;
        }
        if (lifecycle_entries->contains(name))
          return false;
        continue;
      }
      for (const auto &block : *function)
        for (const auto &instruction : block)
          pending.push_back(&instruction);
    }
    if (const auto *user = llvm::dyn_cast<llvm::User>(value))
      for (const auto &operand : user->operands())
        if (operand)
          pending.push_back(operand.get());
  }
  return true;
}

bool translation_unit(const fs::path &path) {
  const auto suffix = path.extension();
  return suffix == ".cpp" || suffix == ".cc" || suffix == ".cxx" || suffix == ".C";
}

std::size_t reserved_entry_prefix(std::uintptr_t address, std::string_view code) {
  const auto prefix = native_entry::nop_prefix(address, code.substr(0, kMaxPatchBytes), 14);
  return prefix < code.size() ? prefix : 0;
}

Entry bind(std::uint32_t pid, const Image &image, const Symbol &symbol,
           const Definition &definition, const Revision &active) {
  if (symbol.type != llvm::ELF::STT_FUNC || !symbol.size)
    throw std::runtime_error("requires an exact callable ELF symbol with a defined extent");
  const auto bias = client::bind_live_executable(pid, image.path, image.elf,
                                                 symbol.address, symbol.size);
  const auto address = bias + symbol.address;
  const auto prior = active.entries.find(address);
  if (prior != active.entries.end())
    return prior->second.binding;
  const auto code = image.code(symbol);
  const auto prefix = reserved_entry_prefix(address, code);
  if (!prefix)
    throw std::runtime_error("rebuild required: entry needs a reserved 14-byte NOP overwrite window");
  return {symbol.name, signature(symbol.name, definition), site(definition), image.path,
          symbol.address, address, symbol.size, std::string(code.substr(0, prefix)),
          llvm::toHex(llvm::object::getBuildID(&image.elf), true),
          native_entry::may_return_at_end(address, code)};
}

bool thunk_for(const std::string &symbol, const std::string &demangled) {
  return symbol.starts_with("_ZT") && llvm::demangle(symbol).ends_with(" thunk to " + demangled);
}

bool forwards_to_body(const Image &image, const Symbol &thunk, const Symbol &body,
                      const std::string &callable, std::set<std::uint64_t> &chain) {
  using native_entry::Flow;
  if (!chain.insert(thunk.address).second)
    return false;
  const auto decoded = native_entry::scan(thunk.address, image.code(thunk));
  if (decoded.decode_failed || decoded.instructions.empty())
    return false;
  const auto forwards = [&](std::uint64_t address) {
    if (address == body.address)
      return true;
    const auto next = std::ranges::find_if(image.symbols, [&](const Symbol &symbol) {
      return symbol.address == address && symbol.size && symbol.type == llvm::ELF::STT_FUNC &&
             thunk_for(symbol.name, callable);
    });
    if (next == image.symbols.end())
      return false;
    auto path = chain;
    return forwards_to_body(image, *next, body, callable, path);
  };

  // Every returning path must call the body or a verified forwarding
  // thunk. An external unconditional jump may also forward the call.
  std::set<std::pair<std::uint64_t, bool>> pending{{thunk.address, false}};
  bool reached_body = false;
  while (!pending.empty()) {
    auto [pc, forwarded] = *pending.begin();
    pending.erase(pending.begin());
    const auto current = std::ranges::lower_bound(decoded.instructions, pc, {},
                                                   &native_entry::Instruction::address);
    if (current == decoded.instructions.end() || current->address != pc)
      return false;
    if (current->flow == Flow::returns) {
      if (!forwarded)
        return false;
      continue;
    }
    if (current->flow == Flow::call) {
      if (!current->target || !forwards(*current->target))
        return false;
      reached_body = forwarded = true;
    } else if (current->flow == Flow::branch) {
      if (!current->target)
        return false;
      const auto destination = *current->target;
      if (destination >= thunk.address && destination - thunk.address < thunk.size) {
        if (destination <= pc)
          return false;
        pending.emplace(destination, forwarded);
      } else {
        if (current->falls_through || !forwards(destination))
          return false;
        reached_body = true;
        continue;
      }
    }
    if (current->falls_through)
      pending.emplace(pc + current->size, forwarded);
    else if (current->flow != Flow::branch)
      return false;
  }
  return reached_body;
}
} // namespace

Entry bind_native_entry(std::uint32_t pid, const std::string &module,
                        const std::string &linkage, const Revision &active, bool overwrite) {
  const Image image(module, {});
  const auto *symbol = image.find(linkage);
  if (!symbol)
    throw std::runtime_error("missing exact native entry: " + linkage);
  if (!overwrite) {
    if (symbol->type != llvm::ELF::STT_FUNC || !symbol->size)
      throw std::runtime_error("helper does not have an exact native code range: " + linkage);
    const auto bias = client::bind_live_executable(pid, module, image.elf,
                                                   symbol->address, symbol->size);
    return {linkage, llvm::demangle(linkage), {}, module, symbol->address,
            bias + symbol->address, symbol->size, {},
            llvm::toHex(llvm::object::getBuildID(&image.elf), true)};
  }
  for (const auto &alias : image.symbols) {
    if (alias.name != linkage && alias.type == llvm::ELF::STT_FUNC &&
        (alias.address == symbol->address || thunk_for(alias.name, llvm::demangle(linkage))))
      throw std::runtime_error("rebuild required: native alias or thunk needs coverage proof: " + alias.name);
  }
  return bind(pid, image, *symbol, {}, active);
}

std::string live_build_module(const std::string &manifest_path) {
  api::LiveBuildManifest manifest;
  if (!google::protobuf::TextFormat::ParseFromString(read_file(manifest_path), &manifest) ||
      manifest.module().empty())
    throw std::runtime_error("live build manifest does not identify a module");
  return fs::canonical(manifest.module()).string();
}

BuildInputs open_native_build(const std::string &executable,
                             const api::LiveUpdateRequest &request) {
  const auto manifest_path = request.build_manifest();
  const auto database_path = request.compilation_database();
  const auto manifest_contents = read_file(manifest_path);
  const auto database_contents = read_file(database_path);
  api::LiveBuildManifest manifest;
  if (!google::protobuf::TextFormat::ParseFromString(manifest_contents, &manifest) ||
      manifest.module_sha256().size() != 64 ||
      digest(database_contents) != manifest.compilation_database_sha256())
    throw std::runtime_error("invalid or stale live build manifest or compile database");
  const auto module = fs::canonical(manifest.module()).string();
  if (module != executable)
    throw std::runtime_error("build manifest does not identify the selected executable");
  const Image image(module, manifest.module_sha256());
  std::string error;
  const auto database = clang::tooling::JSONCompilationDatabase::loadFromBuffer(
      database_contents, error, clang::tooling::JSONCommandLineSyntax::AutoDetect);
  if (!database)
    throw std::runtime_error("cannot read compilation database: " + error);

  const auto sources = snapshot_inputs(manifest, {}, {}, nullptr);
  BuildInputs result;
  result.module = module;
  result.source_root = sources.root.string();
  result.baseline.files = sources.baseline;
  std::set<std::string> seen;
  for (const auto &command : database->getAllCompileCommands()) {
    const auto path = resolve_path(command.Directory, command.Filename);
    if (!sources.saved.count(path))
      continue;
    if (!seen.insert(path).second)
      throw std::runtime_error("ambiguous compilation profiles: " + path);
    result.baseline.translation_units.push_back(profile(command, path));
  }
  if (result.baseline.translation_units.empty())
    throw std::runtime_error("no build-bound compilation profiles for the source inventory");
  result.revalidate = [manifest_path, manifest_contents, database_path, database_contents,
                       module, sha = manifest.module_sha256()] {
    if (read_file(manifest_path) != manifest_contents ||
        read_file(database_path) != database_contents || digest(read_file(module)) != sha)
      throw Stale("ELF, compile profile or build manifest changed after staging");
  };
  return result;
}

ExtensionSources extension_sources(std::uint32_t pid, const std::string &module,
    const api::LiveUpdateRequest &request, const std::vector<std::string> &paths,
    const std::string &registry) {
  ExtensionSources result;
  const auto build = paths.empty() ? BuildInputs{} : open_native_build(module, request);
  // bind_live_executable checks the actual loaded build ID, not just the
  // pathname. For source execution the selected executable provides main.
  const Image image(module, {});
  const Symbol *anchor = image.find("main");
  if (!anchor) {
    // Advisory scans also accept shared libraries without main.
    const auto found = std::find_if(image.symbols.begin(), image.symbols.end(),
        [](const Symbol &symbol) { return symbol.type == llvm::ELF::STT_FUNC && symbol.size; });
    if (found != image.symbols.end())
      anchor = &*found;
  }
  if (!anchor)
    throw std::runtime_error("no exact ELF code range for live object identity");
  const auto bias = client::bind_live_executable(pid, module, image.elf, anchor->address, anchor->size);
  result.build_id = llvm::toHex(llvm::object::getBuildID(&image.elf), true);
  if (!registry.empty()) {
    try {
      result.entry = bind_native_entry(pid, module, registry, {}, false).address;
    } catch (const std::exception &error) {
      throw std::runtime_error("rebuild required: managed registry entry could not be bound: " +
                               std::string(error.what()));
    }
  }
  if (paths.empty())
    return result;
  result.jit = build.baseline;
  result.jit.translation_units.clear();

  auto arguments = build.baseline.translation_units.front().compiler_arguments;
  const auto directory = build.baseline.translation_units.front().directory;
  // A callback outside the compilation database may use the target's
  // common profile. Reject conflicting ABI options instead of guessing.
  for (const auto &unit : build.baseline.translation_units)
    if (unit.directory != directory || unit.compiler_arguments != arguments)
      throw std::runtime_error("live callbacks require one unambiguous recorded target compile profile");
  const auto sdk = fs::path(TRACER_LIVE_SDK_DIR);
  const auto sdk_dir = fs::exists(sdk / "clang_tracer/live.hpp")
      ? sdk.string() : TRACER_LIVE_BUILD_SDK_DIR;
  arguments.insert(arguments.end(), {"-I", sdk_dir});
  result.jit.compiler_arguments = arguments;

  std::map<std::string, std::string> submitted;
  for (const auto &path : paths) {
    auto content = read_file(path);
    const auto original = std::find_if(request.sources().begin(), request.sources().end(),
        [&](const auto &item) { return item.path() == path; });
    if (original != request.sources().end() && original->contents() != content)
      throw Stale("saved source changed after submission: " + path);
    if (!submitted.emplace(path, content).second)
      throw std::runtime_error("duplicate live source path: " + path);
    auto snapshot = std::find_if(result.jit.files.begin(), result.jit.files.end(),
        [&](const SourceFile &file) { return file.path == path; });
    if (snapshot == result.jit.files.end())
      result.jit.files.push_back({path, std::move(content)});
    else
      snapshot->contents = std::move(content);
    const auto extension = fs::path(path).extension();
    if (extension == ".cpp" || extension == ".cc" || extension == ".cxx" || extension == ".C")
      result.jit.translation_units.push_back({path, directory, arguments});
  }

  // Admit only requested references into the exact loaded image.
  // Link-time relocations stay target-native, including external data.
  for (const auto &unit : result.jit.translation_units) {
    const auto compiled = compile_unit(result.jit, unit);
    const auto add_import = [&](const auto &value, unsigned type) {
      if (!value.isDeclaration() || value.getName().empty())
        return;
      const auto name = value.getName().str();
      const auto *symbol = image.find(name, false);
      if (symbol && symbol->type == type && symbol->address <= UINTPTR_MAX - bias)
        result.jit.imports.emplace(name, bias + symbol->address);
    };
    for (const auto &function : *compiled.module)
      add_import(function, llvm::ELF::STT_FUNC);
    for (const auto &global : compiled.module->globals())
      if (!global.isThreadLocal())
        add_import(global, llvm::ELF::STT_OBJECT);
    for (const auto &dependency : compiled.inputs) {
      if (within(build.source_root, dependency) &&
          std::none_of(result.jit.files.begin(), result.jit.files.end(), [&](const auto &file) {
            return file.path == dependency;
          }))
        throw std::runtime_error("unrecorded project dependency: " + dependency);
    }
  }
  result.validate = [submitted = std::move(submitted), validate_build = build.revalidate] {
    validate_build();
    for (const auto &[path, contents] : submitted)
      if (read_file(path) != contents)
        throw Stale("newer source save invalidated the staged snapshot: " + path);
  };
  return result;
}

void validate_entry(std::uint32_t pid, const Entry &entry) {
  auto binary = Image::open(entry.module);
  const auto &elf = Image::as_elf(*binary.getBinary());
  if (llvm::toHex(llvm::object::getBuildID(&elf), true) != entry.build_id)
    throw Stale("native ELF build ID changed: " + entry.symbol);
  const auto bias = client::bind_live_executable(pid, entry.module, elf,
                                                  entry.elf_address, entry.function_size);
  if (bias + entry.elf_address != entry.address)
    throw Stale("native image address changed: " + entry.symbol);
}

Plan plan_sources(std::uint32_t pid, const std::string &executable,
                  const api::LiveUpdateRequest &request, const Revision &active,
                  const std::function<bool()> &cancelled,
                  const std::set<std::string> *managed_inputs) {
  if (request.sources().empty())
    throw std::runtime_error("submit at least one source or header");
  const auto manifest_path = request.build_manifest();
  const auto manifest_contents = read_file(manifest_path);
  api::LiveBuildManifest manifest;
  if (!google::protobuf::TextFormat::ParseFromString(manifest_contents, &manifest))
    throw std::runtime_error("invalid build input manifest: " + manifest_path);
  if (manifest.source_root().empty() || manifest.snapshot_root().empty() ||
      manifest.module().empty() || manifest.module_sha256().size() != 64)
    throw std::runtime_error("build manifest requires source root, snapshot root and module");
  const auto module = fs::canonical(manifest.module()).string();
  if (module != executable)
    throw std::runtime_error("initial live profile requires the executable's exact ELF build");
  Image image(module, manifest.module_sha256());
  const auto database_path = request.compilation_database();
  const auto database_contents = read_file(database_path);
  if (digest(database_contents) != manifest.compilation_database_sha256())
    throw std::runtime_error("compile database differs from the build manifest");
  std::string error;
  auto database = clang::tooling::JSONCompilationDatabase::loadFromBuffer(
      database_contents, error, clang::tooling::JSONCommandLineSyntax::AutoDetect);
  if (!database)
    throw std::runtime_error("cannot read compilation database: " + error);

  auto sources = snapshot_inputs(manifest, request, active, managed_inputs);
  std::vector<std::string> external_recipe_inputs;
  if (managed_inputs)
    for (const auto &path : *managed_inputs)
      if (!within(sources.root, path))
        external_recipe_inputs.push_back(path);
  const auto verify_recipe_inputs = [&](const CompiledUnit &unit) {
    for (const auto &external : external_recipe_inputs)
      for (const auto &dependency : unit.inputs) {
        std::error_code error;
        if (external == dependency || fs::equivalent(external, dependency, error))
          throw std::runtime_error("native dependency outside the build source root cannot be updated: " +
                                   external);
      }
  };
  const auto commands = database->getAllCompileCommands();
  std::map<std::string, TranslationUnit> profiles;
  for (const auto &command : commands) {
    const auto path = resolve_path(command.Directory, command.Filename);
    if (!sources.saved.count(path) && !sources.submitted.count(path) &&
        !active.sources.count(path))
      continue;
    if (!profiles.emplace(path, profile(command, path)).second)
      throw std::runtime_error("ambiguous compilation profiles: " + path);
  }
  if (profiles.empty())
    throw std::runtime_error("no build-bound compilation profiles for the source inventory");
  const TranslationUnit *inherited = nullptr;
  bool ambiguous_profile = false;
  for (const auto &[path, unit] : profiles) {
    if (!sources.saved.contains(path))
      continue;
    if (!inherited)
      inherited = &unit;
    else if (unit.directory != inherited->directory ||
             unit.compiler_arguments != inherited->compiler_arguments)
      ambiguous_profile = true;
  }
  for (const auto &file : sources.current) {
    if (sources.saved.contains(file.path) || profiles.contains(file.path) ||
        (managed_inputs && managed_inputs->contains(file.path)) || !translation_unit(file.path))
      continue;
    if (!inherited || ambiguous_profile)
      throw std::runtime_error("new translation unit requires one verified build profile: " + file.path);
    auto unit = *inherited;
    unit.path = file.path;
    profiles.emplace(file.path, std::move(unit));
  }
  std::map<std::string, unsigned> source_basenames;
  for (const auto &[path, contents] : sources.saved) {
    (void)contents;
    if (translation_unit(path))
      ++source_basenames[fs::path(path).filename().string()];
  }
  const auto local_symbol = [&](const std::string &name, const std::string &unit) {
    const auto file = fs::path(unit).filename().string();
    const auto count = source_basenames.find(file);
    return count != source_basenames.end() && count->second == 1
        ? image.find_local(name, unit) : nullptr;
  };
  const auto recorded_unit = [&](const std::string &path) {
    const auto basename = fs::path(path).filename().string();
    const auto count = source_basenames.find(basename);
    const auto marker = image.file_markers.find(basename);
    return sources.saved.count(path) && count != source_basenames.end() && count->second == 1 &&
        marker != image.file_markers.end() && marker->second == 1;
  };

  JitRequest current;
  current.files = sources.current;
  current.resolve_standard_runtime = true;
  JitRequest baseline;
  baseline.files = sources.baseline;
  JitRequest original;
  if (!active.sources.empty())
    for (const auto &[path, contents] : sources.saved)
      original.files.push_back({path, contents});
  std::map<std::string, Body> old_bodies, new_bodies;
  CallGraph before_calls, after_calls;
  std::map<std::string, Change> changes;
  std::set<std::string> covered, unchanged;
  std::set<std::string> verified_lambdas;
  std::map<std::string, std::set<std::string>> native_locals;
  bool target_initialized_streams = false;
  struct Reference {
    std::string name;
    std::string unit;
    bool local;
    bool has_storage_definition;
    std::optional<std::uint64_t> size;
  };
  std::multimap<std::string, Reference> referenced;
  Plan result;
  const auto block = [&](const std::string &path, const std::string &name, std::string diagnostic) {
    auto &candidate = result.candidates.emplace_back();
    candidate.set_state(api::LiveUpdateStatus::BLOCKED);
    candidate.set_symbol(name);
    candidate.set_source(path);
    candidate.set_module(module);
    candidate.set_diagnostic(std::move(diagnostic));
  };
  struct Snapshots {
    std::optional<CompiledUnit> before, updated;
  };
  const auto compile_snapshots = [&](const TranslationUnit &unit) {
    Snapshots compiled;
    if (sources.saved.contains(unit.path) || active.sources.contains(unit.path))
      compiled.before.emplace(compile_unit(baseline, unit, nullptr, {},
                                           CompileInspection::native_metadata,
                                           sources.saved.contains(unit.path)
                                               ? &sources.submitted : nullptr));
    if (compiled.before)
      verify_recipe_inputs(*compiled.before);
    if (!compiled.before || sources.submitted.contains(unit.path) ||
        std::ranges::any_of(compiled.before->inputs,
            [&](const auto &input) { return sources.submitted.contains(input); }))
      compiled.updated.emplace(compile_unit(current, unit, nullptr, {},
                                            CompileInspection::native_metadata));
    if (compiled.updated)
      verify_recipe_inputs(*compiled.updated);
    return compiled;
  };
  // Independent frontends use separate LLVM contexts. Leave capacity for
  // target work and keep only one batch of compiled modules in flight.
  auto compile_jobs = std::max(1u, std::thread::hardware_concurrency() / 2);
  if (const auto *value = std::getenv("CLANG_TRACER_LIVE_COMPILE_JOBS")) {
    const std::string_view text(value);
    const auto end = text.data() + text.size();
    const auto [next, error] = std::from_chars(text.data(), end, compile_jobs);
    if (error != std::errc{} || next != end || !compile_jobs)
      throw std::runtime_error("CLANG_TRACER_LIVE_COMPILE_JOBS must be a positive integer");
  }
  std::deque<std::future<Snapshots>> compiling;
  auto next_unit = profiles.begin();
  const auto launch = [&] {
    while (compiling.size() < compile_jobs && next_unit != profiles.end()) {
      const auto *unit = &next_unit++->second;
      compiling.push_back(std::async(std::launch::async, [&compile_snapshots, unit] {
        return compile_snapshots(*unit);
      }));
    }
  };
  launch();
  for (const auto &[path, unit] : profiles) {
    if (cancelled && cancelled())
      throw Stale("live source compilation cancelled");
    auto [before, updated] = compiling.front().get();
    compiling.pop_front();
    launch();
    if (!target_initialized_streams && before && recorded_unit(path) &&
        has_standard_stream_initializer(*before->module))
      target_initialized_streams = true;
    // Only submitted inputs differ between the snapshots. Reuse the first
    // compilation when neither the TU nor any of its includes was submitted.
    const bool changed = updated.has_value();
    const auto &after = updated ? *updated : *before;
    covered.insert(path);
    for (const auto &dependency : after.inputs) {
      if (within(sources.root, dependency) &&
          std::none_of(sources.current.begin(), sources.current.end(), [&](const auto &file) {
            return dependency == file.path;
          }))
        throw std::runtime_error("unrecorded project dependency: " + dependency);
      covered.insert(dependency);
    }
    if (!changed && sources.saved.contains(path)) {
      // No submitted file reaches this TU. Its emitted names still provide
      // coverage evidence for shared symbols, without fingerprinting bodies.
      const auto mark_unchanged = [&](const llvm::GlobalValue &value) {
        const auto name = value.getName().str();
        unchanged.insert(value.hasLocalLinkage() ? local_import_name(path, name) : name);
      };
      for (const auto &function : *after.module)
        if (!function.isDeclaration())
          mark_unchanged(function);
      for (const auto &alias : after.module->aliases())
        mark_unchanged(alias);
      if (managed_inputs) {
        collect_calls(before_calls, *after.module, path);
        collect_calls(after_calls, *after.module, path);
      }
      continue;
    }
    auto after_definitions = definitions(after);
    if (managed_inputs) {
      if (before)
        collect_calls(before_calls, *before->module, path);
      collect_calls(after_calls, *after.module, path);
    }
    bool reuse_global_lifecycle = false;
    std::set<std::string> revision_closures;
    // A private dispatch route from an earlier revision must stay private
    // when an enclosing function is recompiled for a different edit.
    if (const auto prior = active.private_closures.find(path); prior != active.private_closures.end())
      for (const auto &name : prior->second)
        if (after_definitions.contains(name))
          revision_closures.insert(name);
    if (before) {
      auto before_definitions = changed ? definitions(*before) : after_definitions;
      std::optional<CompiledUnit> built;
      std::map<std::string, Definition> built_definitions;
      const auto inspect_build = [&] {
        if (!built) {
          built.emplace(compile_unit(original, unit, nullptr, {}, CompileInspection::native_metadata));
          built_definitions = definitions(*built);
        }
      };
      std::set<std::string> reappeared_patched;
      // Dropping the last use can stop Clang emitting a callable while its
      // native entry still exists. Compare a reappearing body with the exact
      // build snapshot and keep calls routed through that entry.
      if (changed && sources.saved.contains(path) && !original.files.empty() &&
          std::ranges::any_of(after_definitions, [&](const auto &entry) {
            if (before_definitions.contains(entry.first))
              return false;
            return entry.second.local ? local_symbol(entry.first, path) != nullptr
                                      : image.find(entry.first, false) != nullptr;
          })) {
        inspect_build();
        auto vanished = built_definitions;
        std::erase_if(vanished, [&](const auto &entry) {
          return !after_definitions.contains(entry.first);
        });
        // The built definition proves identity, but the current native entry
        // may still dispatch to an earlier live replacement.
        for (const auto &[name, definition] : vanished) {
          if (before_definitions.contains(name))
            continue;
          const auto *symbol = definition.local ? local_symbol(name, path) : image.find(name, false);
          if (symbol && std::ranges::any_of(active.entries, [&](const auto &entry) {
                const auto &binding = entry.second.binding;
                return binding.module == module && binding.elf_address == symbol->address &&
                       !entry.second.mock_original;
              }))
            reappeared_patched.insert(name);
        }
        before_definitions.merge(vanished);
      }
      std::set<std::string> native_closures;
      for (const auto &[name, prior] : before_definitions) {
        const auto next = after_definitions.find(name);
        if (!prior.lambda_site || prior.lambda_site->empty() || next == after_definitions.end() ||
            prior.lambda_site != next->second.lambda_site || prior.abi != next->second.abi ||
            revision_closures.contains(name) ||
            (!reappeared_patched.contains(name) && equivalent_body(prior, next->second)))
          continue;
        const auto *symbol = prior.local ? local_symbol(name, path) : image.find(name, false);
        if (!symbol || symbol->type != llvm::ELF::STT_FUNC || !symbol->size ||
            !reserved_entry_prefix(symbol->address, image.code(*symbol)))
          continue;
        // An earlier revision may have introduced a different closure at the
        // same mangled ordinal. Require the native build's lexical identity.
        if (!original.files.empty()) {
          inspect_build();
          const auto native = built_definitions.find(name);
          if (native == built_definitions.end() || native->second.lambda_site != prior.lambda_site ||
              native->second.abi != prior.abi)
            continue;
        }
        native_closures.insert(name);
        verified_lambdas.insert(prior.local ? local_import_name(path, name) : name);
      }
      if (changed) {
        for (const auto &[name, layout] : before->record_layouts) {
          const auto updated = after.record_layouts.find(name);
          if (updated != after.record_layouts.end() && updated->second != layout)
            block(path, name, "rebuild required: existing record layout changed");
        }
        const auto old_data = data_definitions(*before);
        const auto new_data = data_definitions(after);
        reuse_global_lifecycle = recorded_unit(path) &&
            unchanged_global_lifecycle(*before->module, *after.module,
                old_data, new_data, before_definitions, after_definitions, native_closures);
        for (const auto &[name, storage] : old_data) {
          const auto updated = new_data.find(name);
          const auto *global = before->module->getNamedGlobal(name);
          const bool omitted_local = updated == new_data.end() && global &&
              (global->hasLocalLinkage() || global->hasLinkOnceODRLinkage() ||
               global->hasWeakODRLinkage());
          if (!omitted_local && (updated == new_data.end() || updated->second != storage))
            block(path, name, "rebuild required: existing data definition changed");
        }
        std::optional<std::map<std::string, std::string>> built_data;
        for (const auto &[name, storage] : new_data) {
          const auto *global = after.module->getNamedGlobal(name);
          if (old_data.contains(name) || (global && standard_inline_constant(*global)))
            continue;
          if (global && sources.saved.contains(path) && !original.files.empty()) {
            const auto *symbol = global->hasLocalLinkage() ? local_symbol(name, path)
                                                           : image.find(name, false);
            if (symbol && symbol->type == llvm::ELF::STT_OBJECT && symbol->size) {
              // Storage can also stop being emitted while its native value
              // remains live. A matching built definition reuses that value.
              inspect_build();
              if (!built_data)
                built_data.emplace(data_definitions(*built));
              const auto prior = built_data->find(name);
              if (prior != built_data->end() && prior->second == storage)
                continue;
            }
          }
          block(path, name, "rebuild required: new persistent data definition");
        }
      }
      if (sources.saved.count(path))
        for (const auto &value : (built ? built->module : before->module)->global_values())
          if (value.hasName() && value.hasLocalLinkage())
            native_locals[value.getName().str()].insert(path);
      collect(old_bodies, path, before_definitions);
      std::set<std::string> changed_owners;
      std::set<ClosureScope> changed_families;
      const auto compare_closures = [&](const auto &left, const auto &right) {
        for (const auto &[name, body] : left) {
          if (!body.local || native_closures.contains(name))
            continue;
          auto scope = closure_scope(name, before_definitions);
          if (scope.first.empty() || !after_definitions.contains(scope.first))
            continue;
          const auto other = right.find(name);
          if (other == right.end() || !equivalent_body(body, other->second)) {
            changed_owners.insert(scope.first);
            changed_families.insert(std::move(scope));
          }
        }
      };
      compare_closures(before_definitions, after_definitions);
      compare_closures(after_definitions, before_definitions);
      for (const auto &[name, prior] : before_definitions) {
        if (!prior.lambda_site || native_closures.contains(name))
          continue;
        const auto [owner, ordinal] = closure_scope(name, before_definitions);
        if (owner.empty() || !changed_owners.contains(owner))
          continue;
        const auto next = after_definitions.find(name);
        if (next != after_definitions.end() && equivalent_body(prior, next->second))
          continue;
        const bool captured_state = !prior.has_source_abi || llvm::StringRef(prior.abi).contains(" capture ") ||
            (next != after_definitions.end() && llvm::StringRef(next->second.abi).contains(" capture "));
        if (captured_state && next != after_definitions.end() && prior.abi != next->second.abi)
          block(path, name, "rebuild required: lambda capture layout or callable ABI changed");
        else if (!closure_is_local_to(*before->module, name, owner) ||
            (captured_state && !closure_is_on_stack(*before->module->getFunction(name), owner)))
          block(path, name, "rebuild required: cannot match an existing lambda used outside its enclosing function");
        else {
          result.safe_point = SafePoint::drained_stack;
          // Preserve the whole new dispatch route. Importing an unchanged
          // algorithm instantiation would send the call to the old operator.
          std::set<std::string> route;
          if (next != after_definitions.end() &&
              closure_is_local_to(*after.module, name, owner, &route))
            revision_closures.merge(route);
        }
      }
      for (const auto &[name, body] : after_definitions) {
        if (!body.local || native_closures.contains(name))
          continue;
        const auto scope = closure_scope(name, before_definitions);
        if (!changed_families.contains(scope))
          continue;
        const auto prior = before_definitions.find(name);
        if (body.lambda_site && prior != before_definitions.end() &&
            equivalent_body(prior->second, body) &&
            !closure_is_local_to(*before->module, name, scope.first))
          continue;
        revision_closures.insert(name);
      }
      for (const auto &name : native_closures)
        revision_closures.erase(name);
      // A linker may select one of several differently lowered inline
      // copies. Detect source changes against the same TU's prior IR first.
      for (const auto &[name, prior] : before_definitions) {
        if (revision_closures.contains(name))
          continue;
        const auto key = prior.local ? local_import_name(path, name) : name;
        const auto next = after_definitions.find(name);
        if (next == after_definitions.end() && prior.discardable)
          continue;
        if (next != after_definitions.end() && !changed_owners.contains(name) &&
            !reappeared_patched.contains(name) &&
            equivalent_body(prior, next->second)) {
          unchanged.insert(key);
          continue;
        }
        auto replacement = next == after_definitions.end()
            ? std::nullopt : std::optional<Definition>{next->second};
        auto [item, inserted] = changes.try_emplace(
            key, Change{Body{name, path, prior}, replacement});
        if (!inserted && (item->second.before.definition.abi != prior.abi ||
            !replacement || !item->second.after ||
            (!same_definition(*item->second.after, *replacement) &&
             !equivalent_inline_body(*item->second.after, *replacement))))
          item->second.before.inconsistent = true;
      }
      covered.insert(before->inputs.begin(), before->inputs.end());
    }
    collect(new_bodies, path, std::move(after_definitions));
    // A previous revision may have supplied a TU with no ELF counterpart.
    // Relink its accepted snapshot so new callers can resolve those helpers.
    if (changed || !sources.saved.count(path)) {
      result.private_closures.emplace(path, revision_closures);
      current.translation_units.push_back(unit);
      current.translation_units.back().reuse_global_lifecycle = reuse_global_lifecycle;
      for (const auto &value : after.module->global_values()) {
        if (!value.hasName())
          continue;
        const auto *global = llvm::dyn_cast<llvm::GlobalVariable>(&value);
        if (global && private_constant(*global))
          continue;
        const auto name = value.getName().str();
        if (value.hasLocalLinkage() && revision_closures.contains(name))
          continue;
        const auto import = value.hasLocalLinkage() ? local_import_name(path, name) : name;
        std::optional<std::uint64_t> size;
        if (global && global->getValueType()->isSized())
          size = after.module->getDataLayout().getTypeAllocSize(global->getValueType()).getFixedValue();
        referenced.emplace(import, Reference{name, path, value.hasLocalLinkage(),
                                             global && global->hasInitializer(), size});
      }
    }
  }
  for (const auto &path : sources.submitted)
    if (!covered.count(path) && !(managed_inputs && managed_inputs->contains(path)))
      throw std::runtime_error("no verified translation unit profile or dependency uses " + path);
  if (managed_inputs && changes.empty() && result.candidates.empty() &&
      std::ranges::all_of(sources.submitted, [&](const auto &path) {
        return managed_inputs->contains(path) && !covered.contains(path);
      }))
    return result;

  result.jit = std::move(current);
  for (auto &unit : result.jit.translation_units)
    unit.reuse_stream_initializer = target_initialized_streams;
  for (const auto &file : result.jit.files)
    if (sources.submitted.count(file.path) && covered.contains(file.path))
      result.accepted_sources.push_back(file);
  std::set<std::string> replaced;
  std::map<std::uintptr_t, std::string> entries;
  const auto physical_body = [](const std::string &name, const auto &definitions) {
    const auto item = definitions.find(name);
    if (item == definitions.end())
      return std::string{};
    return item->second.definition.alias_target.empty()
        ? name : item->second.definition.alias_target;
  };
  for (const auto &[key, change] : changes) {
    const auto &body = change.before;
    const auto &name = body.name;
    const auto &next = change.after;
    const auto &old = body.definition;
    const auto *symbol = old.local ? local_symbol(name, body.unit) : image.find(name, false);
    if (!symbol) {
      if (!within(sources.root, old.path))
        continue;
      auto &candidate = result.candidates.emplace_back();
      candidate.set_symbol(name);
      candidate.set_signature(signature(name, old));
      candidate.set_source(site(old));
      candidate.set_module(module);
      candidate.set_state(api::LiveUpdateStatus::BLOCKED);
      candidate.set_diagnostic(old.local
          ? "rebuild required: local definition has no exact ELF translation-unit entry"
          : "rebuild required: changed definition has no standalone native entry");
      continue;
    }
    auto &candidate = result.candidates.emplace_back();
    candidate.set_symbol(name);
    candidate.set_signature(signature(name, old));
    candidate.set_source(site(next ? *next : old));
    candidate.set_module(module);
    try {
      if (managed_inputs && (enters_registry(before_calls, key) || enters_registry(after_calls, key)))
        throw std::runtime_error(
            "rebuild required: acquire the managed registry gate in the caller before entering the updated function");
      if (old.lambda_site && !verified_lambdas.contains(key))
        throw std::runtime_error("rebuild required: cannot verify that the native lambda is the same closure");
      if (!old.has_source_abi || !old.supported_abi || !next || body.inconsistent ||
          unchanged.contains(key) || old.abi != next->abi || !next->supported_abi)
        throw std::runtime_error("rebuild required: emitted implementations or callable ABI differ");
      const auto callable = llvm::demangle(name);
      for (const auto &alias : image.symbols) {
        if (&alias == symbol || alias.type != llvm::ELF::STT_FUNC)
          continue;
        if (alias.address == symbol->address) {
          const auto prior = physical_body(name, old_bodies);
          const auto updated = physical_body(name, new_bodies);
          if (alias.size != symbol->size || prior.empty() || updated.empty() ||
              physical_body(alias.name, old_bodies) != prior ||
              physical_body(alias.name, new_bodies) != updated)
            throw std::runtime_error("rebuild required: unverified folded alias " + alias.name);
          continue;
        }
        if (alias.address > symbol->address && alias.address - symbol->address < symbol->size)
          throw std::runtime_error("rebuild required: overlapping callable entry " + alias.name);
        if (thunk_for(alias.name, callable)) {
          std::set<std::uint64_t> chain;
          if (forwards_to_body(image, alias, *symbol, callable, chain))
            continue;
          const auto previous_thunk = old_bodies.find(alias.name);
          const auto updated_thunk = new_bodies.find(alias.name);
          if (previous_thunk == old_bodies.end() || updated_thunk == new_bodies.end() ||
              !previous_thunk->second.definition.has_source_abi ||
              previous_thunk->second.definition.abi != updated_thunk->second.definition.abi ||
              previous_thunk->second.definition.implementation ==
                  updated_thunk->second.definition.implementation)
            throw std::runtime_error("rebuild required: standalone thunk needs an emitted replacement " +
                                     alias.name);
        }
      }
      if (old.local && (!old.alias_target.empty() || !next->alias_target.empty()))
        throw std::runtime_error("rebuild required: local lifecycle alias needs a verified native entry");
      const auto canonical = physical_body(key, new_bodies);
      auto binding = bind(pid, image, *symbol, old, active);
      candidate.set_address(binding.address);
      candidate.set_state(api::LiveUpdateStatus::PLANNED);
      const auto address = binding.address;
      if (entries.emplace(address, canonical).second) {
        binding.symbol = old.local ? name : canonical;
        result.entries.push_back(std::move(binding));
      } else if (entries.at(address) != canonical) {
        throw std::runtime_error("rebuild required: multiple definitions name one native entry");
      }
      replaced.insert(key);
      replaced.insert(canonical);
      result.jit.imports.emplace(native_entry_import_name(key), address);
    } catch (const std::exception &failure) {
      candidate.set_state(api::LiveUpdateStatus::BLOCKED);
      candidate.set_diagnostic(failure.what());
    }
  }

  const auto *main = image.find("main");
  if (!main)
    throw std::runtime_error("cannot verify the executable load bias without main");
  const auto bias = client::bind_live_executable(pid, module, image.elf, main->address, main->size);
  auto mappings = client::snapshot_live_code_mappings(pid, module, image.elf, bias);
  for (const auto &[import, ref] : referenced) {
    const auto &name = ref.name;
    const bool local = ref.local;
    if (replaced.count(import))
      continue;
    if (local) {
      const auto prior = native_locals.find(name);
      if (prior == native_locals.end() || !prior->second.contains(ref.unit))
        continue;
    }
    const auto *symbol = local ? local_symbol(name, ref.unit) : image.find(name, false);
    if (local && !symbol && ref.size) {
      block(ref.unit, name, "rebuild required: no exact ELF translation-unit storage");
      continue;
    }
    if (!symbol || !symbol->size ||
        (symbol->type != llvm::ELF::STT_FUNC && symbol->type != llvm::ELF::STT_OBJECT))
      continue;
    if (symbol->type == llvm::ELF::STT_FUNC && !result.jit.imports.contains(import)) {
      if (!mappings.contains(symbol->address, symbol->size))
        throw Stale("import moved across ELF instances: " + name);
    }
    // Some external IR declarations, including RTTI vtables, describe a
    // prefix or placeholder for larger native storage.
    if (symbol->type == llvm::ELF::STT_OBJECT && ref.size &&
        (*ref.size > symbol->size || (ref.has_storage_definition && *ref.size != symbol->size))) {
      block(ref.unit, name, "rebuild required: imported storage size differs from the ELF");
      continue;
    }
    result.jit.imports.emplace(import, bias + symbol->address);
  }
  for (const auto &entry : result.entries)
    result.jit.entry_symbols.push_back(entries.at(entry.address));
  if (result.entries.size() > 1)
    result.safe_point = SafePoint::drained_stack;

  result.validate_sources = [files = std::move(sources.submitted),
      snapshots = result.jit.files, module, sha = manifest.module_sha256(),
      database_path, database_contents, manifest_path, manifest_contents,
      current_mappings = std::move(mappings.current)] {
    if (read_file(database_path) != database_contents || read_file(manifest_path) != manifest_contents ||
        digest(read_file(module)) != sha || !current_mappings())
      throw Stale("ELF, compile profile or build manifest changed after staging");
    for (const auto &file : snapshots)
      if (files.count(file.path) && read_file(file.path) != file.contents)
        throw Stale("newer source save invalidated the staged snapshot: " + file.path);
  };
  return result;
}
} // namespace tracer::live

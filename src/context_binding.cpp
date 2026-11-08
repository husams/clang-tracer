#include "context_binding.hpp"

#include <algorithm>
#include <atomic>
#include <map>
#include <set>

#include <llvm/Config/llvm-config.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalAlias.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Local.h>

#include "context_stdlib.hpp"
#include "context_storage_support.hpp"
#include "context_unwind.hpp"
#include "engine_storage.hpp"
#include "tracer/compat.hpp"

namespace tracer {
namespace {
std::atomic<std::uint64_t> next_generation{1};

llvm::Error invalid(const std::string &reason) {
  return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                 "unsupported_state_support_profile: " +
                                     reason);
}

std::string signature(const llvm::Function &function) {
  std::string value;
  llvm::raw_string_ostream stream(value);
  function.getFunctionType()->print(stream);
  stream << ";cc=" << function.getCallingConv()
         << ";ret=" << function.getAttributes().getRetAttrs().getAsString();
  for (unsigned i = 0; i < function.arg_size(); ++i)
    stream << ';' << function.getAttributes().getParamAttrs(i).getAsString();
  return value;
}

const ContextCompilationContract::Bridge *
bridge_contract(const ContextCompilationContract &contract,
                llvm::StringRef symbol) {
  const auto found =
      std::find_if(contract.bridges.begin(), contract.bridges.end(),
                   [&](const auto &bridge) { return bridge.symbol == symbol; });
  return found == contract.bridges.end() ? nullptr : &*found;
}

bool pointer_bridge(const llvm::Function &function) {
  if (function.getReturnType()->isPointerTy())
    return true;
  for (const auto &argument : function.args())
    if (argument.getType()->isPointerTy())
      return true;
  return false;
}

// These intrinsics lower entirely within the compiler-generated module on the
// admitted host. Memory/exception/runtime/target-specific intrinsics are not
// assumed private. The three memory intrinsics are rewritten to the fixed
// native byte-loop catalog before codegen; other runtime intrinsics reject.
bool intrinsic(llvm::Intrinsic::ID id) {
  switch (id) {
  case llvm::Intrinsic::lifetime_start:
  case llvm::Intrinsic::lifetime_end:
  case llvm::Intrinsic::assume:
  case llvm::Intrinsic::is_constant: // GCC 8 std::less; resolved by code generation.
  case llvm::Intrinsic::experimental_noalias_scope_decl:
  case llvm::Intrinsic::sadd_with_overflow:
  case llvm::Intrinsic::uadd_with_overflow:
  case llvm::Intrinsic::ssub_with_overflow:
  case llvm::Intrinsic::usub_with_overflow:
  case llvm::Intrinsic::smul_with_overflow:
  case llvm::Intrinsic::umul_with_overflow:
  case llvm::Intrinsic::bswap:
  case llvm::Intrinsic::ctpop:
  case llvm::Intrinsic::ctlz:
  case llvm::Intrinsic::cttz:
  case llvm::Intrinsic::abs:
  case llvm::Intrinsic::smin:
  case llvm::Intrinsic::smax:
  case llvm::Intrinsic::umin:
  case llvm::Intrinsic::umax:
  case llvm::Intrinsic::memcpy:
  case llvm::Intrinsic::memmove:
  case llvm::Intrinsic::memset:
    return true;
  default:
    return false;
  }
}

llvm::Error constants(const llvm::Value *value,
                      std::set<const llvm::Value *> &seen) {
  const auto *constant = llvm::dyn_cast<llvm::Constant>(value);
  if (!constant || !seen.insert(value).second)
    return llvm::Error::success();
  if (llvm::isa<llvm::BlockAddress>(constant))
    return invalid("unvalidated computed control-flow address");
  if (const auto *expression = llvm::dyn_cast<llvm::ConstantExpr>(constant))
    if (expression->getOpcode() == llvm::Instruction::IntToPtr ||
        expression->getOpcode() == llvm::Instruction::PtrToInt)
      return invalid("unvalidated native address constant in support closure");
  // GlobalValue operands include initializers, aliases and function metadata.
  // Function bodies are scanned separately; walking them here is unnecessary.
  if (llvm::isa<llvm::Function>(constant))
    return llvm::Error::success();
  for (const auto &operand : constant->operands())
    if (auto error = constants(operand.get(), seen))
      return error;
  return llvm::Error::success();
}

enum class StoragePrimitive { none, allocate, release };
StoragePrimitive storage_primitive(llvm::StringRef name) {
  if (name == "__tracer_storage_allocate_v1")
    return StoragePrimitive::allocate;
  if (name == "__tracer_storage_release_v1")
    return StoragePrimitive::release;
  return StoragePrimitive::none;
}

bool storage_signature(const llvm::Function &function,
                       StoragePrimitive primitive) {
  auto &context = function.getContext();
  auto *pointer = llvm::PointerType::getUnqual(context);
  auto *word = llvm::Type::getInt64Ty(context);
  auto *status = llvm::Type::getInt32Ty(context);
  auto *expected =
      primitive == StoragePrimitive::allocate
          ? llvm::FunctionType::get(pointer, {word, word}, false)
          : llvm::FunctionType::get(status, {pointer, word, word}, false);
  if (function.getFunctionType() != expected ||
      function.getCallingConv() != llvm::CallingConv::C)
    return false;
  for (const auto &parameter : function.args()) {
    const auto attributes =
        function.getAttributes().getParamAttrs(parameter.getArgNo());
    for (auto attribute : {llvm::Attribute::StructRet, llvm::Attribute::ByVal,
                           llvm::Attribute::InAlloca, llvm::Attribute::InReg,
                           llvm::Attribute::SExt, llvm::Attribute::ZExt})
      if (attributes.hasAttribute(attribute))
        return false;
  }
  const auto returns = function.getAttributes().getRetAttrs();
  return !returns.hasAttribute(llvm::Attribute::SExt) &&
         !returns.hasAttribute(llvm::Attribute::ZExt) &&
         !returns.hasAttribute(llvm::Attribute::InReg);
}

void bind_storage(llvm::Function &function, StoragePrimitive primitive,
                  const context_engine::StorageDomain &domain) {
  auto &context = function.getContext();
  auto *pointer = llvm::PointerType::getUnqual(context);
  std::vector<llvm::Type *> parameters{pointer};
  for (auto &argument : function.args())
    parameters.push_back(argument.getType());
  auto *native_type =
      llvm::FunctionType::get(function.getReturnType(), parameters, false);
  const auto name = primitive == StoragePrimitive::allocate
                        ? "__tracer_native_storage_allocate_v1"
                        : "__tracer_native_storage_release_v1";
  auto *native =
      llvm::Function::Create(native_type, llvm::GlobalValue::ExternalLinkage,
                             name, function.getParent());
  native->setDoesNotThrow();
  native->setDSOLocal(true);
  function.setLinkage(llvm::GlobalValue::InternalLinkage);
  llvm::IRBuilder<> builder(
      llvm::BasicBlock::Create(context, "engine", &function));
  auto *domain_pointer = llvm::ConstantExpr::getIntToPtr(
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(context),
                             reinterpret_cast<std::uintptr_t>(&domain)),
      pointer);
  std::vector<llvm::Value *> arguments{domain_pointer};
  for (auto &argument : function.args())
    arguments.push_back(&argument);
  builder.CreateRet(builder.CreateCall(native, arguments));
}

const char *memory_name(llvm::Intrinsic::ID id) {
  if (id == llvm::Intrinsic::memcpy)
    return "__tracer_native_memory_copy_v1";
  if (id == llvm::Intrinsic::memmove)
    return "__tracer_native_memory_move_v1";
  return "__tracer_native_memory_set_v1";
}

void bind_memory(llvm::MemIntrinsic &instruction) {
  auto &context = instruction.getContext();
  auto &module = *instruction.getModule();
  auto *pointer = llvm::PointerType::getUnqual(context);
  auto *word = llvm::Type::getInt64Ty(context);
  const auto id = instruction.getIntrinsicID();
  const bool is_set = id == llvm::Intrinsic::memset;
  llvm::Type *second_type = pointer;
  if (is_set)
    second_type = llvm::Type::getInt32Ty(context);
  auto *type = llvm::FunctionType::get(llvm::Type::getVoidTy(context),
                                       {pointer, second_type, word}, false);
  auto callee = module.getOrInsertFunction(memory_name(id), type);
  auto *function = llvm::cast<llvm::Function>(callee.getCallee());
  function->setDoesNotThrow();
  function->setDSOLocal(true);
  llvm::IRBuilder<> builder(&instruction);
  auto *second = instruction.getArgOperand(1);
  if (is_set)
    second = builder.CreateZExt(second, llvm::Type::getInt32Ty(context));
  auto *length = builder.CreateZExtOrTrunc(instruction.getLength(), word);
  builder.CreateCall(callee, {instruction.getRawDest(), second, length});
  instruction.eraseFromParent();
}
} // namespace

llvm::Error initialize_private_context_target() {
  static const bool failed = llvm::InitializeNativeTarget() ||
                             llvm::InitializeNativeTargetAsmPrinter();
  return failed ? invalid("native JIT target is unavailable")
                : llvm::Error::success();
}

struct PrivateContextCode::Impl {
  // Reverse destruction: private callback JIT (including EH registrations)
  // disappears before any owner import can be released. The outer native
  // shared_ptr deleter runs only after the last bridge has returned.
  std::vector<ContextSymbol> imports;
  std::shared_ptr<const context_engine::StorageDomain> storage;
  std::shared_ptr<const ContextStdlibImage> standard_library;
  ContextUnwindTable unwind;
  Role role = Role::owner;
  ContextCompilationContract contract;
  const std::uint64_t generation =
      next_generation.fetch_add(1, std::memory_order_relaxed);
  std::map<std::string, std::pair<std::string, std::uintptr_t>> exports;
  std::unique_ptr<llvm::orc::LLJIT> jit;
};

namespace {
class IndexedMemoryManager final : public llvm::SectionMemoryManager {
public:
  IndexedMemoryManager(ContextUnwindTable &index, unsigned arch)
      : index_(index), arch_(arch) {}
  void registerEHFrames(std::uint8_t *address, std::uint64_t load,
                        std::size_t size) override {
    llvm::SectionMemoryManager::registerEHFrames(address, load, size);
    if (auto error = index_.add_region(address, size, load, arch_))
      index_.record_error(llvm::toString(std::move(error)));
  }

private:
  ContextUnwindTable &index_;
  unsigned arch_;
};
} // namespace

PrivateContextCode::PrivateContextCode(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}
PrivateContextCode::~PrivateContextCode() = default;
std::uint64_t PrivateContextCode::generation() const noexcept {
  return impl_->generation;
}
const ContextCompilationContract &
PrivateContextCode::contract() const noexcept {
  return impl_->contract;
}
const std::shared_ptr<const ContextStdlibImage> &
PrivateContextCode::standard_library() const noexcept {
  return impl_->standard_library;
}
const void *PrivateContextCode::find_fde(
    std::uintptr_t pc, context_support::UnwindBases &bases) const noexcept {
  if (const auto *fde = impl_->unwind.find(pc, bases))
    return fde;
  for (const auto &dependency : impl_->imports)
    if (const auto *fde = dependency.owner_->find_fde(pc, bases))
      return fde;
  return nullptr;
}
const void *PrivateContextCode::unwind_lookup(
    const void *code, std::uintptr_t pc,
    context_support::UnwindBases &bases) noexcept {
  return static_cast<const PrivateContextCode *>(code)->find_fde(pc, bases);
}
std::uint64_t ContextSymbol::generation() const noexcept {
  return owner_ ? owner_->generation() : 0;
}

llvm::Expected<std::vector<ContextNativeSymbol>> ContextNativeSymbol::catalog(
    const std::string &source, const std::vector<std::string> &arguments,
    const std::vector<ContextNativeAddress> &addresses) {
  std::string observed = source;
  for (std::size_t i = 0; i < addresses.size(); ++i)
    observed += "\nauto __clang_tracer_native_catalog_" + std::to_string(i) +
                " = &" + addresses[i].source_name + ";\n";
  auto compiled = compile_context_module(observed, arguments, {});
  if (!compiled)
    return compiled.takeError();
  std::vector<ContextNativeSymbol> result;
  for (const auto &address : addresses) {
    if (!address.address)
      return invalid("null native capability");
    auto source_name = address.source_name;
    if (::tracer::compat::starts_with(source_name, "::")) {
      source_name.erase(0, 2);
    }
    auto symbols = context_bridge_symbols(compiled->contract, {source_name});
    if (!symbols)
      return symbols.takeError();
    const auto *bridge = bridge_contract(compiled->contract, symbols->front());
    ContextNativeSymbol symbol;
    symbol.profile_ = compiled->contract;
    symbol.bridge_ = *bridge;
    symbol.address_ = address.address;
    auto error =
        compiled->module.withModuleDo([&](llvm::Module &module) -> llvm::Error {
          const auto *function = module.getFunction(bridge->symbol);
          if (!function || !function->isDeclaration())
            return invalid(
                "native catalog entry must be an observed declaration: " +
                bridge->source_name);
          symbol.signature_ = signature(*function);
          return llvm::Error::success();
        });
    if (error)
      return error;
    result.push_back(std::move(symbol));
  }
  return result;
}

llvm::Expected<ContextSymbol>
PrivateContextCode::symbol(const std::string &name) const {
  const auto found = impl_->exports.find(name);
  if (found == impl_->exports.end())
    return invalid("symbol is not a generated private export: " + name);
  ContextSymbol result;
  result.owner_ = shared_from_this();
  result.name_ = name;
  result.signature_ = found->second.first;
  if (const auto *bridge = bridge_contract(impl_->contract, name))
    result.bridge_ = *bridge;
  result.address_ = found->second.second;
  return result;
}

llvm::Expected<std::shared_ptr<const PrivateContextCode>>
PrivateContextCode::load(
    llvm::orc::ThreadSafeModule module, ContextCompilationContract contract,
    std::vector<std::string> exports, std::vector<ContextSymbol> imports,
    Role role, std::shared_ptr<const ContextStdlibImage> standard_library,
    std::vector<ContextNativeSymbol> native_imports) {
  if (!module || contract.compiler.empty() || contract.language.empty() ||
      exports.empty())
    return invalid("missing generated compilation contract or exports");
  std::map<std::string, const ContextSymbol *> imported;
  std::map<std::string, const ContextNativeSymbol *> native;
  for (const auto &symbol : native_imports) {
    const auto &profile = symbol.profile_;
    if (!symbol.address_ || profile.target != contract.target ||
        profile.data_layout != contract.data_layout ||
        profile.compiler != contract.compiler ||
        profile.language != contract.language ||
        profile.standard_library != contract.standard_library ||
        !native.emplace(symbol.bridge_.symbol, &symbol).second)
      return invalid("native capability profile differs or repeats");
  }
  const PrivateContextCode *owner = nullptr;
  for (const auto &symbol : imports) {
    if (!symbol.owner_ || !symbol.address_ ||
        symbol.owner_->impl_->role == Role::callback)
      return invalid("import is not an owner/support capability");
    const auto &dependency = *symbol.owner_;
    if (standard_library != dependency.standard_library())
      return invalid(
          "import uses a different sealed standard-library generation");
    if (dependency.impl_->role == Role::owner) {
      if (role != Role::callback)
        return invalid("only a callback may import an owner");
      if (owner && owner != &dependency)
        return invalid("mixed owner generations");
      owner = &dependency;
      if (dependency.contract() != contract) {
        const auto &expected = dependency.contract();
        std::string detail;
        const auto mismatch = [&](std::string_view field) {
          if (!detail.empty())
            detail += ",";
          detail += field;
        };
        if (expected.target != contract.target)
          mismatch("target");
        if (expected.data_layout != contract.data_layout)
          mismatch("data_layout");
        if (expected.compiler != contract.compiler)
          mismatch("compiler");
        if (expected.language != contract.language)
          mismatch("language");
        if (expected.standard_library != contract.standard_library)
          mismatch("standard_library");
        if (expected.types != contract.types) {
          mismatch("types(owner=" + std::to_string(expected.types.size()) +
                   ",callback=" + std::to_string(contract.types.size()) + ")");
        }
        return invalid("owner/callback compilation contracts differ (" +
                       detail + ")");
      }
    } else {
      const auto &other = dependency.contract();
      if (other.target != contract.target ||
          other.data_layout != contract.data_layout ||
          other.compiler != contract.compiler ||
          other.language != contract.language ||
          other.standard_library != contract.standard_library)
        return invalid("support compilation profiles differ");
      for (const auto &type : other.types)
        for (const auto &local : contract.types)
          if (type.name == local.name && type != local)
            return invalid("support type layout differs: " + type.name);
    }
    if (!imported.emplace(symbol.name_, &symbol).second)
      return invalid("duplicate owner import " + symbol.name_);
  }
  if (role == Role::callback) {
    if (!owner)
      return invalid("callback lacks an exact owner capability");
    std::set<const PrivateContextCode *> allowed;
    const auto collect = [&](auto &&self,
                             const PrivateContextCode &code) -> void {
      if (!allowed.insert(&code).second)
        return;
      for (const auto &dependency : code.impl_->imports)
        self(self, *dependency.owner_);
    };
    collect(collect, *owner);
    for (const auto &symbol : imports)
      if (!::tracer::compat::contains(allowed, symbol.owner_.get())) {
        return invalid("callback support is not from its owner's sealed generation");
      }
  }
  auto impl = std::make_unique<Impl>();
  impl->role = role;
  impl->standard_library = std::move(standard_library);
  std::map<std::string, std::uintptr_t> standard_symbols;
  const auto sealed = [&](llvm::StringRef name) -> llvm::Error {
    if (!impl->standard_library)
      return invalid("unsealed external helper: " + name.str());
    auto address = impl->standard_library->symbol(name.str());
    if (!address)
      return address.takeError();
    standard_symbols.emplace(name.str(), *address);
    return llvm::Error::success();
  };
  std::vector<std::pair<llvm::Function *, StoragePrimitive>> storage_primitives;
  std::vector<llvm::MemIntrinsic *> memory_primitives;
  std::vector<llvm::CallBase *> abstract_destructor_traps;
  std::set<llvm::Intrinsic::ID> memory_kinds;
  auto audited = module.withModuleDo([&](llvm::Module &ir) -> llvm::Error {
    std::string verifier;
    llvm::raw_string_ostream stream(verifier);
    if (llvm::verifyModule(ir, &stream))
      return invalid("invalid generated IR: " + verifier);
    // Clang 21 can retain a disconnected missing-return trap block after a
    // complete try/catch. Admit only executable instructions, without relaxing
    // the checks on any block reachable from a function entry.
    for (auto &function : ir)
      if (!function.isDeclaration())
        llvm::removeUnreachableBlocks(function);
    for (auto iterator = ir.begin(); iterator != ir.end();) {
      auto &function = *iterator++;
      if (function.isIntrinsic() && function.use_empty())
        function.eraseFromParent();
    }
    if (llvm::Triple::normalize(ir.getTargetTriple().str()) !=
            contract.target ||
        ir.getDataLayoutStr() != contract.data_layout)
      return invalid("IR target/layout differs from its Clang contract");
    if (!ir.getModuleInlineAsm().empty() || !ir.ifunc_empty() ||
        (!impl->standard_library && !ir.alias_empty()))
      return invalid(
          "unvalidated assembly, resolver or alias in support closure");
    constexpr llvm::StringLiteral stack_check_failure = "__stack_chk_fail";
    if (ir.getNamedValue(stack_check_failure))
      return invalid("stack protector failure symbol is reserved for the backend");
    bool needs_stack_check = false;
    std::set<const llvm::Value *> seen;
    for (const auto &alias : ir.aliases()) {
      if (!llvm::isa<llvm::Function>(alias.getAliaseeObject()))
        return invalid("unvalidated non-function alias");
    }
    for (const auto &global : ir.globals()) {
      if (global.isDeclaration() && impl->standard_library) {
        if (auto error = sealed(global.getName()))
          return error;
        continue;
      }
      if (::tracer::compat::starts_with(global.getName(), "llvm.") || global.isThreadLocal() ||
          ::tracer::compat::starts_with(global.getName(), "__tracer_") || global.isDeclaration() ||
          global.hasAvailableExternallyLinkage() || global.isExternallyInitialized()) {
        return invalid("unvalidated global/lifecycle reference: " + global.getName().str());
      }
      if (auto error = constants(&global, seen))
        return error;
    }
    for (auto &function : ir) {
      const auto primitive = storage_primitive(function.getName());
      if (::tracer::compat::starts_with(function.getName(), "__tracer_") &&
          !(role == Role::support && function.isDeclaration() &&
            primitive != StoragePrimitive::none && storage_signature(function, primitive))) {
        return invalid("reserved private support symbol or ABI: " + function.getName().str());
      }
      if (function.isIntrinsic()) {
        if (impl->standard_library &&
            function.getIntrinsicID() == llvm::Intrinsic::trap) {
          for (auto *user : function.users()) {
            auto *call = llvm::dyn_cast<llvm::CallBase>(user);
            if (!call ||
                !std::binary_search(contract.standard_definitions.begin(),
                                    contract.standard_definitions.end(),
                                    call->getFunction()->getName().str())) {
              return invalid(
                  "trap is not from an admitted standard-header definition: " +
                  (call ? call->getFunction()->getName().str()
                        : std::string("non-call use")));
            }
            // Clang emits a trap for the deleting destructor of an abstract
            // standard control-block base. Keep even this invalid dynamic
            // dispatch inside the sealed runtime instead of a CPU trap.
            abstract_destructor_traps.push_back(call);
          }
          if (auto error = sealed("__cxa_pure_virtual"))
            return error;
          continue;
        }
        if (!intrinsic(function.getIntrinsicID()) &&
            !(impl->standard_library &&
              function.getIntrinsicID() == llvm::Intrinsic::eh_typeid_for)) {
          std::string callers;
          for (const auto *user : function.users())
            if (const auto *instruction =
                    llvm::dyn_cast<llvm::Instruction>(user))
              callers += " " + instruction->getFunction()->getName().str();
          return invalid("unsealed intrinsic: " + function.getName().str() +
                         callers);
        }
        continue;
      }
      if (function.isDeclaration()) {
        if (primitive != StoragePrimitive::none) {
          storage_primitives.emplace_back(&function, primitive);
          continue;
        }
        const auto found = imported.find(function.getName().str());
        if (found == imported.end()) {
          const auto builtin = native.find(function.getName().str());
          if (builtin != native.end()) {
            const auto *cpp = bridge_contract(contract, function.getName());
            if (!cpp || *cpp != builtin->second->bridge_ ||
                signature(function) != builtin->second->signature_)
              return invalid("native capability C++/LLVM ABI mismatch: " +
                             function.getName().str());
            continue;
          }
          if (auto error = sealed(function.getName()))
            return error;
          continue;
        }
        if (signature(function) != found->second->signature_)
          return invalid("owner bridge ABI mismatch: " +
                         function.getName().str());
        const auto *cpp = bridge_contract(contract, function.getName());
        const auto &expected = found->second->bridge_;
        if ((cpp && *cpp != expected) || (!cpp && !expected.symbol.empty()) ||
            (pointer_bridge(function) && (!cpp || expected.symbol.empty())))
          return invalid("owner bridge C++ pointee/definition mismatch: " +
                         function.getName().str());
      } else {
        needs_stack_check |= function.hasFnAttribute(llvm::Attribute::StackProtect) ||
                             function.hasFnAttribute(llvm::Attribute::StackProtectReq) ||
                             function.hasFnAttribute(llvm::Attribute::StackProtectStrong);
        if (::tracer::compat::contains(imported, function.getName().str()) ||
            ::tracer::compat::contains(native, function.getName().str())) {
          return invalid("callback definition shadows owner bridge: " + function.getName().str());
        }
        if (function.hasAvailableExternallyLinkage() ||
            (function.hasPersonalityFn() && !impl->standard_library) ||
            function.hasGC() || function.isVarArg())
          return invalid("unvalidated runtime/ABI requirement: " +
                         function.getName().str());
        if (function.hasPersonalityFn()) {
          const auto *personality = llvm::dyn_cast<llvm::Function>(
              function.getPersonalityFn()->stripPointerCasts());
          if (!personality || personality->getName() != "__gxx_personality_v0")
            return invalid("unvalidated exception personality");
        }
        const bool trusted_standard =
            impl->standard_library &&
            std::binary_search(contract.standard_definitions.begin(),
                               contract.standard_definitions.end(),
                               function.getName().str());
        if (impl->standard_library && !trusted_standard &&
            impl->standard_library->contains(function.getName().str()))
          return invalid("definition shadows sealed support: " +
                         function.getName().str());
        for (auto &block : function)
          for (auto &instruction : block) {
            if (llvm::isa<llvm::ResumeInst>(instruction))
              if (auto error = sealed("_Unwind_Resume"))
                return error;
            for (const auto &operand : instruction.operands())
              if (auto error = constants(operand.get(), seen))
                return error;
            if (llvm::isa<llvm::IndirectBrInst>(instruction))
              return invalid("unvalidated computed control flow");
            if (auto *call = llvm::dyn_cast<llvm::CallBase>(&instruction)) {
              const auto *callee = llvm::dyn_cast<llvm::Function>(
                  call->getCalledOperand()->stripPointerCastsAndAliases());
              if (!callee && !trusted_standard)
                return invalid("unvalidated indirect call in " +
                               function.getName().str());
              if (!callee)
                continue;
              if (call->getFunctionType() != callee->getFunctionType() ||
                  call->getCallingConv() != callee->getCallingConv())
                return invalid(
                    "cast or calling convention changes a support call ABI");
              if (auto *memory = llvm::dyn_cast<llvm::MemIntrinsic>(call)) {
                if (memory->getDestAddressSpace() ||
                    memory->getLength()->getType()->getIntegerBitWidth() > 64 ||
                    (llvm::isa<llvm::MemTransferInst>(memory) &&
                     llvm::cast<llvm::MemTransferInst>(memory)
                         ->getSourceAddressSpace()))
                  return invalid("unsealed intrinsic address space/length ABI");
                memory_primitives.push_back(memory);
                memory_kinds.insert(memory->getIntrinsicID());
              }
            }
            if (!trusted_standard &&
                (llvm::isa<llvm::IntToPtrInst>(instruction) ||
                 llvm::isa<llvm::PtrToIntInst>(instruction)))
              return invalid(
                  "unvalidated native address conversion in support closure");
          }
      }
      // Private definitions are not eligible for application interposition.
      function.setDSOLocal(true);
    }
    for (auto &global : ir.globals())
      global.setDSOLocal(true);
    for (const auto &name : exports) {
      const auto *function = ir.getFunction(name);
      if (!function || function->isDeclaration() ||
          function->hasLocalLinkage() || function->isIntrinsic())
        return invalid("missing generated owner export: " + name);
      if (function->getCallingConv() != llvm::CallingConv::C)
        return invalid(
            "private native export changes the engine calling convention: " +
            name);
      if (pointer_bridge(*function) && !bridge_contract(contract, name))
        return invalid("pointer bridge lacks its observed C++ contract: " +
                       name);
      if (!impl->exports
               .emplace(name, std::pair(signature(*function), std::uintptr_t{}))
               .second)
        return invalid("duplicate owner export: " + name);
    }
    for (const auto &[name, symbol] : imported)
      if (!ir.getFunction(name))
        return invalid("unused/unmatched owner import: " + name);
    // The backend adds this failure call after the audited IR stage.
    if (needs_stack_check)
      return sealed(stack_check_failure);
    return llvm::Error::success();
  });
  if (audited)
    return std::move(audited);
  if (!memory_primitives.empty() || !abstract_destructor_traps.empty()) {
    auto error = module.withModuleDo([&](llvm::Module &ir) -> llvm::Error {
      for (auto *trap : abstract_destructor_traps) {
        auto callee = ir.getOrInsertFunction(
            "__cxa_pure_virtual",
            llvm::FunctionType::get(llvm::Type::getVoidTy(ir.getContext()),
                                    false));
        auto *function = llvm::cast<llvm::Function>(callee.getCallee());
        function->setDoesNotThrow();
        function->setDoesNotReturn();
        llvm::IRBuilder<> builder(trap);
        builder.CreateCall(callee);
        trap->eraseFromParent();
      }
      for (auto *instruction : memory_primitives)
        bind_memory(*instruction);
      if (llvm::verifyModule(ir))
        return invalid("invalid generated memory binding IR");
      return llvm::Error::success();
    });
    if (error)
      return std::move(error);
  }
  // Only the engine may introduce these two native primitive calls. All source
  // IR was checked first, including reserved-name collisions and ABI shape.
  if (!storage_primitives.empty()) {
    impl->storage = context_engine::StorageDomain::create();
    auto error = module.withModuleDo([&](llvm::Module &ir) -> llvm::Error {
      for (auto [function, primitive] : storage_primitives)
        bind_storage(*function, primitive, *impl->storage);
      if (llvm::verifyModule(ir))
        return invalid("invalid generated storage binding IR");
      return llvm::Error::success();
    });
    if (error)
      return std::move(error);
  }
  if (auto error = initialize_private_context_target())
    return std::move(error);
  auto builder = llvm::orc::LLJITBuilder();
  builder.setLinkProcessSymbolsByDefault(false)
      .setProcessSymbolsJITDylibSetup(
          [](llvm::orc::LLJIT &) -> llvm::Expected<llvm::orc::JITDylibSP> {
            return llvm::orc::JITDylibSP{};
          })
      .setPlatformSetUp(llvm::orc::setUpInactivePlatform);
  if (impl->standard_library) {
    auto *index = &impl->unwind;
    const auto arch = llvm::Triple(contract.target).getArch();
    builder.setObjectLinkingLayerCreator(
        [index, arch](llvm::orc::ExecutionSession &session)
            -> llvm::Expected<std::unique_ptr<llvm::orc::ObjectLayer>> {
          return std::make_unique<llvm::orc::RTDyldObjectLinkingLayer>(
              session, [index, arch](const llvm::MemoryBuffer &) {
                return std::make_unique<IndexedMemoryManager>(*index, arch);
              });
        });
  }
  auto created = builder.create();
  if (!created)
    return created.takeError();
  impl->jit = std::move(*created);
  if (impl->jit->getDataLayout().getStringRepresentation() !=
          contract.data_layout ||
      llvm::Triple(contract.target).getArch() !=
          impl->jit->getTargetTriple().getArch())
    return invalid("compiled contract does not match the native private JIT");
  // The default process-symbol JITDylib is neither created nor in link order.
  // Absolute entries come exclusively from retained exact-owner capabilities.
  llvm::orc::SymbolMap symbols;
  for (const auto &[name, address] : standard_symbols)
    symbols[impl->jit->mangleAndIntern(name)] = {
        llvm::orc::ExecutorAddr(address), llvm::JITSymbolFlags::Exported};
  for (const auto &[name, symbol] : native)
    symbols[impl->jit->mangleAndIntern(name)] = {
        llvm::orc::ExecutorAddr(symbol->address_),
        llvm::JITSymbolFlags::Exported};
  for (const auto id : memory_kinds) {
    const auto address = id == llvm::Intrinsic::memcpy
                             ? context_engine::memory_copy_entrypoint()
                         : id == llvm::Intrinsic::memmove
                             ? context_engine::memory_move_entrypoint()
                             : context_engine::memory_set_entrypoint();
    symbols[impl->jit->mangleAndIntern(memory_name(id))] = {
        llvm::orc::ExecutorAddr(address), llvm::JITSymbolFlags::Exported};
  }
  for (auto [function, primitive] : storage_primitives) {
    const auto name = primitive == StoragePrimitive::allocate
                          ? "__tracer_native_storage_allocate_v1"
                          : "__tracer_native_storage_release_v1";
    const auto address = primitive == StoragePrimitive::allocate
                             ? context_engine::storage_allocate_entrypoint()
                             : context_engine::storage_release_entrypoint();
    symbols[impl->jit->mangleAndIntern(name)] = {
        llvm::orc::ExecutorAddr(address), llvm::JITSymbolFlags::Exported};
  }
  for (const auto &symbol : imports)
    symbols[impl->jit->mangleAndIntern(symbol.name_)] = {
        llvm::orc::ExecutorAddr(symbol.address_),
        llvm::JITSymbolFlags::Exported};
  if (!symbols.empty())
    if (auto error = impl->jit->getMainJITDylib().define(
            llvm::orc::absoluteSymbols(std::move(symbols))))
      return std::move(error);
  if (auto error = impl->jit->addIRModule(std::move(module)))
    return std::move(error);
  for (auto &[name, value] : impl->exports) {
    auto address = impl->jit->lookup(name);
    if (!address)
      return address.takeError();
    value.second = address->getValue();
  }
  if (!impl->unwind.error().empty())
    return invalid(impl->unwind.error());
  impl->contract = std::move(contract);
  impl->imports = std::move(imports);
  return std::shared_ptr<const PrivateContextCode>(
      new PrivateContextCode(std::move(impl)));
}
} // namespace tracer

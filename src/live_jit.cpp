#include "tracer/compat.hpp"
#include "live_compile.hpp"

#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/StringSwitch.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DiagnosticInfo.h>
#include <llvm/IR/DiagnosticPrinter.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/IPO/GlobalDCE.h>
#include <llvm/Transforms/IPO/Internalize.h>

#include <dlfcn.h>
#include <link.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <unordered_set>

namespace tracer::live {
namespace {
template <typename T> T checked(llvm::Expected<T> result) {
  if (!result)
    throw std::runtime_error(llvm::toString(result.takeError()));
  return std::move(*result);
}

void checked(llvm::Error error) {
  if (error)
    throw std::runtime_error(llvm::toString(std::move(error)));
}

[[noreturn]] void glibcxx_assert_fail(const char *file, int line, const char *function,
                                      const char *condition) noexcept {
  std::fprintf(stderr, "%s:%d: %s: assertion '%s' failed\n", file, line, function, condition);
  std::abort();
}

std::uintptr_t standard_runtime_import(llvm::StringRef name);

std::uintptr_t runtime_import(llvm::StringRef name) {
  // GCC 8's gthread-active predicate references this pthread alias. Depending
  // on glibc's version it belongs to libc or libpthread; bind the loaded runtime
  // provider rather than treating shared-ownership code as an unknown import.
  if (name == "__pthread_key_create") {
    return standard_runtime_import(name);
  }
  const auto malloc_family = llvm::StringSwitch<bool>(name)
      .Cases("malloc", "calloc", "free", "realloc", true)
      .Cases("aligned_alloc", "posix_memalign", "memalign", "valloc", "pvalloc", true)
      .Default(false);
  const auto new_delete = ::tracer::compat::starts_with(name, "_Znw") || ::tracer::compat::starts_with(name, "_Zna") ||
                         ::tracer::compat::starts_with(name, "_Zdl") || ::tracer::compat::starts_with(name, "_Zda");
  if (malloc_family || new_delete) {
    // Use the target's interposed allocator so objects can cross the boundary
    // between native code and the JIT in either direction.
    return reinterpret_cast<std::uintptr_t>(::dlsym(RTLD_DEFAULT, name.str().c_str()));
  }
  const auto is_c = llvm::StringSwitch<bool>(name)
      .Cases("memcpy", "memset", "memmove", "memcmp", "bcmp", "memchr", true)
      .Cases("strlen", "strcmp", "abort", "puts", true)
      .Cases("printf", "snprintf", "getenv", "strncmp", "usleep", true)
      .Cases("pthread_mutex_lock", "pthread_mutex_unlock", "__libc_single_threaded", true)
      .Default(false);
  const auto is_cxx = ::tracer::compat::starts_with(name, "__cxa_") && name != "__cxa_atexit" &&
                      name != "__cxa_thread_atexit" && name != "__cxa_thread_atexit_impl";
  const auto fundamental_rtti = llvm::StringSwitch<bool>(name)
      .Cases("_ZTIb", "_ZTIc", "_ZTIa", "_ZTIh", "_ZTIs", "_ZTIt", true)
      .Cases("_ZTIi", "_ZTIj", "_ZTIl", "_ZTIm", "_ZTIx", "_ZTIy", true)
      .Cases("_ZTIf", "_ZTId", "_ZTIe", "_ZTIw", "_ZTIv", true)
      .Default(false);
  const auto is_cpp = is_cxx || fundamental_rtti || name == "__gxx_personality_v0" ||
                      ::tracer::compat::starts_with(name, "_ZSt") || ::tracer::compat::starts_with(name, "_ZNSt") ||
                      ::tracer::compat::starts_with(name, "_ZNKSt") || ::tracer::compat::starts_with(name, "_ZTVSt") ||
                      ::tracer::compat::starts_with(name, "_ZNSa") || ::tracer::compat::starts_with(name, "_ZNKSa") ||
                      ::tracer::compat::starts_with(name, "_ZNSi") || ::tracer::compat::starts_with(name, "_ZNSo") ||
                      name == "__dynamic_cast" ||
                      ::tracer::compat::starts_with(name, "_ZTISt") || ::tracer::compat::starts_with(name, "_ZTSSt") ||
                      ::tracer::compat::starts_with(name, "_ZTVN10__cxxabiv1");
  const auto is_unwind = ::tracer::compat::starts_with(name, "_Unwind_");
  if (!is_c && !is_cpp && !is_unwind)
    return 0;
  const auto symbol = name.str();
  const auto library = is_c ? "libc.so.6" : is_unwind ? "libgcc_s.so.1" : "libstdc++.so.6";
  std::unique_ptr<void, decltype(&::dlclose)> handle(
      ::dlopen(library, RTLD_NOW | RTLD_NOLOAD), &::dlclose);
  if (!handle)
    return 0;
  void *address = ::dlsym(handle.get(), symbol.c_str());
  // Red Hat's GCC toolset supplies this assertion handler in a static
  // libstdc++ supplement. Keep the same fail-stop ABI in generated code.
  if (!address && name == "_ZSt21__glibcxx_assert_failPKciS0_S0_")
    return reinterpret_cast<std::uintptr_t>(&glibcxx_assert_fail);
  Dl_info info{};
  if (!address || !::dladdr(address, &info) || !info.dli_fname)
    return 0;
  const auto owner = llvm::sys::path::filename(info.dli_fname);
  const auto expected = is_c ? "libc.so." : is_unwind ? "libgcc_s.so." : "libstdc++.so.";
  if (!::tracer::compat::starts_with(owner, expected))
    return 0;
  auto *visible = ::dlsym(RTLD_DEFAULT, symbol.c_str());
  return reinterpret_cast<std::uintptr_t>(visible ? visible : address);
}

std::uintptr_t standard_runtime_import(llvm::StringRef name) {
  if (name == "__cxa_atexit" || name == "__cxa_thread_atexit" ||
      name == "__cxa_thread_atexit_impl")
    return 0;
  const auto symbol = name.str();
  for (const auto *library : {"libc.so.6", "libm.so.6", "libpthread.so.0",
                              "libstdc++.so.6", "libgcc_s.so.1"}) {
    std::unique_ptr<void, decltype(&::dlclose)> handle(
        ::dlopen(library, RTLD_LAZY | RTLD_NOLOAD), &::dlclose);
    if (!handle)
      continue;
    const auto *address = ::dlsym(handle.get(), symbol.c_str());
    struct link_map *provider = nullptr;
    void *owner = nullptr;
    Dl_info info{};
    if (address && ::dlinfo(handle.get(), RTLD_DI_LINKMAP, &provider) == 0 &&
        ::dladdr1(address, &info, &owner, RTLD_DL_LINKMAP) &&
        (owner == provider ||
         (std::string_view(library) == "libc.so.6" && info.dli_fname &&
          std::string_view(info.dli_fname) == "linux-vdso.so.1")))
      if (const auto *visible = ::dlsym(RTLD_DEFAULT, symbol.c_str())) {
        Dl_info visible_info{};
        if (::dladdr(visible, &visible_info))
          return reinterpret_cast<std::uintptr_t>(visible);
      }
  }
  return 0;
}

class Image final : public JitImage {
public:
  explicit Image(bool resolve_standard_runtime) {
    jit_ = checked(llvm::orc::LLJITBuilder()
        .setLinkProcessSymbolsByDefault(false)
        .setProcessSymbolsJITDylibSetup(
            [](llvm::orc::LLJIT &) -> llvm::Expected<llvm::orc::JITDylibSP> {
              return llvm::orc::JITDylibSP{};
            })
        .setObjectLinkingLayerCreator(
            [](llvm::orc::ExecutionSession &session)
                -> llvm::Expected<std::unique_ptr<llvm::orc::ObjectLayer>> {
              // SectionMemoryManager registers EH frames with the process
              // unwinder, including with the inactive ORC platform.
              return std::make_unique<llvm::orc::RTDyldObjectLinkingLayer>(
                  session, [](const llvm::MemoryBuffer &) {
                    return std::make_unique<llvm::SectionMemoryManager>();
                  });
            })
        .setPlatformSetUp(llvm::orc::setUpInactivePlatform)
        .create());
    jit_->getExecutionSession().setErrorReporter([this](llvm::Error failure) {
      link_error_ = llvm::toString(std::move(failure));
    });
    jit_->getMainJITDylib().addGenerator(
        std::make_unique<RuntimeGenerator>(resolve_standard_runtime));
  }

  void add(std::unique_ptr<llvm::Module> module, std::unique_ptr<llvm::LLVMContext> context,
           const std::map<std::string, std::uintptr_t> &imports) {
    llvm::orc::SymbolMap absolute;
    for (const auto &[name, address] : imports) {
      if (name.empty() || !address)
        throw std::runtime_error("invalid typed import: " + name);
      absolute[jit_->mangleAndIntern(name)] = {
          llvm::orc::ExecutorAddr(address), llvm::JITSymbolFlags::Exported};
    }
    checked(jit_->getMainJITDylib().define(llvm::orc::absoluteSymbols(std::move(absolute))));
    checked(jit_->addIRModule(llvm::orc::ThreadSafeModule(std::move(module), std::move(context))));
  }

  std::uintptr_t lookup(std::string_view symbol) override {
    link_error_.clear();
    auto address = jit_->lookup(llvm::StringRef(symbol.data(), symbol.size()));
    if (!address) {
      auto message = llvm::toString(address.takeError());
      if (!link_error_.empty())
        message += ": " + link_error_;
      throw std::runtime_error(std::string(symbol) + ": " + message);
    }
    return address->getValue();
  }

private:
  class RuntimeGenerator final : public llvm::orc::DefinitionGenerator {
  public:
    explicit RuntimeGenerator(bool resolve_standard_runtime)
        : resolve_standard_runtime_(resolve_standard_runtime) {}

    llvm::Error tryToGenerate(llvm::orc::LookupState &, llvm::orc::LookupKind,
        llvm::orc::JITDylib &dylib, llvm::orc::JITDylibLookupFlags,
        const llvm::orc::SymbolLookupSet &symbols) override {
      llvm::orc::SymbolMap bindings;
      for (const auto &[symbol, flags] : symbols) {
        (void)flags;
        auto address = runtime_import(*symbol);
        if (!address && resolve_standard_runtime_)
          address = standard_runtime_import(*symbol);
        if (address)
          bindings[symbol] = {llvm::orc::ExecutorAddr(address), llvm::JITSymbolFlags::Exported};
      }
      return dylib.define(llvm::orc::absoluteSymbols(std::move(bindings)));
    }
  private:
    bool resolve_standard_runtime_;
  };

  std::string link_error_;
  std::unique_ptr<llvm::orc::LLJIT> jit_;
};

bool source_file(const std::string &path) {
  const auto extension = std::filesystem::path(path).extension();
  return extension == ".cpp" || extension == ".cc" || extension == ".cxx" || extension == ".C";
}

bool only_reads_constant(const llvm::GlobalVariable &global) {
  llvm::SmallPtrSet<const llvm::Value *, 16> seen;
  llvm::SmallVector<const llvm::Value *> pending{&global};
  while (!pending.empty()) {
    const auto *pointer = pending.pop_back_val();
    if (!seen.insert(pointer).second)
      continue;
    for (const auto *use : pointer->users()) {
      if (const auto *copy = llvm::dyn_cast<llvm::MemTransferInst>(use);
          copy && copy->getRawSource() == pointer && copy->getRawDest() != pointer &&
          !copy->isVolatile())
        continue;
      if (const auto *read = llvm::dyn_cast<llvm::LoadInst>(use);
          read && !read->isVolatile() && !read->isAtomic())
        continue;
      if (llvm::isa<llvm::GEPOperator, llvm::BitCastOperator>(use)) {
        pending.push_back(use);
        continue;
      }
      return false;
    }
  }
  return true;
}

void isolate_definitions(llvm::Module &module, const JitRequest &request,
                         const TranslationUnit &unit) {
  // Keep ELF imports for internal-linkage symbols private to their original TU.
  for (auto &value : module.global_values()) {
    if (!value.hasLocalLinkage() || !value.hasName())
      continue;
    const auto import = local_import_name(unit.path, value.getName().str());
    if (auto *function = llvm::dyn_cast<llvm::Function>(&value);
        function && ::tracer::compat::ranges::find(request.entry_symbols, import) != request.entry_symbols.end()) {
      function->setName(import);
      function->setLinkage(llvm::GlobalValue::ExternalLinkage);
      function->setDSOLocal(false);
    } else if (::tracer::compat::contains(request.imports, import))
      value.setName(import);
    else if (::tracer::compat::contains(request.imports, value.getName().str()))
      throw std::runtime_error("unscoped import collides with local symbol: " + value.getName().str());
  }
  // Keep calls in a retained image on the native dispatch path, including
  // calls between entries published together. Either entry may change later.
  std::vector<llvm::GlobalValue *> dispatches;
  for (auto &value : module.global_values())
    if (llvm::isa<llvm::FunctionType>(value.getValueType()) &&
        ::tracer::compat::contains(request.imports, native_entry_import_name(value.getName().str())))
      dispatches.push_back(&value);
  for (auto *entry : dispatches) {
    const auto import = native_entry_import_name(entry->getName().str());
    if (module.getNamedValue(import))
      throw std::runtime_error("native entry dispatch symbol collision: " + import);
    auto *callee = llvm::Function::Create(llvm::cast<llvm::FunctionType>(entry->getValueType()),
                                         llvm::GlobalValue::ExternalLinkage, import, module);
    entry->replaceUsesWithIf(callee, [](const llvm::Use &use) {
      return !llvm::isa<llvm::GlobalAlias>(use.getUser());
    });
  }
  // Use the target instance of known application storage and unchanged code.
  // The retained revision owns only its new code and immutable local constants.
  for (auto &function : module) {
    if (request.imports.count(function.getName().str())) {
      function.deleteBody();
      function.setLinkage(llvm::GlobalValue::ExternalLinkage);
      function.setComdat(nullptr);
      function.setDSOLocal(false);
    }
    if (function.isDeclaration())
      continue;
    if (!supported_entry_abi(function) ||
        (function.hasFnAttribute("target-features") &&
         function.getFnAttribute("target-features").getValueAsString().contains("+shstk")))
      throw std::runtime_error("unsupported native calling convention: " + function.getName().str());
  }
  for (auto &global : module.globals()) {
    if (request.imports.count(global.getName().str())) {
      if (global.isThreadLocal())
        throw std::runtime_error("target TLS cannot be rebound: " + global.getName().str());
      global.setInitializer(nullptr);
      global.setLinkage(llvm::GlobalValue::ExternalLinkage);
      global.setComdat(nullptr);
      global.setDSOLocal(false);
    }
    if (global.hasInitializer() && !global.getName().starts_with("llvm.") &&
        (!global.isConstant() || global.isThreadLocal() ||
         (!global.hasLocalLinkage() && !global.hasLinkOnceODRLinkage() &&
          !standard_inline_constant(global))))
      throw std::runtime_error("unverified global storage in revision: " + global.getName().str());
  }
  if (!module.getModuleInlineAsm().empty() || module.getNamedGlobal("llvm.global_ctors") ||
      module.getNamedGlobal("llvm.global_dtors") || !module.ifunc_empty())
    throw std::runtime_error("module assembly, global lifecycle hooks and IFUNC are unsupported");
  for (auto alias = module.alias_begin(); alias != module.alias_end();) {
    auto &current = *alias++;
    if (!request.imports.count(current.getName().str()))
      continue;
    const auto name = current.getName().str();
    current.setName("");
    if (auto *type = llvm::dyn_cast<llvm::FunctionType>(current.getValueType())) {
      auto *declaration = llvm::Function::Create(type,
          llvm::GlobalValue::ExternalLinkage, name, module);
      current.replaceAllUsesWith(declaration);
    } else {
      throw std::runtime_error("unverified data alias in revision: " + name);
    }
    current.eraseFromParent();
  }
  llvm::StripDebugInfo(module);
}
} // namespace

std::shared_ptr<JitImage> compile_in_target(const JitRequest &request) {
  static const bool initialized = [] {
    return !llvm::InitializeNativeTarget() && !llvm::InitializeNativeTargetAsmPrinter();
  }();
  if (!initialized)
    throw std::runtime_error("cannot initialize the native LLVM target");

  auto units = request.translation_units;
  if (units.empty()) {
    for (const auto &source : request.files) {
      if (source_file(source.path))
        units.push_back({source.path, std::filesystem::path(source.path).parent_path().string(),
                         request.compiler_arguments});
    }
  }
  if (units.empty())
    throw std::runtime_error("no translation units in the source snapshot");

  auto context = std::make_unique<llvm::LLVMContext>();
  std::string link_error;
  context->setDiagnosticHandlerCallBack(
      [](const llvm::DiagnosticInfo *diagnostic, void *data) {
        llvm::raw_string_ostream output(*static_cast<std::string *>(data));
        llvm::DiagnosticPrinterRawOStream printer(output);
        diagnostic->print(printer);
      }, &link_error);
  auto linked = std::make_unique<llvm::Module>("clang-tracer-live-revision", *context);
  const std::unordered_set<std::string> entries(
      request.entry_symbols.begin(), request.entry_symbols.end());
  std::unordered_set<std::string> definitions;
  for (const auto &unit : units) {
    auto compiled = compile_unit(request, unit, context.get());
    auto &module = *compiled.module;
    if (unit.reuse_stream_initializer)
      omit_standard_stream_initializer(module);
    if (unit.reuse_global_lifecycle) {
      for (const auto *name : {"llvm.global_ctors", "llvm.global_dtors"})
        if (auto *table = module.getNamedGlobal(name); table && table->use_empty())
          table->eraseFromParent();
    }
    isolate_definitions(module, request, unit);
    for (auto &function : module) {
      // The IR linker can drop unused linkonce_odr instantiations before
      // ORC sees the caller's exact requested export.
      if (function.hasLinkOnceODRLinkage() && ::tracer::compat::contains(entries, function.getName().str()))
        function.setLinkage(llvm::GlobalValue::WeakODRLinkage);
      if (!function.isDeclaration() && function.hasExternalLinkage() &&
          !definitions.insert(function.getName().str()).second)
        throw std::runtime_error("duplicate native definition: " + function.getName().str());
    }
    if (llvm::Linker::linkModules(*linked, std::move(compiled.module)))
      throw std::runtime_error("cannot link live translation units: " + link_error);
  }
  context->setDiagnosticHandlerCallBack(nullptr);

  if (!entries.empty()) {
    for (const auto &entry : entries) {
      const auto *function = linked->getFunction(entry);
      if (!function || function->isDeclaration() || function->hasLocalLinkage())
        throw std::runtime_error("no generated externally linkable native entry: " + entry);
    }
    llvm::internalizeModule(*linked, [&](const llvm::GlobalValue &value) {
      return entries.count(value.getName().str()) != 0;
    });
    llvm::ModuleAnalysisManager analyses;
    llvm::GlobalDCEPass().run(*linked, analyses);
  }
  if (request.resolve_standard_runtime) {
    // Clang may emit address-sensitive backing storage for unrelated code.
    // Once unused code is dropped, any such surviving object could expose
    // an address different from the native revision's.
    for (const auto &global : linked->globals())
      if (private_constant(global) && !global.hasGlobalUnnamedAddr() &&
          !only_reads_constant(global))
        throw std::runtime_error("rebuild required: unverified address-sensitive constant " +
                                 global.getName().str());
  }
  auto image = std::make_shared<Image>(request.resolve_standard_runtime);
  image->add(std::move(linked), std::move(context), request.imports);
  // Materialize selected entrypoints now, before the transaction stops peers.
  for (const auto &entry : request.entry_symbols)
    image->lookup(entry);
  return image;
}
} // namespace tracer::live

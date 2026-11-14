#include "live_compile.hpp"

#include <llvm/ADT/StringSwitch.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DiagnosticInfo.h>
#include <llvm/IR/DiagnosticPrinter.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/IPO/GlobalDCE.h>
#include <llvm/Transforms/IPO/Internalize.h>

#include <dlfcn.h>

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

std::uintptr_t runtime_import(llvm::StringRef name) {
  const auto is_c = llvm::StringSwitch<bool>(name)
      .Cases("malloc", "calloc", "free", "realloc", true)
      .Cases("memcpy", "memset", "memmove", "memcmp", true)
      .Cases("strlen", "strcmp", "abort", "puts", true)
      .Cases("pthread_mutex_lock", "pthread_mutex_unlock", "__libc_single_threaded", true)
      .Default(false);
  const auto is_cxx = name.starts_with("__cxa_") && name != "__cxa_atexit" &&
                      name != "__cxa_thread_atexit" && name != "__cxa_thread_atexit_impl";
  const auto is_cpp = is_cxx || name == "__gxx_personality_v0" ||
                      name.starts_with("_ZSt") || name.starts_with("_ZNSt") ||
                      name.starts_with("_ZNKSt") || name.starts_with("_ZTVSt") ||
                      name.starts_with("_ZNSa") || name.starts_with("_ZNKSa") ||
                      name.starts_with("_ZNSi") || name.starts_with("_ZNSo") ||
                      name == "__dynamic_cast" ||
                      name.starts_with("_ZTISt") || name.starts_with("_ZTSSt") ||
                      name.starts_with("_ZTVN10__cxxabiv1") ||
                      name.starts_with("_Zn") || name.starts_with("_Zd");
  const auto is_unwind = name.starts_with("_Unwind_");
  if (!is_c && !is_cpp && !is_unwind)
    return 0;
  const auto symbol = name.str();
  const auto library = is_c ? "libc.so.6" : is_unwind ? "libgcc_s.so.1" : "libstdc++.so.6";
  std::unique_ptr<void, decltype(&::dlclose)> handle(
      ::dlopen(library, RTLD_NOW | RTLD_NOLOAD), &::dlclose);
  if (!handle)
    return 0;
  void *address = ::dlsym(handle.get(), symbol.c_str());
  Dl_info info{};
  if (!address || !::dladdr(address, &info) || !info.dli_fname)
    return 0;
  const auto owner = llvm::sys::path::filename(info.dli_fname);
  const auto expected = is_c ? "libc.so." : is_unwind ? "libgcc_s.so." : "libstdc++.so.";
  return owner.starts_with(expected) ? reinterpret_cast<std::uintptr_t>(address) : 0;
}

class Image final : public JitImage {
public:
  Image() {
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
        std::make_unique<RuntimeGenerator>());
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
    llvm::Error tryToGenerate(llvm::orc::LookupState &, llvm::orc::LookupKind,
        llvm::orc::JITDylib &dylib, llvm::orc::JITDylibLookupFlags,
        const llvm::orc::SymbolLookupSet &symbols) override {
      llvm::orc::SymbolMap bindings;
      for (const auto &[symbol, flags] : symbols) {
        (void)flags;
        if (const auto address = runtime_import(*symbol))
          bindings[symbol] = {llvm::orc::ExecutorAddr(address), llvm::JITSymbolFlags::Exported};
      }
      return dylib.define(llvm::orc::absoluteSymbols(std::move(bindings)));
    }

  };

  std::string link_error_;
  std::unique_ptr<llvm::orc::LLJIT> jit_;
};

bool source_file(const std::string &path) {
  const auto extension = std::filesystem::path(path).extension();
  return extension == ".cpp" || extension == ".cc" || extension == ".cxx" || extension == ".C";
}

void isolate_definitions(llvm::Module &module, const JitRequest &request) {
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
    if (function.getCallingConv() != llvm::CallingConv::C || function.isVarArg() ||
        function.hasFnAttribute(llvm::Attribute::Naked) ||
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
         (!global.hasLocalLinkage() && !global.hasLinkOnceODRLinkage())))
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
  std::unordered_set<std::string> definitions;
  for (const auto &unit : units) {
    auto compiled = compile_unit(request, unit, context.get());
    auto &module = *compiled.module;
    isolate_definitions(module, request);
    for (const auto &function : module) {
      if (!function.isDeclaration() && function.hasExternalLinkage() &&
          !definitions.insert(function.getName().str()).second)
        throw std::runtime_error("duplicate native definition: " + function.getName().str());
    }
    if (llvm::Linker::linkModules(*linked, std::move(compiled.module)))
      throw std::runtime_error("cannot link live translation units: " + link_error);
  }
  context->setDiagnosticHandlerCallBack(nullptr);

  if (!request.entry_symbols.empty()) {
    const std::unordered_set<std::string> entries(
        request.entry_symbols.begin(), request.entry_symbols.end());
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
  auto image = std::make_shared<Image>();
  image->add(std::move(linked), std::move(context), request.imports);
  // Materialize selected entrypoints now, before the transaction stops peers.
  for (const auto &entry : request.entry_symbols)
    image->lookup(entry);
  return image;
}
} // namespace tracer::live

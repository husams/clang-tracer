#include "native_plugins.hpp"

#include "repl/clang_tracer_repl_plugin.h"
#ifdef TRACER_RUNTIME_IS_LIBRARY
#include "native_detour.hpp"
#endif

#include <cerrno>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <sys/stat.h>
#include <unistd.h>

#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/Version.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Frontend/Utils.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SHA256.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>

namespace tracer {
namespace {
namespace fs = std::filesystem;

void require(llvm::Error error) {
  if (error)
    throw std::runtime_error(llvm::toString(std::move(error)));
}

template <class T> T require(llvm::Expected<T> value) {
  if (!value)
    require(value.takeError());
  return std::move(*value);
}

class PreprocessPlugin : public clang::PreprocessorFrontendAction {
  llvm::SmallVectorImpl<char> &output_;

public:
  explicit PreprocessPlugin(llvm::SmallVectorImpl<char> &output)
      : output_(output) {}

  void ExecuteAction() override {
    auto &compiler = getCompilerInstance();
    llvm::raw_svector_ostream output(output_);
    clang::DoPrintPreprocessedInput(compiler.getPreprocessor(), &output,
                                    compiler.getPreprocessorOutputOpts());
  }
};

class CompilePlugin : public clang::EmitLLVMOnlyAction {
  std::unique_ptr<llvm::Module> &module_;

public:
  CompilePlugin(llvm::LLVMContext &context,
                std::unique_ptr<llvm::Module> &module)
      : clang::EmitLLVMOnlyAction(&context), module_(module) {}

  void EndSourceFileAction() override {
    clang::EmitLLVMOnlyAction::EndSourceFileAction();
    module_ = takeModule();
  }
};

void clang_action(std::vector<std::string> arguments,
                  std::unique_ptr<clang::FrontendAction> action,
                  llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> filesystem,
                  const fs::path &source) {
  auto files = llvm::makeIntrusiveRefCnt<clang::FileManager>(
      clang::FileSystemOptions{}, std::move(filesystem));
  clang::tooling::ToolInvocation invocation(std::move(arguments),
                                            std::move(action), files.get());
  std::string diagnostics;
  llvm::raw_string_ostream diagnostic_output(diagnostics);
  clang::DiagnosticOptions options;
  clang::TextDiagnosticPrinter printer(diagnostic_output, options);
  invocation.setDiagnosticConsumer(&printer);
  if (!invocation.run())
    throw std::runtime_error("plugin compilation failed: " + source.string() +
                             "\n" + diagnostics);
}

std::string digest(llvm::StringRef bytes) {
  return llvm::toHex(llvm::SHA256::hash(llvm::arrayRefFromStringRef(bytes)),
                     /*LowerCase=*/true);
}

fs::path plugin_cache(const fs::path &directory) {
  fs::create_directories(directory.parent_path());
  if (::mkdir(directory.c_str(), 0700) && errno != EEXIST)
    throw std::system_error(errno, std::generic_category(), directory.string());
  // A cache entry becomes executable code. Use the resolved path and reject
  // ancestors that another user could replace.
  auto canonical = fs::canonical(directory);
  struct stat info{};
  for (auto parent = canonical.parent_path();; parent = parent.parent_path()) {
    if (::stat(parent.c_str(), &info) ||
        (info.st_uid != 0 && info.st_uid != ::geteuid()) ||
        ((info.st_mode & 0022) && !(info.st_mode & S_ISVTX)))
      throw std::runtime_error("untrusted plugin cache ancestor: " +
                               parent.string());
    if (parent == parent.root_path())
      break;
  }
  if (::lstat(canonical.c_str(), &info) || !S_ISDIR(info.st_mode) ||
      info.st_uid != ::geteuid() || (info.st_mode & 0077))
    throw std::runtime_error("plugin cache must be an owner-only directory: " +
                             canonical.string());
  return canonical;
}

std::unique_ptr<llvm::MemoryBuffer> cached_object(const fs::path &path) {
  if (!fs::is_regular_file(fs::symlink_status(path)))
    return {};
  auto file = llvm::MemoryBuffer::getFile(path.string(), false, false, true);
  if (!file || (*file)->getBufferSize() <= 64)
    return {};
  const auto contents = (*file)->getBuffer();
  const auto object = contents.drop_front(64);
  if (contents.take_front(64) != digest(object))
    return {};
  return llvm::MemoryBuffer::getMemBufferCopy(object, path.string());
}

void store_object(const fs::path &path, llvm::StringRef object) {
  auto file = require(llvm::sys::fs::TempFile::create(
      path.string() + ".%%%%%%",
      llvm::sys::fs::owner_read | llvm::sys::fs::owner_write));
  std::error_code error;
  {
    llvm::raw_fd_ostream out(file.FD, false);
    out << digest(object) << object;
    out.flush();
    error = out.error();
    out.clear_error();
  }
  if (error)
    require(llvm::joinErrors(llvm::errorCodeToError(error), file.discard()));
  require(file.keep(path.string()));
}

struct Artifact {
  fs::path path;
  std::unique_ptr<llvm::MemoryBuffer> object;
  bool reused;
};

Artifact plugin_object(llvm::orc::LLJIT &jit, const NativePluginSource &input,
                       const std::vector<std::string> &profile,
                       const fs::path &cache) {
  const auto &source = input.path;
  auto overlay = llvm::makeIntrusiveRefCnt<llvm::vfs::OverlayFileSystem>(
      llvm::vfs::getRealFileSystem());
  auto memory = llvm::makeIntrusiveRefCnt<llvm::vfs::InMemoryFileSystem>();
  overlay->pushOverlay(memory);
  if (input.contents) {
    memory->addFile(source.string(), 0,
                    llvm::MemoryBuffer::getMemBufferCopy(*input.contents));
    memory->addFile(
        (source.parent_path() / "clang_tracer_repl_plugin.h").string(), 0,
        llvm::MemoryBuffer::getMemBufferCopy(input.sdk));
  } else if (!fs::is_regular_file(source)) {
    throw std::runtime_error("plugin source is not a regular file: " +
                             source.string());
  }
  auto arguments = profile;
  arguments.insert(arguments.begin(), "clang++");
  arguments.insert(arguments.end(), {"-E", "-x", "c++", "--", source.string()});
  llvm::SmallVector<char, 0> preprocessed;
  clang_action(std::move(arguments),
               std::make_unique<PreprocessPlugin>(preprocessed), overlay,
               source);

  // Hash what Clang saw, including resolved headers and macro expansions.
  // Compile this same snapshot so edits between the two passes cannot poison
  // the cache. Re-resolving includes also catches newly shadowing headers.
  std::string identity = "tracer-repl-plugin-v1\n" LLVM_VERSION_STRING "\n" +
                         clang::getClangFullVersion();
  identity += '\0' + jit.getTargetTriple().str();
  identity += '\0' + llvm::sys::getHostCPUName().str();
  std::map<std::string, bool> features;
  for (const auto &feature : llvm::sys::getHostCPUFeatures())
    features.emplace(feature.getKey().str(), feature.getValue());
  for (const auto &[feature, enabled] : features) {
    identity += '\0';
    identity += (enabled ? '+' : '-') + feature;
  }
  identity += '\0';
  identity += "clang++";
  for (const auto &flag : profile)
    identity += '\0' + flag;
  identity += '\0';
  identity.append(preprocessed.data(), preprocessed.size());
  auto path = plugin_cache(cache) / (digest(identity) + ".o");
  if (auto object = cached_object(path))
    return {std::move(path), std::move(object), true};

  const auto snapshot = source.string() + ".ii";
  memory->addFile(
      snapshot, 0,
      llvm::MemoryBuffer::getMemBufferCopy(
          llvm::StringRef(preprocessed.data(), preprocessed.size()), snapshot));
  arguments = profile;
  arguments.insert(arguments.begin(), "clang++");
  arguments.insert(arguments.end(), {"-Qunused-arguments", "-c", "-x",
                                     "c++-cpp-output", "--", snapshot});
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module;
  clang_action(std::move(arguments),
               std::make_unique<CompilePlugin>(context, module),
               std::move(overlay), source);
  if (!module)
    throw std::runtime_error("Clang produced no plugin module: " +
                             source.string());

  const auto &target = module->getTargetTriple();
  if (target.getArch() != jit.getTargetTriple().getArch() ||
      target.getObjectFormat() != jit.getTargetTriple().getObjectFormat())
    throw std::runtime_error(
        "plugin compiler profile must target the native JIT");
  if (module->getNamedGlobal("llvm.global_ctors") ||
      module->getNamedGlobal("llvm.global_dtors"))
    throw std::runtime_error("plugins require constant initialization and "
                             "trivial global destruction");
  for (const auto &global : module->globals())
    if (global.isThreadLocal())
      throw std::runtime_error(
          "plugins cannot define or import thread-local variables");
  auto object = require(jit.getIRCompileLayer().getCompiler()(*module));
  return {std::move(path), std::move(object), false};
}

bool valid_name(const std::string &name) {
  constexpr auto allowed =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
  return !name.empty() && name.find_first_not_of(allowed) == name.npos &&
         name.front() != '-';
}

struct Handler {
  std::string help;
  uint64_t (*run)();
};

struct Generation {
  fs::path source;
  llvm::orc::JITDylib *scope;
  std::uint64_t number;
  std::map<std::string, Handler> handlers;
};

std::map<std::string, Handler> read_handlers(llvm::orc::LLJIT &jit,
                                             llvm::orc::JITDylib &scope) {
  auto entry = require(jit.lookup(scope, "tracer_repl_plugin_v1"))
                   .toPtr<decltype(&tracer_repl_plugin_v1)>();
  const auto *descriptor = entry();
  if (!descriptor || descriptor->size != sizeof(TracerReplPluginV1) ||
      !descriptor->commands || !descriptor->command_count)
    throw std::runtime_error("invalid v1 plugin descriptor");
  std::map<std::string, Handler> handlers;
  for (size_t i = 0; i < descriptor->command_count; ++i) {
    const auto &command = descriptor->commands[i];
    if (!command.name || !valid_name(command.name) || !command.help ||
        !command.run)
      throw std::runtime_error("invalid v1 plugin command descriptor");
    if (!handlers.emplace(command.name, Handler{command.help, command.run})
             .second)
      throw std::runtime_error("duplicate plugin command: " +
                               std::string(command.name));
  }
  return handlers;
}
} // namespace

struct NativePlugins::Impl {
  std::string jit_error;
  std::unique_ptr<llvm::orc::LLJIT> jit;
  std::map<std::string, Generation> active;
  std::uint64_t next_scope = 0;

  Impl() {
#ifdef TRACER_RUNTIME_IS_LIBRARY
    std::string error;
    if (!native_detour::initialize_llvm(error))
      throw std::runtime_error(error);
    static const bool target_error = llvm::InitializeNativeTargetAsmParser();
#else
    static const bool target_error = llvm::InitializeNativeTarget() ||
                                     llvm::InitializeNativeTargetAsmPrinter() ||
                                     llvm::InitializeNativeTargetAsmParser();
#endif
    if (target_error)
      throw std::runtime_error("cannot initialize the native plugin JIT");
    jit = require(llvm::orc::LLJITBuilder()
                      .setLinkProcessSymbolsByDefault(false)
                      .setPlatformSetUp(llvm::orc::setUpInactivePlatform)
                      .create());
    jit->getExecutionSession().setErrorReporter([this](llvm::Error error) {
      jit_error = llvm::toString(std::move(error));
    });
  }

  const Generation &find(const std::string &id) const {
    const auto found = active.find(id);
    if (found == active.end())
      throw std::invalid_argument("plugin is not loaded: " + id);
    return found->second;
  }

  LoadResult load(const std::string &id, NativePluginSource source,
                  const std::vector<std::string> &profile,
                  const fs::path &cache, bool reload) {
    const auto previous = active.find(id);
    if (reload)
      find(id);
    else if (previous != active.end())
      throw std::invalid_argument("plugin already loaded: " + id);
    auto artifact = plugin_object(*jit, source, profile, cache);
    jit_error.clear();
    auto &session = jit->getExecutionSession();
    // Bare scopes see only their own object. Peers and host process symbols
    // are intentionally absent from the link order.
    auto &scope =
        session.createBareJITDylib(id + "." + std::to_string(++next_scope));
    const auto number =
        previous == active.end() ? 1 : previous->second.number + 1;
    auto *old_scope =
        previous == active.end() ? nullptr : previous->second.scope;
    try {
      auto object = artifact.reused ? std::move(artifact.object)
                                    : llvm::MemoryBuffer::getMemBufferCopy(
                                          artifact.object->getBuffer(),
                                          artifact.path.string());
      require(jit->addObjectFile(scope, std::move(object)));
      auto handlers = read_handlers(*jit, scope);
      if (!artifact.reused)
        store_object(artifact.path, artifact.object->getBuffer());
      active.insert_or_assign(id, Generation{std::move(source.path), &scope,
                                             number, std::move(handlers)});
    } catch (const std::exception &error) {
      if (auto cleanup = session.removeJITDylib(scope))
        throw std::runtime_error(std::string(error.what()) + "; cleanup: " +
                                 llvm::toString(std::move(cleanup)));
      if (!jit_error.empty())
        throw std::runtime_error(std::string(error.what()) + ": " + jit_error);
      throw;
    }
    if (old_scope)
      if (auto retirement = session.removeJITDylib(*old_scope))
        throw std::runtime_error(id +
                                 " replacement active; retiring old scope: " +
                                 llvm::toString(std::move(retirement)));
    return {number, artifact.reused};
  }
};

NativePlugins::NativePlugins() = default;
NativePlugins::~NativePlugins() = default;

NativePlugins::LoadResult
NativePlugins::load(const std::string &id, NativePluginSource source,
                    const std::vector<std::string> &profile,
                    const fs::path &cache, bool reload) {
  if (!valid_name(id))
    throw std::invalid_argument("invalid plugin name: " + id);
  if (!impl_ && reload)
    throw std::invalid_argument("plugin is not loaded: " + id);
  if (!impl_)
    impl_ = std::make_unique<Impl>();
  return impl_->load(id, std::move(source), profile, cache, reload);
}

NativePlugins::Info NativePlugins::info(const std::string &id) const {
  if (!impl_)
    throw std::invalid_argument("plugin is not loaded: " + id);
  const auto &plugin = impl_->find(id);
  return {id, plugin.source, plugin.number};
}

std::vector<NativePlugins::Info> NativePlugins::list() const {
  std::vector<Info> result;
  if (impl_)
    for (const auto &[id, plugin] : impl_->active)
      result.push_back({id, plugin.source, plugin.number});
  return result;
}

std::vector<std::pair<std::string, std::string>>
NativePlugins::commands(const std::string &id) const {
  if (!impl_)
    throw std::invalid_argument("plugin is not loaded: " + id);
  std::vector<std::pair<std::string, std::string>> result;
  for (const auto &[name, handler] : impl_->find(id).handlers)
    result.emplace_back(name, handler.help);
  return result;
}

std::uint64_t NativePlugins::run(const std::string &id,
                                 const std::string &command) const {
  if (!impl_)
    throw std::invalid_argument("plugin is not loaded: " + id);
  const auto &handlers = impl_->find(id).handlers;
  const auto handler = handlers.find(command);
  if (handler == handlers.end())
    throw std::invalid_argument("unknown plugin command: " + command);
  return handler->second.run();
}
} // namespace tracer

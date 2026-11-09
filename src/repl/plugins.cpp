#include "plugins.hpp"

#include "clang_tracer_repl_plugin.h"
#include "interpreter.hpp"
#include "mapping_cache.hpp"
#include "repl_config.hpp"
#include "server_process.hpp"

#include <cerrno>
#include <filesystem>
#include <map>
#include <ostream>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <sys/stat.h>
#include <unistd.h>

#include <clang/Basic/FileManager.h>
#include <clang/Basic/Version.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Frontend/Utils.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>

namespace tracer_repl {
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
  if (!invocation.run())
    throw std::runtime_error("plugin compilation failed: " + source.string());
}

std::vector<std::string> plugin_profile(const CppEngine &cpp) {
  auto sdk = fs::path(executable_path()).parent_path() /
             fs::path(TRACER_REPL_API_RELATIVE).parent_path();
  if (!fs::is_regular_file(sdk / "clang_tracer_repl_plugin.h"))
    sdk = fs::path(TRACER_REPL_API_HEADER).parent_path();
  auto arguments = cpp.effective_compiler_options();
  arguments.insert(arguments.begin(), {"clang++", "-I", sdk.string()});
  return arguments;
}

fs::path plugin_cache() {
  const auto directory = mapping_cache_directory().parent_path() / "plugins";
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
  if (contents.take_front(64) != mapping_digest(object.str()))
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
    out << mapping_digest(object.str()) << object;
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

Artifact plugin_object(llvm::orc::LLJIT &jit, const fs::path &source,
                       const CppEngine &cpp) {
  const auto profile = plugin_profile(cpp);
  auto real = llvm::vfs::getRealFileSystem();
  auto arguments = profile;
  arguments.insert(arguments.end(), {"-E", "-x", "c++", "--", source.string()});
  llvm::SmallVector<char, 0> preprocessed;
  clang_action(std::move(arguments),
               std::make_unique<PreprocessPlugin>(preprocessed), real, source);

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
  for (const auto &flag : profile)
    identity += '\0' + flag;
  identity += '\0';
  identity.append(preprocessed.data(), preprocessed.size());
  auto path = plugin_cache() / (mapping_digest(identity) + ".o");
  if (auto object = cached_object(path))
    return {std::move(path), std::move(object), true};

  auto memory = llvm::makeIntrusiveRefCnt<llvm::vfs::InMemoryFileSystem>();
  const auto snapshot = source.string() + ".ii";
  memory->addFile(
      snapshot, 0,
      llvm::MemoryBuffer::getMemBufferCopy(
          llvm::StringRef(preprocessed.data(), preprocessed.size()), snapshot));
  auto overlay = llvm::makeIntrusiveRefCnt<llvm::vfs::OverlayFileSystem>(real);
  overlay->pushOverlay(memory);
  arguments = profile;
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

struct Plugins::Impl {
  std::unique_ptr<llvm::orc::LLJIT> jit;
  std::map<std::string, Generation> active;
  std::uint64_t next_scope = 0;

  Impl() {
    if (llvm::InitializeNativeTarget() ||
        llvm::InitializeNativeTargetAsmPrinter() ||
        llvm::InitializeNativeTargetAsmParser())
      throw std::runtime_error("cannot initialize the native plugin JIT");
    jit = require(llvm::orc::LLJITBuilder()
                      .setLinkProcessSymbolsByDefault(false)
                      .setPlatformSetUp(llvm::orc::setUpInactivePlatform)
                      .create());
  }

  void load(const std::string &id, fs::path source, const CppEngine &cpp,
            std::ostream &out) {
    if (!fs::is_regular_file(source))
      throw std::runtime_error("plugin source is not a regular file: " +
                               source.string());
    auto artifact = plugin_object(*jit, source, cpp);
    auto &session = jit->getExecutionSession();
    // Bare scopes see only their own object. Peers and host process symbols
    // are intentionally absent from the link order.
    auto &scope =
        session.createBareJITDylib(id + "." + std::to_string(++next_scope));
    const auto previous = active.find(id);
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
      active.insert_or_assign(id, Generation{std::move(source), &scope, number,
                                             std::move(handlers)});
    } catch (const std::exception &error) {
      if (auto cleanup = session.removeJITDylib(scope))
        throw std::runtime_error(std::string(error.what()) + "; cleanup: " +
                                 llvm::toString(std::move(cleanup)));
      throw;
    }
    if (old_scope)
      if (auto retirement = session.removeJITDylib(*old_scope))
        throw std::runtime_error(id +
                                 " replacement active; retiring old scope: " +
                                 llvm::toString(std::move(retirement)));
    out << id << " generation " << number << " active ("
        << (artifact.reused ? "cache hit" : "compiled") << ")\n";
  }
};

Plugins::Plugins() = default;
Plugins::~Plugins() = default;

bool is_plugin_command(CommandKind kind) {
  using K = CommandKind;
  return kind == K::plugin_load || kind == K::plugin_reload ||
         kind == K::plugin_list || kind == K::plugin_run ||
         kind == K::plugin_commands;
}

void Plugins::execute(const Command &command, const CppEngine &cpp,
                      std::ostream &out) {
  using K = CommandKind;
  if (command.kind == K::plugin_list) {
    if (!impl_ || impl_->active.empty())
      out << "(no plugins loaded)\n";
    else
      for (const auto &[id, generation] : impl_->active)
        out << id << " generation " << generation.number << "  "
            << generation.source.string() << '\n';
    return;
  }

  if (command.kind == K::plugin_load) {
    const auto id = command.option("--as");
    if (!valid_name(id))
      throw std::invalid_argument("invalid plugin name: " + id);
    if (impl_ && impl_->active.count(id))
      throw std::invalid_argument("plugin already loaded: " + id +
                                  "; use %plugin reload");
    if (!impl_)
      impl_ = std::make_unique<Impl>();
    impl_->load(id, fs::absolute(command.arguments.at(0)), cpp, out);
    return;
  }

  const auto &id = command.arguments.at(0);
  if (!impl_ || !impl_->active.count(id))
    throw std::invalid_argument("plugin is not loaded: " + id);
  auto &generation = impl_->active.at(id);
  if (command.kind == K::plugin_reload) {
    impl_->load(id, generation.source, cpp, out);
  } else if (command.kind == K::plugin_commands) {
    for (const auto &[name, handler] : generation.handlers)
      out << "%plugin run " << id << ' ' << name << "  " << handler.help
          << '\n';
  } else {
    const auto &name = command.arguments.at(1);
    const auto handler = generation.handlers.find(name);
    if (handler == generation.handlers.end())
      throw std::invalid_argument("unknown plugin command: " + name +
                                  "; use %plugin commands " + id);
    out << id << '.' << name << " = " << handler->second.run() << '\n';
  }
}
} // namespace tracer_repl

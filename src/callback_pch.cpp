#include "callback_pch.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Attr.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/TargetOptions.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/FrontendOptions.h>
#include <clang/Frontend/MultiplexConsumer.h>
#include <clang/Frontend/PCHContainerOperations.h>
#include <clang/Lex/HeaderSearchOptions.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Lex/Token.h>
#include <fcntl.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SHA256.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <sys/stat.h>
#include <unistd.h>

namespace tracer::callback_pch {
namespace {
namespace fs = std::filesystem;

// The interpreter can omit definitions for templates first instantiated in
// this PCH: char_traits with libstdc++ 8–11 and std::min with libstdc++ 15.
// Keep the affected headers textual so callbacks emit those definitions.
constexpr std::string_view kCommonPrefix = R"PCH(
#pragma once
#include <cstdint>
#include <cstddef>
#if !defined(__GLIBCXX__) || !defined(_GLIBCXX_RELEASE) || \
    (_GLIBCXX_RELEASE > 11 && _GLIBCXX_RELEASE < 15)
#include <string_view>
#include <stdexcept>
#include <type_traits>
#endif
)PCH";

std::string sha256(std::string_view text) {
  llvm::SHA256 digest;
  digest.update(text);
  const auto bytes = digest.final();
  static constexpr char hex[] = "0123456789abcdef";
  std::string result;
  result.reserve(bytes.size() * 2);
  for (const auto byte : bytes) {
    result.push_back(hex[byte >> 4]);
    result.push_back(hex[byte & 0x0f]);
  }
  return result;
}

bool write_all(int fd, std::string_view bytes) noexcept {
  while (!bytes.empty()) {
    const auto written = ::write(fd, bytes.data(), bytes.size());
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (written == 0) {
      return false;
    }
    bytes.remove_prefix(static_cast<std::size_t>(written));
  }
  return true;
}

class PrefixEffects final : public clang::ASTConsumer {
public:
  explicit PrefixEffects(bool &safe) : safe_(safe) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    class Visitor final : public clang::RecursiveASTVisitor<Visitor> {
    public:
      Visitor(clang::ASTContext &context, bool &safe)
          : context_(context), safe_(safe) {}
      bool shouldVisitTemplateInstantiations() const { return true; }
      bool VisitVarDecl(clang::VarDecl *variable) {
        if (!variable->hasGlobalStorage() || variable->isStaticLocal() ||
            variable->getType()->isDependentType() ||
            (variable->hasInit() && (variable->getInit()->isValueDependent() ||
                                     variable->getInit()->isTypeDependent())) ||
            variable->isThisDeclarationADefinition(context_) ==
                clang::VarDecl::DeclarationOnly)
          return true;
        if ((variable->hasInit() && !variable->hasConstantInitialization()) ||
            variable->needsDestruction(context_) != clang::QualType::DK_none)
          safe_ = false;
        return safe_;
      }
      bool VisitFunctionDecl(clang::FunctionDecl *function) {
        if (function->hasAttr<clang::ConstructorAttr>() ||
            function->hasAttr<clang::DestructorAttr>())
          safe_ = false;
        return safe_;
      }
      bool VisitFileScopeAsmDecl(clang::FileScopeAsmDecl *) {
        safe_ = false;
        return false;
      }

    private:
      clang::ASTContext &context_;
      bool &safe_;
    } visitor(context, safe_);
    visitor.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  bool &safe_;
};

class CallbackPchAction final : public clang::GeneratePCHAction {
public:
  bool safe_prefix() const { return safe_; }

protected:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &compiler,
                    llvm::StringRef input) override {
    auto generated =
        clang::GeneratePCHAction::CreateASTConsumer(compiler, input);
    if (!generated)
      return nullptr;
    std::vector<std::unique_ptr<clang::ASTConsumer>> consumers;
    consumers.push_back(std::make_unique<PrefixEffects>(safe_));
    consumers.push_back(std::move(generated));
    return std::make_unique<clang::MultiplexConsumer>(std::move(consumers));
  }

  bool BeginSourceFileAction(clang::CompilerInstance &compiler) override {
    if (!clang::GeneratePCHAction::BeginSourceFileAction(compiler)) {
      return false;
    }
    auto &preprocessor = compiler.getPreprocessor();
    preprocessor.setTokenWatcher([](const clang::Token &token) {
      if (token.is(clang::tok::annot_repl_input_end)) {
        const_cast<clang::Token &>(token).setKind(clang::tok::eof);
      }
    });
    // Keep IncrementalExtensions in the PCH LangOptions for the interpreter
    // variant, but parse this header as one completed input.
    preprocessor.enableIncrementalProcessing(false);
    return true;
  }

private:
  bool safe_ = true;
};

// Missing paths matter too: __has_include and earlier include search roots can
// change their answer even when every previously included header is untouched.
using FileStamp = std::optional<llvm::vfs::Status>;
using Dependencies = std::map<std::string, FileStamp>;

bool same_stamp(const FileStamp &left, const FileStamp &right) {
  if (left.has_value() != right.has_value()) {
    return false;
  }
  return !left ||
         (left->getType() == right->getType() &&
          left->getUniqueID() == right->getUniqueID() &&
          left->getSize() == right->getSize() &&
          left->getLastModificationTime() == right->getLastModificationTime() &&
          left->getPermissions() == right->getPermissions() &&
          left->getUser() == right->getUser() &&
          left->getGroup() == right->getGroup());
}

bool read_stamp(const llvm::ErrorOr<llvm::vfs::Status> &status,
                FileStamp &stamp) {
  if (status) {
    stamp = *status;
    return true;
  }
  if (status.getError() == std::errc::no_such_file_or_directory ||
      status.getError() == std::errc::not_a_directory) {
    stamp.reset();
    return true;
  }
  return false;
}

class RecordingFileSystem final : public llvm::vfs::ProxyFileSystem {
public:
  RecordingFileSystem(llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> base,
                      const fs::path &cache_directory,
                      const fs::path &prefix_path)
      : ProxyFileSystem(std::move(base)), cache_directory_(cache_directory),
        prefix_path_(prefix_path) {}

  llvm::ErrorOr<llvm::vfs::Status> status(const llvm::Twine &path) override {
    auto result = getUnderlyingFS().status(path);
    record(path, result);
    return result;
  }

  bool exists(const llvm::Twine &path) override {
    auto result = status(path);
    return result && result->exists();
  }

  llvm::ErrorOr<std::unique_ptr<llvm::vfs::File>>
  openFileForRead(const llvm::Twine &path) override {
    auto result = getUnderlyingFS().openFileForRead(path);
    if (result) {
      record(path, (*result)->status());
    } else {
      record(path, result.getError());
    }
    return result;
  }

  const Dependencies &dependencies() const { return dependencies_; }

  bool valid() const { return valid_; }

private:
  void record(const llvm::Twine &requested,
              const llvm::ErrorOr<llvm::vfs::Status> &status) {
    fs::path path(requested.str());
    if (!path.is_absolute()) {
      auto cwd = getUnderlyingFS().getCurrentWorkingDirectory();
      if (!cwd) {
        valid_ = false;
        return;
      }
      path = fs::path(*cwd) / path;
    }
    // The PCH output changes this directory during generation. Only its
    // immutable input header participates in the dependency snapshot.
    const auto relative = path.lexically_relative(cache_directory_);
    if (path != prefix_path_ && !relative.empty() &&
        *relative.begin() != "..") {
      return;
    }
    if (path == cache_directory_) {
      return;
    }
    FileStamp stamp;
    if (!read_stamp(status, stamp)) {
      valid_ = false;
      return;
    }
    auto [found, inserted] = dependencies_.emplace(path.string(), stamp);
    if (!inserted && !same_stamp(found->second, stamp)) {
      valid_ = false;
    }
  }

  fs::path cache_directory_;
  fs::path prefix_path_;
  Dependencies dependencies_;
  bool valid_ = true;
};

struct CachedPch {
  Artifact artifact;
  Dependencies dependencies;
  bool disabled = false;
};

bool dependencies_current(const CachedPch &cached) {
  if (cached.dependencies.empty()) {
    return false;
  }
  for (const auto &[path, expected] : cached.dependencies) {
    FileStamp current;
    if (!read_stamp(llvm::vfs::getRealFileSystem()->status(path), current) ||
        !same_stamp(current, expected)) {
      return false;
    }
  }
  return true;
}

class Cache final {
public:
  ~Cache() {
    std::error_code ignored;
    if (!directory_.empty()) {
      fs::remove_all(directory_, ignored);
    }
  }

  std::optional<Artifact> prepare(const clang::CompilerInvocation &base,
                                  Mode mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto &preprocessor = base.getPreprocessorOpts();
    const auto &frontend = base.getFrontendOpts();
    const auto &headers = base.getHeaderSearchOpts();
    const bool empty_builder_input =
        preprocessor.RemappedFileBuffers.size() == 1 &&
        preprocessor.RemappedFileBuffers.front().first == "<<< inputs >>>" &&
        preprocessor.RemappedFileBuffers.front().second &&
        preprocessor.RemappedFileBuffers.front().second->getBufferSize() == 0;
    // This cache never captures user forced includes, remapped buffers or
    // modules/plugins. Those profiles retain the original compilation path.
    if (!preprocessor.Includes.empty() || !preprocessor.MacroIncludes.empty() ||
        !preprocessor.ImplicitPCHInclude.empty() ||
        !preprocessor.ChainedIncludes.empty() ||
        !preprocessor.PCHThroughHeader.empty() || preprocessor.PCHWithHdrStop ||
        preprocessor.PCHWithHdrStopCreate ||
        !preprocessor.RemappedFiles.empty() ||
        (!preprocessor.RemappedFileBuffers.empty() && !empty_builder_input) ||
        preprocessor.DisablePCHOrModuleValidation !=
            clang::DisableValidationForModuleKind::None ||
        !headers.VFSOverlayFiles.empty() ||
        !headers.PrebuiltModuleFiles.empty() ||
        !headers.PrebuiltModulePaths.empty() || !frontend.Plugins.empty() ||
        !frontend.AddPluginActions.empty() || !frontend.PluginArgs.empty() ||
        !frontend.ModuleFiles.empty() || !frontend.ModuleMapFiles.empty()) {
      return std::nullopt;
    }
    if (!ensure_directory()) {
      return std::nullopt;
    }
    const auto key = invocation_key(base, mode);
    if (key.empty()) {
      return std::nullopt;
    }
    auto found = entries_.find(key);
    if (found != entries_.end() && dependencies_current(found->second)) {
      if (found->second.disabled) {
        return std::nullopt;
      }
      return found->second.artifact;
    }
    if (found != entries_.end()) {
      // Keep invalidated artifacts until process teardown: previously created
      // ASTReaders may still hold those files open.
      entries_.erase(found);
    }

    const auto pch_path =
        directory_ /
        (std::string(mode == Mode::Syntax ? "syntax-" : "interpreter-") +
         std::to_string(next_generation_++) + ".pch");
    auto dependencies = build(base, mode, pch_path);
    if (!dependencies) {
      return std::nullopt;
    }
    Artifact artifact{pch_path.string(), prefix_path_.string(), key,
                      base.getTargetOpts().Triple};
    entries_.emplace(key, CachedPch{artifact, std::move(*dependencies), false});
    return artifact;
  }

  void disable(const Artifact &artifact) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = entries_.find(artifact.key);
    if (found != entries_.end() &&
        found->second.artifact.pch_path == artifact.pch_path) {
      found->second.disabled = true;
    }
  }

private:
  bool ensure_directory() {
    if (!directory_.empty()) {
      return true;
    }
    fs::path temporary_root = "/tmp";
    if (const char *configured = std::getenv("TMPDIR");
        configured && *configured && fs::path(configured).is_absolute()) {
      temporary_root = configured;
    }
    std::string pattern = (temporary_root / "clang-tracer-pch-XXXXXX").string();
    std::vector<char> writable_pattern(pattern.begin(), pattern.end());
    writable_pattern.push_back('\0');
    char *created = ::mkdtemp(writable_pattern.data());
    if (!created) {
      return false;
    }
    directory_ = created;
    if (::chmod(directory_.c_str(), 0700) != 0) {
      std::error_code ignored;
      fs::remove_all(directory_, ignored);
      directory_.clear();
      return false;
    }
    prefix_path_ = directory_ / "common-prefix.hpp";
    const int fd = ::open(prefix_path_.c_str(),
                          O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd < 0) {
      std::error_code ignored;
      fs::remove_all(directory_, ignored);
      directory_.clear();
      prefix_path_.clear();
      return false;
    }
    const bool written = write_all(fd, kCommonPrefix);
    const bool closed = ::close(fd) == 0;
    if (!written || !closed) {
      std::error_code ignored;
      fs::remove_all(directory_, ignored);
      directory_.clear();
      prefix_path_.clear();
      return false;
    }
    return true;
  }

  static std::string invocation_key(const clang::CompilerInvocation &base,
                                    Mode mode) {
    clang::CompilerInvocation normalized(base);
    auto &frontend = normalized.getFrontendOpts();
    frontend.Inputs.clear();
    frontend.OutputFile.clear();
    frontend.ProgramAction = clang::frontend::GeneratePCH;
    auto &preprocessor = normalized.getPreprocessorOpts();
    preprocessor.ImplicitPCHInclude.clear();
    preprocessor.clearRemappedFiles();
    normalized.getHeaderSearchOpts().ModulesValidateSystemHeaders = true;
    normalized.getLangOpts().PCHInstantiateTemplates = true;
    if (mode == Mode::Syntax) {
      normalized.getLangOpts().IncrementalExtensions = false;
    }
    auto arguments = normalized.getCC1CommandLine();
    std::string material = LLVM_VERSION_STRING;
    material += mode == Mode::Syntax ? "\nsyntax\n" : "\ninterpreter\n";
    material.append(kCommonPrefix);
    std::error_code error;
    const auto cwd = fs::current_path(error);
    if (error) {
      return {};
    }
    material += "\n" + cwd.string() + "\n";
    for (const auto &argument : arguments) {
      material += argument;
      material.push_back('\0');
    }
    return sha256(material);
  }

  std::optional<Dependencies> build(const clang::CompilerInvocation &base,
                                    Mode mode, const fs::path &pch_path) {
    auto invocation = std::make_shared<clang::CompilerInvocation>(base);
    auto &frontend = invocation->getFrontendOpts();
    frontend.Inputs.clear();
    frontend.Inputs.emplace_back(prefix_path_.string(),
                                 clang::InputKind(clang::Language::CXX));
    frontend.ProgramAction = clang::frontend::GeneratePCH;
    frontend.OutputFile = pch_path.string();
    invocation->getPreprocessorOpts().ImplicitPCHInclude.clear();
    invocation->getPreprocessorOpts().clearRemappedFiles();
    invocation->getHeaderSearchOpts().ModulesValidateSystemHeaders = true;
    // Complete ODR-used header template definitions while Sema still owns its
    // translation-unit scope, before the effect gate and PCH serialization.
    invocation->getLangOpts().PCHInstantiateTemplates = true;
    if (mode == Mode::Syntax) {
      invocation->getLangOpts().IncrementalExtensions = false;
    }

    auto containers = std::make_shared<clang::PCHContainerOperations>();
    clang::CompilerInstance compiler(invocation, containers);
    auto recording = llvm::makeIntrusiveRefCnt<RecordingFileSystem>(
        llvm::vfs::createPhysicalFileSystem(), directory_, prefix_path_);
    clang::IgnoringDiagConsumer diagnostics;
#if LLVM_VERSION_MAJOR >= 22
    compiler.setVirtualFileSystem(recording);
    compiler.createDiagnostics(&diagnostics, false);
#else
    compiler.createFileManager(recording);
    compiler.createDiagnostics(*recording, &diagnostics, false);
#endif
    CallbackPchAction action;
    if (!compiler.ExecuteAction(action) || !action.safe_prefix()) {
      std::error_code ignored;
      fs::remove(pch_path, ignored);
      return std::nullopt;
    }
    std::error_code error;
    if (!fs::is_regular_file(pch_path, error) || error ||
        fs::file_size(pch_path, error) == 0 || error ||
        ::chmod(pch_path.c_str(), 0600) != 0) {
      fs::remove(pch_path, error);
      return std::nullopt;
    }
    if (!recording->valid() || recording->dependencies().empty()) {
      fs::remove(pch_path, error);
      return std::nullopt;
    }
    return recording->dependencies();
  }

  std::mutex mutex_;
  fs::path directory_;
  fs::path prefix_path_;
  std::uint64_t next_generation_ = 1;
  std::map<std::string, CachedPch> entries_;
};

Cache &cache() {
  static Cache instance;
  return instance;
}

} // namespace

std::optional<Artifact> prepare(const clang::CompilerInvocation &base,
                                Mode mode) {
  try {
    return cache().prepare(base, mode);
  } catch (...) {
    return std::nullopt;
  }
}

void attach(clang::CompilerInvocation &invocation, const Artifact &artifact) {
  invocation.getPreprocessorOpts().ImplicitPCHInclude = artifact.pch_path;
  invocation.getPreprocessorOpts().Includes.push_back(artifact.prefix_path);
  invocation.getHeaderSearchOpts().ModulesValidateSystemHeaders = true;
}

void append_syntax_args(std::vector<std::string> &arguments,
                        const Artifact &artifact) {
  // IncrementalCompilerBuilder and tooling can have different vendor defaults.
  // The consumer must use the same target as the invocation that built the PCH.
  arguments.push_back("--target=" + artifact.target_triple);
  arguments.push_back("-fmodules-validate-system-headers");
  arguments.push_back("-include-pch");
  arguments.push_back(artifact.pch_path);
  arguments.push_back("-include");
  arguments.push_back(artifact.prefix_path);
}

void disable(const Artifact &artifact) noexcept {
  try {
    cache().disable(artifact);
  } catch (...) {
  }
}

} // namespace tracer::callback_pch

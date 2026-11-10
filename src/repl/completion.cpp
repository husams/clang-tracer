// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// ASTUnit setup follows LLVM's clang/lib/Interpreter/CodeCompletion.cpp.
// See LICENSES/LLVM.txt. Session input is parsed for types, never executed.
#include "completion.hpp"
#include "record_mappings.hpp"
#include "mapping_cache.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>

#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Attr.h>
#include <clang/AST/Expr.h>
#include <clang/AST/PrettyPrinter.h>
#include <llvm/Config/llvm-config.h>
#if LLVM_VERSION_MAJOR >= 22
#include <clang/Options/Options.h>
#else
#include <clang/Driver/Options.h>
#endif
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Interpreter/Interpreter.h>
#include <clang/Interpreter/PartialTranslationUnit.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/HeaderSearch.h>
#include <clang/Lex/HeaderSearchOptions.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <clang/Sema/CodeCompleteConsumer.h>
#include <clang/Sema/Lookup.h>
#include <clang/Sema/Sema.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Option/ArgList.h>
#include <llvm/Option/OptTable.h>
#if LLVM_VERSION_MAJOR >= 22
#include <clang/Interpreter/IncrementalExecutor.h>
#endif

namespace tracer_repl {
namespace {
std::string absolute_input(clang::CompilerInstance &compiler, llvm::StringRef name) {
  llvm::SmallString<256> path(name);
  if (compiler.getFileManager().getVirtualFileSystem().makeAbsolute(path))
    throw std::runtime_error("cannot resolve application header path");
  return std::filesystem::weakly_canonical(path.str().str()).string();
}
class HeaderInclude final : public clang::PPCallbacks {
  clang::CompilerInstance &compiler_;
  std::map<std::string, std::string> &resolved_;
  std::map<std::string, std::string> &lookups_;
  std::map<std::string, std::string> &roots_;
  bool &reusable_;
  void lookup(clang::SourceLocation location, llvm::StringRef filename, bool angled,
              clang::OptionalFileEntryRef file) {
    std::set<std::filesystem::path> directories;
    std::set<std::filesystem::path> frameworks;
    auto &sources = compiler_.getSourceManager();
    if (!angled) {
      const auto parent = sources.getFilename(sources.getSpellingLoc(location));
      directories.insert(std::filesystem::path(parent.str()).parent_path());
    }
    for (const auto &dir : compiler_.getPreprocessor().getHeaderSearchInfo().search_dir_range()) {
      if (dir.isNormalDir()) directories.insert(dir.getName().str());
      else if (dir.isFramework()) frameworks.insert(dir.getName().str());
      else reusable_ = false; // Header maps require regeneration.
    }
    const auto &options = compiler_.getHeaderSearchOpts();
    for (const auto &entry : options.UserEntries) {
      std::filesystem::path path(entry.Path);
      if (!entry.IgnoreSysRoot && path.is_absolute() && !options.Sysroot.empty())
        path = std::filesystem::path(options.Sysroot) / path.relative_path();
      if (entry.IsFramework) frameworks.insert(path);
      else directories.insert(path);
    }
    for (const auto *entries : {&directories, &frameworks}) for (const auto &directory : *entries) {
      llvm::SmallString<256> root(directory.string());
      if (compiler_.getFileManager().getVirtualFileSystem().makeAbsolute(root))
        throw std::runtime_error("cannot resolve include root");
      const auto path = std::filesystem::path(root.str().str()).lexically_normal();
      const auto state = mapping_search_root(path);
      const auto previous = roots_.find(path.string());
      if (previous != roots_.end() && previous->second != state)
        throw std::runtime_error("include root changed during AST analysis");
      roots_[path.string()] = state;
      if (state.rfind("file:", 0) == 0) reusable_ = false;
    }
    if (std::filesystem::path(filename.str()).is_absolute()) directories = {""};
    bool represented = !file;
    const auto selected = file ? absolute_input(compiler_, file->getName()) : "";
    std::set<std::filesystem::path> candidates;
    for (const auto &directory : directories) candidates.insert(directory / filename.str());
    const auto slash = filename.find('/');
    if (!std::filesystem::path(filename.str()).is_absolute() && slash != llvm::StringRef::npos) {
      for (const auto &framework : frameworks) {
        const auto bundle = framework / (filename.substr(0, slash).str() + ".framework");
        candidates.insert(bundle / "Headers" / filename.substr(slash + 1).str());
        candidates.insert(bundle / "PrivateHeaders" / filename.substr(slash + 1).str());
      }
    }
    for (const auto &entry : candidates) {
      llvm::SmallString<256> candidate(entry.string());
      if (compiler_.getFileManager().getVirtualFileSystem().makeAbsolute(candidate))
        throw std::runtime_error("cannot resolve include search path");
      const auto path = std::filesystem::path(candidate.str().str()).lexically_normal();
      const auto actual = std::filesystem::is_regular_file(path)
          ? std::filesystem::weakly_canonical(path).string() : "";
      const auto previous = lookups_.find(path.string());
      if (previous != lookups_.end() && previous->second != actual)
        throw std::runtime_error("include search changed during AST analysis");
      lookups_[path.string()] = actual;
      represented |= actual == selected;
      if (lookups_.size() > 20000) throw std::runtime_error("include search manifest is too large");
    }
    reusable_ &= represented;
  }
public:
  HeaderInclude(clang::CompilerInstance &compiler, std::map<std::string, std::string> &resolved,
                std::map<std::string, std::string> &lookups,
                std::map<std::string, std::string> &roots, bool &reusable)
      : compiler_(compiler), resolved_(resolved),
        lookups_(lookups), roots_(roots), reusable_(reusable) {}
  void InclusionDirective(clang::SourceLocation location, const clang::Token &, llvm::StringRef filename,
                           bool angled, clang::CharSourceRange, clang::OptionalFileEntryRef file,
                           llvm::StringRef, llvm::StringRef, const clang::Module *, bool,
                           clang::SrcMgr::CharacteristicKind) override {
    if (file) {
      const auto requested = resolved_.find(filename.str());
      if (requested != resolved_.end() && requested->second.empty())
        requested->second = absolute_input(compiler_, file->getName());
    }
    lookup(location, filename, angled, file);
  }
  void HasInclude(clang::SourceLocation location, llvm::StringRef filename, bool angled,
                  clang::OptionalFileEntryRef file, clang::SrcMgr::CharacteristicKind) override {
    lookup(location, filename, angled, file);
  }
};
#if LLVM_VERSION_MAJOR >= 22
namespace compiler_options = clang::options;
using clang::getDriverOptTable;
#else
namespace compiler_options = clang::driver::options;
using clang::driver::getDriverOptTable;
#endif
class Consumer final : public clang::CodeCompleteConsumer {
  CppCompletions &out_;
  std::shared_ptr<clang::GlobalCodeCompletionAllocator> allocator_ =
      std::make_shared<clang::GlobalCodeCompletionAllocator>();
  clang::CodeCompletionTUInfo info_{allocator_};

public:
  explicit Consumer(CppCompletions &out)
      : clang::CodeCompleteConsumer(clang::CodeCompleteOptions{}), out_(out) {}
  clang::CodeCompletionAllocator &getAllocator() override {
    return *allocator_;
  }
  clang::CodeCompletionTUInfo &getCodeCompletionTUInfo() override {
    return info_;
  }
  void ProcessCodeCompleteResults(clang::Sema &sema,
                                  clang::CodeCompletionContext context,
                                  clang::CodeCompletionResult *results,
                                  unsigned count) override {
    out_.prefix = sema.getPreprocessor().getCodeCompletionFilter().str();
    const bool check_access = sema.getLangOpts().AccessControl;
    for (unsigned i = 0; i < count; ++i) {
      const auto &result = results[i];
      std::string name;
      if (result.Kind == clang::CodeCompletionResult::RK_Declaration &&
          !result.Hidden && result.Declaration->getIdentifier() &&
          (!check_access ||
           (result.Declaration->getAccess() != clang::AS_private &&
            result.Declaration->getAccess() != clang::AS_protected)))
        name = result.Declaration->getNameAsString();
      else if (result.Kind == clang::CodeCompletionResult::RK_Keyword &&
               context.getBaseType().isNull())
        name = result.Keyword;
      if (!name.empty() && name.rfind(out_.prefix, 0) == 0)
        out_.items.push_back(std::move(name));
    }
  }
};

class VoidStatementConsumer final : public clang::ASTConsumer {
  bool &missing_;

public:
  explicit VoidStatementConsumer(bool &missing) : missing_(missing) {}
  bool HandleTopLevelDecl(clang::DeclGroupRef group) override {
    for (const auto *decl : group) {
      const auto *statement = llvm::dyn_cast<clang::TopLevelStmtDecl>(decl);
      if (!statement || !statement->isSemiMissing())
        continue;
      const auto *expr = llvm::dyn_cast<clang::Expr>(statement->getStmt());
      // Incremental syntax-only parsing accepts a missing statement terminator
      // even on a loop/if. Code-generating Parse can then crash in
      // ExtractValueFromExpr, rather than diagnose the malformed statement.
      // Only bare expressions participate in the REPL's value-printing syntax.
      if (!expr) {
        auto &diagnostics = statement->getASTContext().getDiagnostics();
        const auto id = diagnostics.getCustomDiagID(
            clang::DiagnosticsEngine::Error, "expected ';' after statement");
        diagnostics.Report(statement->getEndLoc(), id);
        return false;
      }
      if (expr && expr->getType()->isVoidType())
        missing_ = true;
    }
    return true;
  }
};

class CheckInterpreter final : public clang::Interpreter {
public:
  CheckInterpreter(std::unique_ptr<clang::CompilerInstance> ci,
                   llvm::Error &error, bool &missing)
      : clang::Interpreter(std::move(ci), error, nullptr,
                           std::make_unique<VoidStatementConsumer>(missing)) {}
};

// Clang's incremental parser can loop on an end-of-input annotation inside an
// unfinished statement, even after a fatal diagnostic. The checker has no live
// session state: stop it on the first error, before parser recovery can loop.
class CheckDiagnostics final : public clang::TextDiagnosticPrinter {
public:
  explicit CheckDiagnostics(clang::DiagnosticOptions &options)
      : clang::TextDiagnosticPrinter(llvm::errs(), options) {}
  void HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                        const clang::Diagnostic &diagnostic) override {
    if (level < clang::DiagnosticsEngine::Error)
      return;
    clang::TextDiagnosticPrinter::HandleDiagnostic(level, diagnostic);
    if (level >= clang::DiagnosticsEngine::Error) {
      llvm::errs().flush();
      std::_Exit(1);
    }
  }
};

std::unique_ptr<clang::CompilerInstance>
compiler_for(const std::vector<std::string> &arguments) {
  std::vector<const char *> argv;
  for (const auto &argument : arguments)
    argv.push_back(argument.c_str());
  unsigned missing_index = 0, missing_count = 0;
  const auto parsed_arguments = getDriverOptTable().ParseArgs(
      argv, missing_index, missing_count);
  if (missing_count)
    throw std::runtime_error("C++ compiler option is missing its value");
  for (const auto *argument : parsed_arguments)
    if (argument->getOption().matches(compiler_options::OPT_UNKNOWN) ||
        argument->getOption().matches(compiler_options::OPT_INPUT))
      throw std::runtime_error("unrecognized compiler option or unexpected input file: " + argument->getAsString(parsed_arguments));
  clang::IncrementalCompilerBuilder builder;
  builder.SetCompilerArgs(argv);
  auto instance = builder.CreateCpp();
  if (!instance)
    throw std::runtime_error(llvm::toString(instance.takeError()));
  if ((*instance)->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("invalid C++ compiler arguments");
  switch ((*instance)->getFrontendOpts().ProgramAction) {
  case clang::frontend::EmitAssembly:
  case clang::frontend::EmitBC:
  case clang::frontend::EmitLLVM:
  case clang::frontend::EmitLLVMOnly:
  case clang::frontend::EmitCodeGenOnly:
  case clang::frontend::EmitObj:
    break;
  default:
    throw std::runtime_error("compiler options must preserve C++ code generation; "
                             "preprocessing and syntax-only actions are unsupported");
  }
  (*instance)->getDiagnostics().setSuppressAllDiagnostics(true);
  return std::move(*instance);
}

void print_function(llvm::raw_ostream &out, const clang::FunctionDecl &function,
                    const clang::PrintingPolicy &policy) {
  if (!llvm::isa<clang::CXXConstructorDecl>(function))
    out << function.getReturnType().getAsString(policy) << " ";
  out << function.getNameAsString() << "(";
  bool first = true;
  for (const auto *parameter : function.parameters()) {
    if (!first)
      out << ", ";
    first = false;
    out << parameter->getType().getAsString(policy) << " "
        << parameter->getNameAsString();
    if (parameter->hasDefaultArg()) {
      out << " = ";
      parameter->getDefaultArg()->printPretty(out, nullptr, policy);
    }
  }
  out << ")";
  if (const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(&function))
    if (method->isConst())
      out << " const";
  out << ";\n";
}

clang::DeclContext* symbol_scope(clang::NamedDecl* declaration,
                                clang::tok::TokenKind separator) {
  clang::QualType type;
  if (separator == clang::tok::coloncolon) {
    if (auto* alias = llvm::dyn_cast<clang::NamespaceAliasDecl>(declaration))
      return alias->getNamespace();
    if (auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(declaration))
      return record->getDefinition();
    if (auto* record = llvm::dyn_cast<clang::ClassTemplateDecl>(declaration))
      return record->getTemplatedDecl()->getDefinition();
    if (auto* scope = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
      return scope;
    if (auto* scope = llvm::dyn_cast<clang::EnumDecl>(declaration))
      return scope->getDefinition();
    if (const auto* alias = llvm::dyn_cast<clang::TypedefNameDecl>(declaration))
      type = alias->getUnderlyingType();
  } else if (const auto* value = llvm::dyn_cast<clang::ValueDecl>(declaration)) {
    type = value->getType().getNonReferenceType();
    if (separator == clang::tok::arrow) type = type->getPointeeType();
  }
  if (!type.isNull())
    if (auto* record = type->getAsCXXRecordDecl())
      return record->getDefinition();
  return nullptr;
}

clang::PrintingPolicy readable_policy(const clang::CompilerInstance& compiler) {
  clang::PrintingPolicy policy(compiler.getLangOpts());
  policy.SuppressTagKeyword = true;
  policy.SuppressUnwrittenScope = true;
  policy.SuppressInlineNamespace =
      static_cast<unsigned>(clang::PrintingPolicy::SuppressInlineNamespaceMode::All);
  return policy;
}

// Some standard libraries omit preferred_name on basic_string. Apply the
// standard aliases to exact default specializations, including nested types.
std::string preferred_string_names(std::string text, const clang::LangOptions& language) {
  if (text.find("std::basic_string<") == std::string::npos) return text;
  constexpr std::pair<llvm::StringLiteral, llvm::StringLiteral> aliases[] = {
      {"std::basic_string<char>", "std::string"},
      {"std::basic_string<wchar_t>", "std::wstring"},
      {"std::basic_string<char16_t>", "std::u16string"},
      {"std::basic_string<char32_t>", "std::u32string"}};
  clang::Lexer lexer({}, language, text.data(), text.data(), text.data() + text.size());
  std::string result;
  std::size_t position = 0;
  clang::Token token;
  for (;;) {
    lexer.LexFromRawLexer(token);
    if (token.is(clang::tok::eof)) break;
    if (token.isNot(clang::tok::raw_identifier) || token.getRawIdentifier() != "std")
      continue;
    const auto begin = token.getLocation().getRawEncoding();
    if (begin < position || (begin && text[begin - 1] == ':')) continue;
    const auto suffix = llvm::StringRef(text).substr(begin);
    for (const auto& alias : aliases) {
      if (!suffix.starts_with(alias.first)) continue;
      result.append(text, position, begin - position);
      result += alias.second.str();
      position = begin + alias.first.size();
      break;
    }
  }
  result.append(text, position);
  return result;
}
} // namespace

bool check_cpp_source(const std::vector<std::string> &arguments,
                      const std::vector<std::string> &history,
                      const std::string &source) {
  auto ci = compiler_for(arguments);
  ci->getDiagnostics().setSuppressAllDiagnostics(false);
  auto diagnostics =
      std::make_unique<CheckDiagnostics>(ci->getDiagnosticOpts());
  ci->getDiagnostics().setClient(diagnostics.release());
  // Syntax-only prevents the Interpreter constructor from running initializers
  // from forced includes. It still uses the real incremental parser/PTUs.
  ci->getFrontendOpts().ProgramAction = clang::frontend::ParseSyntaxOnly;
  bool missing = false;
  llvm::Error error = llvm::Error::success();
  auto interpreter =
      std::make_unique<CheckInterpreter>(std::move(ci), error, missing);
  if (error)
    throw std::runtime_error(llvm::toString(std::move(error)));
  for (const auto &cell : history) {
    auto parsed = interpreter->Parse(cell);
    if (!parsed)
      throw std::runtime_error(llvm::toString(parsed.takeError()));
  }
  missing = false;
  auto parsed = interpreter->Parse(source);
  if (!parsed)
    throw std::runtime_error(llvm::toString(parsed.takeError()));
  return missing;
}

void serve_cpp_checks(const std::vector<std::string>& arguments) {
  // This executable is a single-threaded, syntax-only compiler. Never fork the
  // live REPL/JIT, whose execution engine and user code can create threads.
  ::alarm(0); // Idle sessions have no deadline; the caller bounds each request.
  const auto read = [](std::size_t size) {
    std::string text(size, '\0');
    std::size_t offset = 0;
    while (offset < size) {
      const auto n = ::read(STDIN_FILENO, text.data() + offset, size - offset);
      if (n < 0 && errno == EINTR) {
        continue;
      }
      if (n <= 0) {
        throw std::runtime_error("truncated C++ checker request");
      }
      offset += static_cast<std::size_t>(n);
    }
    return text;
  };
  const auto write = [](const std::string& text) {
    std::size_t offset = 0;
    while (offset < text.size()) {
      const auto n = ::write(STDOUT_FILENO, text.data() + offset, text.size() - offset);
      if (n < 0 && errno == EINTR) {
        continue;
      }
      if (n <= 0) {
        throw std::runtime_error("cannot write C++ checker response");
      }
      offset += static_cast<std::size_t>(n);
    }
  };
  std::unique_ptr<CheckInterpreter> interpreter;
  bool missing = false;
  std::string replayed;
  for (;;) {
    std::string header;
    for (;;) {
      char ch;
      const auto n = ::read(STDIN_FILENO, &ch, 1);
      if (n < 0 && errno == EINTR) {
        continue;
      }
      if (n == 0 && header.empty()) {
        return;
      }
      if (n <= 0) {
        throw std::runtime_error("truncated C++ checker frame");
      }
      if (ch == '\n') {
        break;
      }
      if (header.size() >= 128) {
        throw std::runtime_error("invalid C++ checker frame");
      }
      header += ch;
    }
    constexpr std::size_t limit = 64U * 1024U * 1024U;
    std::size_t history_size = 0, source_size = 0;
    std::istringstream frame(header);
    std::string extra;
    if (!(frame >> history_size >> source_size) || (frame >> extra) || history_size > limit ||
        source_size > limit - history_size) {
      throw std::runtime_error("invalid C++ checker frame size");
    }
    ::alarm(15); // Bound replay even if the console dies during a request.
    const auto history = read(history_size);
    const auto source = read(source_size);
    if (!interpreter || history.compare(0, replayed.size(), replayed) != 0) {
      interpreter.reset();
      replayed.clear();
      auto ci = compiler_for(arguments);
      ci->getDiagnostics().setSuppressAllDiagnostics(false);
      auto diagnostics = std::make_unique<CheckDiagnostics>(ci->getDiagnosticOpts());
      ci->getDiagnostics().setClient(diagnostics.release());
      ci->getFrontendOpts().ProgramAction = clang::frontend::ParseSyntaxOnly;
      llvm::Error error = llvm::Error::success();
      interpreter = std::make_unique<CheckInterpreter>(std::move(ci), error, missing);
      if (error) {
        throw std::runtime_error(llvm::toString(std::move(error)));
      }
    }
    std::size_t cursor = replayed.size();
    while (cursor < history.size()) {
      const auto end = history.find('\n', cursor);
      if (end == std::string::npos) {
        throw std::runtime_error("invalid checker history");
      }
      const auto length_text = history.substr(cursor, end - cursor);
      std::size_t used = 0;
      const auto length = std::stoull(length_text, &used);
      cursor = end + 1;
      if (used != length_text.size() || length > history.size() - cursor) {
        throw std::runtime_error("invalid checker history length");
      }
      auto parsed = interpreter->Parse(history.substr(cursor, length));
      if (!parsed) {
        throw std::runtime_error(llvm::toString(parsed.takeError()));
      }
      cursor += length;
    }
    replayed = history;

    int descriptors[2];
    if (::pipe(descriptors) != 0) {
      throw std::runtime_error("cannot capture C++ diagnostics");
    }
    const auto child = ::fork();
    if (child < 0) {
      ::close(descriptors[0]);
      ::close(descriptors[1]);
      throw std::runtime_error("cannot fork C++ syntax check");
    }
    if (child == 0) {
      ::close(descriptors[0]);
      if (::dup2(descriptors[1], STDOUT_FILENO) < 0 || ::dup2(descriptors[1], STDERR_FILENO) < 0) {
        std::_Exit(1);
      }
      ::close(descriptors[1]);
      std::signal(SIGALRM, SIG_DFL);
      ::alarm(15);
      // Changes in this copy, including failed parses/macros, never enter the
      // retained compiler. Only the next caller-supplied history advances it.
      missing = false;
      auto parsed = interpreter->Parse(source);
      if (!parsed) {
        llvm::logAllUnhandledErrors(parsed.takeError(), llvm::errs(), "C++ check: ");
        llvm::errs().flush();
        std::_Exit(1);
      }
      std::_Exit(missing ? 2 : 0);
    }
    ::close(descriptors[1]);
    std::string diagnostics;
    bool too_large = false;
    for (;;) {
      std::array<char, 4096> buffer{};
      const auto n = ::read(descriptors[0], buffer.data(), buffer.size());
      if (n < 0 && errno == EINTR) {
        continue;
      }
      if (n <= 0) {
        break;
      }
      if (diagnostics.size() + static_cast<std::size_t>(n) <= 65536) {
        diagnostics.append(buffer.data(), static_cast<std::size_t>(n));
      } else {
        too_large = true;
      }
    }
    ::close(descriptors[0]);
    int status = 0;
    pid_t waited;
    do {
      waited = ::waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    int result = 1;
    if (waited == child && WIFEXITED(status)) {
      result = WEXITSTATUS(status);
    }
    if (result > 2 || waited != child || !WIFEXITED(status)) {
      result = 1;
      diagnostics += "C++ checker stopped unexpectedly; previous variables retained\n";
    }
    if (too_large || diagnostics.size() > 65536) {
      result = 1;
      diagnostics = "C++ checker output exceeded 64 KiB; previous variables retained\n";
    }
    write(std::to_string(result) + " " + std::to_string(diagnostics.size()) + "\n" + diagnostics);
    ::alarm(0);
  }
}

std::string reflect_cpp_header(const std::vector<std::string> &arguments,
    const std::vector<std::string> &history, const std::string &request) {
  auto ci = compiler_for(arguments);
  ci->getDiagnostics().setSuppressAllDiagnostics(false);
  ci->getDiagnostics().setClient(new CheckDiagnostics(ci->getDiagnosticOpts()));
  ci->getFrontendOpts().ProgramAction = clang::frontend::ParseSyntaxOnly;
  bool missing = false;
  llvm::Error error = llvm::Error::success();
  auto interpreter = std::make_unique<CheckInterpreter>(std::move(ci), error, missing);
  if (error) throw std::runtime_error(llvm::toString(std::move(error)));
  const auto parse = [&](const std::string &source) -> clang::PartialTranslationUnit & {
    auto parsed = interpreter->Parse(source);
    if (!parsed) throw std::runtime_error(llvm::toString(parsed.takeError()));
    return *parsed;
  };
  std::istringstream input(request);
  std::size_t count = 0;
  if (!(input >> count) || count == 0 || count > 16)
    throw std::runtime_error("provide 1..16 application headers");
  std::vector<std::string> headers;
  std::map<std::string, std::string> resolved;
  std::string include;
  for (std::size_t i = 0; i < count; ++i) {
    std::string header;
    if (!(input >> std::quoted(header)) || header.empty() || header.size() > 4095 ||
        header.find_first_of("\"\\\r\n") != std::string::npos ||
        header.find('\0') != std::string::npos)
      throw std::runtime_error("invalid application header name");
    headers.push_back(header);
    resolved.try_emplace(header);
    include += "#include \"" + header + "\"\n";
  }
  std::vector<std::string> types;
  for (std::string type; input >> std::quoted(type);) {
    if (type.empty() || type.size() > 4096 || types.size() >= 128)
      throw std::runtime_error("invalid record selector");
    types.push_back(type);
  }
  auto &compiler = *interpreter->getCompilerInstance();
  std::map<std::string, std::string> lookups;
  std::map<std::string, std::string> roots;
  bool reusable = true;
  compiler.getPreprocessor().addPPCallbacks(
      std::make_unique<HeaderInclude>(compiler, resolved, lookups, roots, reusable));
  for (const auto &cell : history) parse(cell);
  parse(include);
  std::string resolved_includes;
  for (const auto &header : headers) {
    const auto &path = resolved.at(header);
    if (path.empty() || path.find_first_of("\"\\\r\n") != std::string::npos)
      throw std::runtime_error("cannot resolve reusable application include: " + header);
    resolved_includes += "#include \"" + path + "\"\n";
  }
  unsigned index = 0;
  const auto viable = [&](const std::string &expression) {
    const auto symbol = "__tracer_mapping_trait_" + std::to_string(index++);
    const auto &unit = parse("[[maybe_unused]] constexpr bool " + symbol + "=(" + expression + ");");
    for (const auto *decl : unit.TUPart->decls()) {
      if (const auto *value = llvm::dyn_cast<clang::VarDecl>(decl)) {
        if (value->getName() != symbol || !value->hasInit()) continue;
        bool result = false;
        if (value->getInit()->EvaluateAsBooleanCondition(result, compiler.getASTContext()))
          return result;
      }
    }
    throw std::runtime_error("could not check record construction traits");
  };
  auto generated = generate_record_mappings(compiler, types, viable);
  parse(generated.source);
  std::string validation;
  for (const auto &type : generated.types)
    validation += "[[maybe_unused]] auto __tracer_mapping_decode_" + std::to_string(index++) +
        "=&tracer_repl::EventCodec<::" + type + ">::decode;\n";
  if (!validation.empty()) {
    parse(validation);
    compiler.getSema().PerformPendingInstantiations();
  }
  // Only the header and mappings enter the live interpreter; no checker-only
  // trait variables or decoder validation cells are executed or retained.
  std::ostringstream manifest;
  if (!reusable) manifest << "// tracer-repl-no-reuse\n";
  for (const auto &[path, state] : roots) {
    if (path.find_first_of("\r\n") != std::string::npos || state.find_first_of("\r\n") != std::string::npos)
      throw std::runtime_error("unsupported newline in include root");
    manifest << "// tracer-repl-search-root " << std::quoted(path) << ' ' << std::quoted(state) << '\n';
  }
  for (const auto &[path, actual] : lookups) {
    if (path.find_first_of("\r\n") != std::string::npos || actual.find_first_of("\r\n") != std::string::npos)
      throw std::runtime_error("unsupported newline in include search filename");
    manifest << "// tracer-repl-lookup " << std::quoted(path) << ' ' << std::quoted(actual) << '\n';
  }
  std::set<std::string> files;
  const auto &sources = compiler.getSourceManager();
  for (auto it = sources.fileinfo_begin(); it != sources.fileinfo_end(); ++it) {
    const auto *cache = it->second;
    if (!cache->OrigEntry) continue;
    const auto path = absolute_input(compiler, cache->OrigEntry->getName());
    const auto basename = std::filesystem::path(path).filename().string();
    if (!std::filesystem::is_regular_file(path) &&
        (basename.rfind("input_line_", 0) == 0 || (!basename.empty() && basename.front() == '<')))
      continue; // Clang's incremental cells are virtual FileEntries.
    if (path.find_first_of("\r\n") != std::string::npos)
      throw std::runtime_error("unsupported newline in dependency filename");
    if (!files.insert(path).second) continue;
    const auto digest = mapping_file_digest(path);
    if (const auto buffer = cache->getBufferIfLoaded(); buffer &&
        mapping_digest(buffer->getBuffer().str()) != digest)
      throw std::runtime_error("header changed during AST analysis: " + path);
    manifest << "// tracer-repl-dependency " << digest << ' ' << std::quoted(path) << '\n';
  }
  return generated.source.substr(0, generated.source.find('\n') + 1) +
         "#pragma once\n" + manifest.str() + resolved_includes +
         generated.source.substr(generated.source.find('\n') + 1);
}

void validate_replay_types(const std::vector<std::string> &arguments,
                           const std::string &prelude, const std::string &source) {
  if (source.find("_Pragma") != std::string::npos ||
      source.find("__pragma") != std::string::npos ||
      source.find("??") != std::string::npos)
    throw std::runtime_error("preprocessor directives are not replay declarations");
  auto ci = compiler_for(arguments);
  ci->getDiagnostics().setSuppressAllDiagnostics(false);
  ci->getDiagnostics().setClient(new CheckDiagnostics(ci->getDiagnosticOpts()));
  ci->getFrontendOpts().ProgramAction = clang::frontend::ParseSyntaxOnly;
  bool missing = false;
  llvm::Error error = llvm::Error::success();
  auto interpreter = std::make_unique<CheckInterpreter>(std::move(ci), error, missing);
  if (error)
    throw std::runtime_error(llvm::toString(std::move(error)));
  const auto parse = [&](const std::string &text) -> clang::PartialTranslationUnit & {
    auto unit = interpreter->Parse(text);
    if (!unit)
      throw std::runtime_error(llvm::toString(unit.takeError()));
    return *unit;
  };
  if (!prelude.empty())
    parse(prelude);
  const auto &unit = parse(source);
  const auto validate = [&](const auto &self, const clang::Decl &decl) -> void {
    if (decl.isImplicit())
      return;
    const auto reject = [&] {
      throw std::runtime_error(std::string("unsupported replay declaration: ") +
                               decl.getDeclKindName());
    };
    for (const auto *scope = decl.getDeclContext(); scope; scope = scope->getParent()) {
      if (const auto *ns = llvm::dyn_cast<clang::NamespaceDecl>(scope);
          ns && (ns->isStdNamespace() ||
                 (ns->getParent()->isTranslationUnit() && ns->getName() == "tracer_repl")))
        reject();
    }
    for (const auto *attr : decl.attrs()) {
      if (attr->isImplicit() ||
          llvm::isa<clang::AlignedAttr, clang::PackedAttr, clang::FinalAttr,
                    clang::NoUniqueAddressAttr>(attr))
        continue;
      if (const auto *label = llvm::dyn_cast<clang::AsmLabelAttr>(attr);
          label && llvm::isa<clang::FunctionDecl>(decl) &&
          label->getLabel().starts_with("_Z") &&
          label->getLabel().find_first_not_of("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_") ==
              llvm::StringRef::npos)
        continue;
      reject();
    }
    if (const auto *function = llvm::dyn_cast<clang::FunctionDecl>(&decl)) {
      if ((function->doesThisDeclarationHaveABody() &&
           !function->isExplicitlyDefaulted() && !function->isDeletedAsWritten()) ||
          llvm::any_of(function->parameters(), [](const auto *parameter) {
            return parameter->hasDefaultArg() || parameter->hasAttrs();
          }))
        reject();
    } else if (const auto *field = llvm::dyn_cast<clang::FieldDecl>(&decl)) {
      if (field->hasInClassInitializer())
        reject();
    } else if (const auto *primary = llvm::dyn_cast<clang::ClassTemplateDecl>(&decl)) {
      if (primary->getTemplatedDecl()->isCompleteDefinition())
        reject();
      self(self, *primary->getTemplatedDecl());
    } else if (llvm::isa<clang::NamespaceDecl, clang::LinkageSpecDecl,
                         clang::CXXRecordDecl, clang::EnumDecl>(decl)) {
      const auto *scope = clang::Decl::castToDeclContext(&decl);
      for (const auto *member : scope->decls())
        self(self, *member);
    } else if (!llvm::isa<clang::EnumConstantDecl, clang::TypedefNameDecl,
                          clang::AccessSpecDecl, clang::EmptyDecl>(decl)) {
      reject();
    }
  };
  for (const auto *decl : unit.TUPart->decls())
    validate(validate, *decl);
}

bool incomplete_cpp(const std::string &source) {
  const auto first = source.find_first_not_of(" \t\r\n");
  if (first != std::string::npos && source[first] == '#')
    return !source.empty() && source.back() == '\\';
  clang::LangOptions language;
  language.CPlusPlus = language.CPlusPlus11 = language.CPlusPlus14 =
      language.CPlusPlus17 = true;
  language.LineComment = true;
  language.Digraphs = true;
  clang::Lexer lexer({}, language, source.data(), source.data(),
                     source.data() + source.size());
  lexer.SetCommentRetentionState(true);
  std::vector<clang::tok::TokenKind> brackets;
  clang::Token token;
  do {
    const auto *before = lexer.getBufferLocation();
    lexer.LexFromRawLexer(token);
    const auto kind = token.getKind();
    if (kind == clang::tok::eof) {
      // The raw lexer discards an unterminated block comment at EOF.
      const std::string remaining(before, source.data() + source.size());
      const auto start = remaining.find_first_not_of(" \t\r\n");
      if (start != std::string::npos &&
          remaining.compare(start, 2, "/*") == 0 &&
          remaining.find("*/", start + 2) == std::string::npos)
        return true;
    }
    if (kind == clang::tok::l_paren || kind == clang::tok::l_brace ||
        kind == clang::tok::l_square) {
      brackets.push_back(kind);
    } else if (kind == clang::tok::r_paren || kind == clang::tok::r_brace ||
               kind == clang::tok::r_square) {
      if (brackets.empty())
        return false; // Let the checker report a mismatch.
      const auto opening = brackets.back();
      brackets.pop_back();
      if (!((opening == clang::tok::l_paren && kind == clang::tok::r_paren) ||
            (opening == clang::tok::l_brace && kind == clang::tok::r_brace) ||
            (opening == clang::tok::l_square && kind == clang::tok::r_square)))
        return false;
    } else if (kind == clang::tok::unknown || kind == clang::tok::comment) {
      const auto text = source.substr(token.getLocation().getRawEncoding(),
                                      token.getLength());
      if ((text.rfind("/*", 0) == 0 &&
           (text.size() < 4 || text.substr(text.size() - 2) != "*/")) ||
          (kind == clang::tok::unknown &&
           text.find_first_of("\"'") != std::string::npos))
        return true;
    }
  } while (token.isNot(clang::tok::eof));
  return !brackets.empty();
}

std::optional<CppCompletions> complete_cpp_directive(std::string_view source) {
  const auto newline = source.rfind('\n');
  auto line = newline == source.npos ? source : source.substr(newline + 1);
  const auto first = line.find_first_not_of(" \t");
  if (first == line.npos || line[first] != '#') return {};
  line.remove_prefix(first + 1);
  const auto start = line.find_first_not_of(" \t");
  line.remove_prefix(start == line.npos ? line.size() : start);
  if (line.find_first_not_of("abcdefghijklmnopqrstuvwxyz_") != line.npos)
    return {};

  CppCompletions result;
  result.prefix = line;
  for (const std::string_view directive : {
           "define", "elif", "elifdef", "elifndef", "else", "endif", "error",
           "if", "ifdef", "ifndef", "include", "include_next", "line",
           "pragma", "undef", "warning"}) {
    if (llvm::StringRef(directive).starts_with(line)) result.items.emplace_back(directive);
  }
  return result;
}

CppCompletions complete_cpp(const std::vector<std::string> &arguments,
                            const std::string &history,
                            const std::string &source) {
  if (source.size() > 65536)
    return {};
  const std::string filename = "input_line_[TracerCompletion]";
  const auto content = history + "\n#line 1\n" + source;
  // Keep remapped buffers alive until both compiler/source managers are gone.
  auto buffer = llvm::MemoryBuffer::getMemBufferCopy(content, filename);
  std::vector<std::unique_ptr<const llvm::MemoryBuffer>> owned_buffers;
  auto ci = compiler_for(arguments);
  auto diagnostics = ci->getDiagnosticsPtr();
  std::unique_ptr<clang::ASTUnit> unit(
      clang::ASTUnit::LoadFromCompilerInvocationAction(
          ci->getInvocationPtr(),
          std::make_shared<clang::PCHContainerOperations>(), nullptr,
          diagnostics));
  if (!unit)
    throw std::runtime_error("cannot initialize C++ completion compiler");
  ci->getFrontendOpts().Inputs[0] = clang::FrontendInputFile(
      filename, clang::Language::CXX, clang::InputKind::Source);
  llvm::SmallVector<clang::ASTUnit::RemappedFile, 1> remapped{
      {filename, buffer.get()}};
  llvm::SmallVector<clang::StoredDiagnostic, 8> stored;
  llvm::SmallVector<const llvm::MemoryBuffer *, 1> temporary;
  CppCompletions results;
  Consumer consumer(results);
  unit->setOwnsRemappedFileBuffers(false);
  const auto newline = content.find_last_of('\n');
  const auto column = newline == std::string::npos ? content.size() + 1
                                                   : content.size() - newline;
  const auto line = static_cast<unsigned>(
      std::count(content.begin(), content.end(), '\n') + 1);
  unit->CodeComplete(
      filename, line, static_cast<unsigned>(column), remapped, false, false,
      false, consumer, std::make_shared<clang::PCHContainerOperations>(),
#if LLVM_VERSION_MAJOR >= 22
      diagnostics, ci->getLangOpts(), unit->getSourceManagerPtr(),
      unit->getFileManagerPtr(),
#else
      *diagnostics, ci->getLangOpts(), unit->getSourceManager(),
      unit->getFileManager(),
#endif
      stored, temporary, std::make_unique<clang::SyntaxOnlyAction>());
  std::set<const llvm::MemoryBuffer *> seen{buffer.get()};
  for (const auto *extra : temporary)
    if (seen.insert(extra).second)
      owned_buffers.emplace_back(extra);
  std::sort(results.items.begin(), results.items.end());
  results.items.erase(std::unique(results.items.begin(), results.items.end()),
                      results.items.end());
  return results;
}
std::string describe_cpp_symbol(clang::CompilerInstance& compiler,
                                const std::string& symbol) {
  constexpr auto usage = "%type expects a C++ symbol such as name, ns::name, or object.member";
  auto& sema = compiler.getSema();
  clang::DeclContext* context = compiler.getASTContext().getTranslationUnitDecl();
  clang::Lexer lexer({}, compiler.getLangOpts(), symbol.data(), symbol.data(),
                     symbol.data() + symbol.size());
  clang::Token token;
  lexer.LexFromRawLexer(token);
  bool unqualified = token.isNot(clang::tok::coloncolon);
  if (!unqualified) lexer.LexFromRawLexer(token);
  auto policy = readable_policy(compiler);
  policy.TerseOutput = true;
  policy.PolishForDeclaration = true;
  policy.FullyQualifiedName = true;

  for (;;) {
    if (token.isNot(clang::tok::raw_identifier))
      throw std::invalid_argument(usage);
    const auto name = token.getRawIdentifier();
    lexer.LexFromRawLexer(token);
    const auto separator = token.getKind();
    if (separator != clang::tok::eof && separator != clang::tok::coloncolon &&
        separator != clang::tok::period && separator != clang::tok::arrow)
      throw std::invalid_argument(usage);
    auto& id = compiler.getPreprocessor().getIdentifierTable().get(name);
    clang::LookupResult lookup(sema, &id, {}, clang::Sema::LookupOrdinaryName);
    lookup.suppressDiagnostics();
    if (unqualified && sema.TUScope)
      sema.LookupName(lookup, sema.TUScope);
    else
      sema.LookupQualifiedName(lookup, context);
    if (lookup.empty())
      throw std::invalid_argument("unknown C++ symbol: " + symbol);
    if (lookup.isAmbiguous())
      throw std::invalid_argument("ambiguous C++ symbol: " + symbol);
    if (separator == clang::tok::eof) {
      std::string output;
      llvm::raw_string_ostream out(output);
      std::set<const clang::Decl*> seen;
      for (auto* candidate : lookup) {
        const auto* declaration = candidate->getUnderlyingDecl();
        if (!seen.insert(declaration->getCanonicalDecl()).second) continue;
        if (const auto* value = llvm::dyn_cast<clang::ValueDecl>(declaration);
            value && !llvm::isa<clang::FunctionDecl>(value)) {
          auto type = value->getType();
          if (const auto* deduced = llvm::dyn_cast<clang::DecltypeType>(type.getTypePtr()))
            type = compiler.getASTContext().getQualifiedType(deduced->getUnderlyingType(),
                                                              type.getLocalQualifiers());
          type.print(out, policy, value->getNameAsString());
        } else if (declaration->getDeclContext()->isStdNamespace() &&
                   llvm::isa<clang::TypeDecl, clang::ClassTemplateDecl,
                             clang::TypeAliasTemplateDecl>(declaration)) {
          declaration->printQualifiedName(out, policy);
        } else {
          declaration->print(out, policy);
        }
        if (!output.empty() && output.back() != ';') out << ';';
        out << '\n';
      }
      return preferred_string_names(std::move(output), compiler.getLangOpts());
    }
    if (!lookup.isSingleResult() ||
        !(context = symbol_scope(lookup.getFoundDecl()->getUnderlyingDecl(), separator)))
      throw std::invalid_argument("C++ symbol has no member scope: " + name.str());
    unqualified = false;
    lexer.LexFromRawLexer(token);
  }
}

std::string describe_cpp_api(clang::CompilerInstance &parent,
                             const std::string &requested) {
  const auto topic = requested == "tracer" ? "Facade" : requested;
  const bool all = topic == "api";
  const std::set<std::string> types = {"Facade",     "Event",         "EventValue",
                                       "Collection", "SessionHandle", "EventRange"};
  if (!all && !types.count(topic) && topic != "ScalarValue")
    throw std::invalid_argument("API topics: api, tracer, Event, EventValue, Collection, "
                                "SessionHandle, ScalarValue, EventRange");
  std::string text;
  llvm::raw_string_ostream out(text);
  const auto policy = readable_policy(parent);
  std::set<const clang::Decl *> seen;
  for (auto *tu = parent.getASTContext().getTranslationUnitDecl(); tu;
       tu = tu->getPreviousDecl()) {
    for (auto *decl : tu->decls()) {
      const auto *ns = llvm::dyn_cast<clang::NamespaceDecl>(decl);
      if (!ns || ns->getName() != "tracer_repl")
        continue;
      for (auto *member : ns->decls()) {
        if (const auto *alias = llvm::dyn_cast<clang::TypeAliasDecl>(member)) {
          if (alias->getName() == "ScalarValue" &&
              (all || topic == "ScalarValue") &&
              seen.insert(alias->getCanonicalDecl()).second)
            out << "using ScalarValue = "
                << alias->getUnderlyingType().getAsString(policy) << ";\n";
        }
        const auto *record = llvm::dyn_cast<clang::CXXRecordDecl>(member);
        if (!record || !types.count(record->getNameAsString()) ||
            (!all && topic != record->getNameAsString()))
          continue;
        record = record->getDefinition();
        if (!record || !seen.insert(record->getCanonicalDecl()).second)
          continue;
        out << "\n" << record->getName() << " {\n";
        for (const auto *item : record->decls()) {
          if (item->isImplicit() || item->getAccess() != clang::AS_public)
            continue;
          if (const auto *field = llvm::dyn_cast<clang::FieldDecl>(item))
            out << "  " << field->getType().getAsString(policy) << " "
                << field->getName() << ";\n";
          if (const auto *function =
                  llvm::dyn_cast<clang::CXXMethodDecl>(item)) {
            out << "  ";
            print_function(out, *function, policy);
          }
          if (const auto *function =
                  llvm::dyn_cast<clang::FunctionTemplateDecl>(item)) {
            out << "  template <";
            bool first = true;
            for (const auto *parameter : *function->getTemplateParameters()) {
              if (!first)
                out << ", ";
              first = false;
              parameter->print(out, policy);
            }
            out << "> ";
            print_function(out, *function->getTemplatedDecl(), policy);
          }
        }
        out << "};\n";
      }
    }
  }
  if (seen.empty())
    throw std::runtime_error(
        "API declarations are unavailable in this compiler session");
  return text;
}
} // namespace tracer_repl

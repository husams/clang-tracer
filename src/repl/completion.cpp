// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// ASTUnit setup follows LLVM's clang/lib/Interpreter/CodeCompletion.cpp.
// See LICENSES/LLVM.txt. Session input is parsed for types, never executed.
#include "completion.hpp"

#include <algorithm>
#include <cstdlib>
#include <set>
#include <stdexcept>

#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Expr.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Interpreter/Interpreter.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Sema/CodeCompleteConsumer.h>
#include <clang/Sema/Sema.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#if LLVM_VERSION_MAJOR >= 22
#include <clang/Interpreter/IncrementalExecutor.h>
#endif

namespace tracer_repl {
namespace {
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
    for (unsigned i = 0; i < count; ++i) {
      const auto &result = results[i];
      std::string name;
      if (result.Kind == clang::CodeCompletionResult::RK_Declaration &&
          !result.Hidden && result.Declaration->getIdentifier() &&
          result.Declaration->getAccess() != clang::AS_private &&
          result.Declaration->getAccess() != clang::AS_protected)
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
  clang::IncrementalCompilerBuilder builder;
  builder.SetCompilerArgs(argv);
  auto instance = builder.CreateCpp();
  if (!instance)
    throw std::runtime_error(llvm::toString(instance.takeError()));
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
std::string describe_cpp_api(clang::CompilerInstance &parent,
                             const std::string &requested) {
  const auto topic = requested == "tracer" ? "Facade" : requested;
  const bool all = topic == "api";
  const std::set<std::string> types = {"Facade", "Event", "Collection",
                                       "SessionHandle"};
  if (!all && !types.count(topic) && topic != "ScalarValue")
    throw std::invalid_argument("API topics: api, tracer, Event, Collection, "
                                "SessionHandle, ScalarValue");
  std::string text;
  llvm::raw_string_ostream out(text);
  clang::PrintingPolicy policy(parent.getLangOpts());
  policy.SuppressTagKeyword = true;
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

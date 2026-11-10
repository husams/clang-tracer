#include "interpreter.hpp"
#include "check_process.hpp"
#include "completion.hpp"
#include "mapping_cache.hpp"
#include "../prepared_contract.hpp"
#include "../prepared_header_search.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Interpreter/Interpreter.h>
#include <clang/Interpreter/PartialTranslationUnit.h>
#include <clang/Interpreter/Value.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/VirtualFileSystem.h>

#include "repl_config.hpp"
#include "server_process.hpp"
#if LLVM_VERSION_MAJOR >= 22
#include <clang/Interpreter/IncrementalExecutor.h>
#endif
#include <cstdlib>
#include <filesystem>
#include <set>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace tracer_repl {
namespace {
std::vector<std::string>
profile_arguments(const std::vector<std::string> &user) {
  const auto directory = std::filesystem::path(executable_path()).parent_path();
  auto resource = directory / TRACER_REPL_RESOURCE_RELATIVE;
  if (const auto *override_path =
          std::getenv("CLANG_TRACER_CLANG_RESOURCE_DIR");
      override_path && *override_path) {
    resource = override_path;
    if (!resource.is_absolute() ||
        !std::filesystem::is_regular_file(resource / "include/stddef.h"))
      throw std::runtime_error("CLANG_TRACER_CLANG_RESOURCE_DIR must name an "
                               "absolute Clang resource directory");
  } else if (!std::filesystem::is_regular_file(resource / "include/stddef.h")) {
    resource = TRACER_REPL_RESOURCE_DIR;
  }
  if (!std::filesystem::is_regular_file(resource / "include/stddef.h"))
    throw std::runtime_error("Clang builtin headers are unavailable");
  std::vector<std::string> result{"-std=c++17", "-fPIC", "-O0", "-resource-dir",
                                  resource.string()};
  for (const auto *argument : repl_compiler_arguments)
    result.emplace_back(argument);
  result.insert(result.end(), user.begin(), user.end());
  // Use the same access rules for entered code, mapping generation, and the
  // syntax checker, including when a compilation database enables checking.
  result.emplace_back("-fno-access-control");
  return result;
}
std::string api_header() {
  auto header = std::filesystem::path(executable_path()).parent_path() /
                TRACER_REPL_API_RELATIVE;
  if (!std::filesystem::is_regular_file(header))
    header = TRACER_REPL_API_HEADER;
  const auto path = header.string();
  if (path.find_first_of("\"\\\r\n") != std::string::npos)
    throw std::runtime_error("unsupported character in facade header path");
  return path;
}

bool is_single_initializer(const clang::TranslationUnitDecl &unit,
                           const std::string &source) {
  // Clang Undo retracts the whole input and does not undo preprocessor
  // directives. Limit this recovery to a single new variable definition.
  if (source.find('#') != std::string::npos)
    return false;
  const clang::VarDecl *variable = nullptr;
  for (const auto *decl : unit.decls()) {
    if (llvm::isa<clang::EmptyDecl>(decl))
      continue;
    const auto *candidate = llvm::dyn_cast<clang::VarDecl>(decl);
    if (variable || !candidate || !candidate->getIdentifier() ||
        candidate->getPreviousDecl() || !candidate->hasInit())
      return false;
    variable = candidate;
  }
  return variable != nullptr;
}

bool can_unload_initializer(const llvm::Module &module) {
  // Undo unmaps the module, but ORC keeps registered destructors until the
  // interpreter shuts down. A function-local static can register one even
  // when the top-level initializer eventually throws.
  if (module.getNamedGlobal("llvm.global_dtors"))
    return false;
  for (const auto *name : {"atexit", "__cxa_thread_atexit",
                           "__cxa_thread_atexit_impl", "_tlregdtor"})
    if (const auto *function = module.getFunction(name);
        function && !function->use_empty())
      return false;
  const auto *registration = module.getFunction("__cxa_atexit");
  if (!registration || registration->use_empty())
    return true;
  if (!registration->hasOneUse())
    return false;
  const auto *call = llvm::dyn_cast<llvm::CallInst>(*registration->user_begin());
  // A single registration at the end of the global initializer is reached
  // only once that variable and its initializer temporaries have succeeded.
  return call && call->getFunction()->getName().starts_with("__cxx_global_var_init") &&
         llvm::isa_and_nonnull<llvm::ReturnInst>(call->getNextNode());
}
} // namespace
struct CppEngine::Impl {
  CppCheckSession checker;
  std::unique_ptr<clang::Interpreter> interpreter;
  std::vector<std::string> arguments;
  std::string history;
  std::string check_history;
  const std::filesystem::path directory = std::filesystem::current_path();
  const std::filesystem::path facade_directory;
  using Stamp = std::pair<std::uintmax_t, std::filesystem::file_time_type>;
  struct HeaderInput {
    Stamp stamp;
    std::string facade_digest;
  };
  std::map<std::filesystem::path, HeaderInput> headers;
  std::map<std::pair<std::string, std::filesystem::path>, std::string> mappings;
  std::set<std::string> imported_adapters;
  bool synchronized = true;
  bool user_cells = false;
  explicit Impl(const std::filesystem::path& facade)
      : facade_directory(std::filesystem::weakly_canonical(facade).parent_path()) {}
  void verify_inputs() {
    if (std::filesystem::current_path() != directory)
      throw std::runtime_error("C++ working directory changed; restore it or "
                               "use %cpp reset; previous variables retained");
    for (auto &[path, input] : headers) {
      std::error_code error;
      const auto size = std::filesystem::file_size(path, error);
      const auto time = std::filesystem::last_write_time(path, error);
      const Stamp current{size, time};
      if (!error && current == input.stamp) continue;
      // Reinstalling the same facade can change its timestamp.
      if (!error && size == input.stamp.first && !input.facade_digest.empty() &&
          mapping_file_digest(path) == input.facade_digest) {
        input.stamp = current;
        continue;
      }
      throw std::runtime_error(
          "C++ header changed: " + path.string() +
          "; restore it or use %cpp reset; previous variables retained");
    }
  }
  void remember(const std::string &cell) {
    check_history += std::to_string(cell.size()) + "\n" + cell;
    if (cell.empty())
      return;
    const auto &sources =
        interpreter->getCompilerInstance()->getSourceManager();
    for (auto it = sources.fileinfo_begin(); it != sources.fileinfo_end();
         ++it) {
      const auto *cache = it->second;
      if (!cache->OrigEntry)
        continue;
      llvm::SmallString<256> filename(cache->OrigEntry->getName());
      if (interpreter->getCompilerInstance()->getFileManager()
              .getVirtualFileSystem().makeAbsolute(filename))
        throw std::runtime_error("cannot resolve C++ input header");
      const auto path = std::filesystem::weakly_canonical(filename.str().str());
      if (headers.count(path))
        continue;
      std::error_code error;
      const auto size = std::filesystem::file_size(path, error);
      const auto time = std::filesystem::last_write_time(path, error);
      if (!error)
        headers.emplace(path, HeaderInput{{size, time},
            path.parent_path() == facade_directory ? mapping_file_digest(path) : ""});
    }
  }
};

CppEngine::CppEngine(Facade &facade, std::vector<std::string> arguments)
    : facade_(facade), arguments_(std::move(arguments)) {}

CppEngine::~CppEngine() = default;

void CppEngine::reset() { impl_.reset(); }

std::vector<std::string> CppEngine::effective_compiler_options() const {
  return profile_arguments(arguments_);
}

void CppEngine::require_compiler_options_mutable() const {
  if (impl_ && impl_->user_cells)
    throw std::runtime_error(
        "compiler options are locked after C++ input; use --cpp-arg at startup "
        "or %cpp reset before changing them; variables retained");
}

void CppEngine::set_compiler_options(std::vector<std::string> arguments) {
  require_compiler_options_mutable();
  std::size_t bytes = 0;
  for (const auto &argument : arguments) {
    bytes += argument.size();
    if (argument.empty() || argument.find('\0') != std::string::npos ||
        bytes > 65536 || arguments.size() > 128)
      throw std::invalid_argument("compiler options must be nonempty and total "
                                  "at most 128 arguments / 64 KiB");
  }
  const auto helper = std::filesystem::path(executable_path()).parent_path() /
                      "clang-tracer-repl-check";
  const auto checked =
      run_cpp_analysis(helper.string(), "check", profile_arguments(arguments),
                       "", "#include \"" + api_header() + "\"\n");
  if (checked.status != 0)
    throw std::runtime_error(
        "compiler options rejected; previous options retained:\n" +
        checked.output);
  impl_.reset();
  arguments_ = std::move(arguments);
}

CppEngine::MappingHeader CppEngine::mapping_header(const std::vector<std::string> &headers,
    const std::vector<std::string> &types, const std::string &output,
    bool standalone) {
  if (impl_) impl_->verify_inputs();
  const bool independent = standalone || !impl_ || !impl_->user_cells;
  const auto arguments = profile_arguments(arguments_);
  const auto api = api_header();
  const auto helper = std::filesystem::path(executable_path()).parent_path() /
                      "clang-tracer-repl-check";
  std::ostringstream identity;
  identity << (headers.size() == 1 ? "mapping-cache-v1\n" : "mapping-cache-v2\n");
  for (const auto &header : headers) identity << std::quoted(header) << '\n';
  identity << std::quoted(std::filesystem::current_path().string()) << '\n'
           << std::quoted(api) << '\n' << mapping_file_digest(helper) << '\n';
  for (const auto &argument : arguments) identity << std::quoted(argument) << '\n';
  identity << "types\n";
  for (const auto &type : types) identity << std::quoted(type) << '\n';
  for (const auto *variable : {"CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH",
       "OBJC_INCLUDE_PATH", "SDKROOT", "MACOSX_DEPLOYMENT_TARGET",
       "SOURCE_DATE_EPOCH", "CCC_OVERRIDE_OPTIONS"}) {
    const auto *value = std::getenv(variable);
    identity << variable << '=' << std::quoted(value ? value : "") << '\n';
  }
  const auto request_key = mapping_digest(identity.str());
  auto key = request_key;
  auto path = output.empty() ? mapping_cache_directory() / (key + ".hpp")
                            : std::filesystem::absolute(output);
  if (path.string().find_first_of("\"\\\r\n") != std::string::npos)
    throw std::runtime_error("unsupported character in generated output path");
  const bool simple_profile = std::none_of(arguments.begin(), arguments.end(), [](const auto &argument) {
    for (const auto *prefix : {"-include", "-imacros", "-fmodule", "-ivfsoverlay", "--config"})
      if (argument.rfind(prefix, 0) == 0) return true;
    return false;
  });
  if (!standalone && impl_ && simple_profile) {
    for (const auto &[request, stored_key] : impl_->mappings) {
      if (request.first != request_key ||
          (!output.empty() && request.second != std::filesystem::weakly_canonical(path))) continue;
      if (valid_mapping_header(request.second, stored_key))
        return {request.second.string(), request_key, stored_key, true};
    }
  }
  const bool already_included = impl_ && impl_->headers.count(std::filesystem::weakly_canonical(path));
  if ((independent || already_included) && simple_profile && valid_mapping_header(path, key)) {
    return {path.string(), request_key, key, true};
  }
  const auto prelude = "#include \"" + api + "\"\n";
  const auto history = independent ? std::to_string(prelude.size()) + "\n" + prelude
                                   : impl_->check_history;
  std::ostringstream request;
  request << headers.size() << '\n';
  for (const auto &header : headers) request << std::quoted(header) << '\n';
  for (const auto &type : types)
    request << std::quoted(type) << '\n';
  const auto generated =
      run_cpp_analysis(helper.string(), "reflect", arguments, history, request.str());
  if (generated.status != 0)
    throw std::runtime_error(
        "automatic mapping generation failed; previous C++ state retained. "
        "For bulk imports, select supported roots with --type QualifiedType:\n" +
        generated.output);
  if (impl_) impl_->verify_inputs();
  const auto first_line = generated.output.find('\n') + 1;
  const auto body = generated.output.substr(0, first_line) + prelude +
                    generated.output.substr(first_line);
  if (!independent) {
    key = mapping_digest(key + body);
    if (output.empty()) path = mapping_cache_directory() / (key + ".hpp");
  }
  save_mapping_header(path, key, body);
  return {path.string(), request_key, key, false};
}

std::string CppEngine::generate_header(const std::string &header,
    const std::vector<std::string> &types, const std::string &output) {
  const auto saved = mapping_header({header}, types, output, true);
  return mapping_header_summary(saved.path) + "\n" +
         (saved.reused ? "reused generated header: " : "saved generated header: ") + saved.path;
}

std::string CppEngine::import_header(const std::string &header,
    const std::vector<std::string> &types, const std::string &output) {
  return import_headers({header}, types, output);
}

std::string CppEngine::import_headers(const std::vector<std::string> &headers,
    const std::vector<std::string> &types, const std::string &output) {
  const auto saved = mapping_header(headers, types, output, false);
  if (!evaluate("#include \"" + saved.path + "\"\n"))
    throw std::runtime_error("application header import failed");
  impl_->mappings[{saved.request_key, std::filesystem::weakly_canonical(saved.path)}] = saved.stored_key;
  return mapping_header_summary(saved.path) + "\n" +
         (saved.reused ? "reused generated header: " : "saved generated header: ") + saved.path;
}

std::string CppEngine::import_prepared_adapter(const std::string &source) {
  const auto key = mapping_digest(source);
  if (impl_ && impl_->imported_adapters.count(key))
    return {};
  const auto path = mapping_cache_directory() / ("prepared-" + key + ".hpp");
  save_mapping_header(path, key, "#pragma once\n" + source);
  auto imported = import_headers({path.string()});
  impl_->imported_adapters.insert(key);
  return imported;
}

std::string CppEngine::import_recorded_types(const clangtracer::v1::PreparedInjector &types) {
  if (!tracer::prepared::is_typed_version(types.format_version()) ||
      !types.adapter_toolchain_options().empty() || types.adapter_uses_default_toolchain() ||
      types.ByteSizeLong() > tracer::prepared::max_replay_type_bytes)
    throw std::invalid_argument("unsupported recorded type bundle");
  const auto source = tracer::prepared::adapter_source(types);
  if (impl_ && impl_->imported_adapters.count(mapping_digest(source)))
    return {};
  if (!types.adapter_standard_headers().empty()) {
    auto unit = clang::tooling::buildASTFromCodeWithArgs("", profile_arguments({}),
                                                       "recorded-toolchain.cc");
    if (!unit || unit->getDiagnostics().hasErrorOccurred())
      throw std::runtime_error("cannot resolve the local toolchain for recorded types");
    tracer::prepared::check_toolchain_headers(unit->getPreprocessor().getHeaderSearchInfo(), types);
  }
  const auto &declarations = types.adapter_declarations();
  const auto helper = std::filesystem::path(executable_path()).parent_path() /
                      "clang-tracer-repl-check";
  const auto checked = run_cpp_analysis(
      helper.string(), "replay-types", profile_arguments(arguments_),
      source.substr(0, source.size() - declarations.size()), declarations);
  if (checked.status != 0)
    throw std::runtime_error("recorded adapter must contain declarations only\n" +
                             checked.output);
  return import_prepared_adapter(source);
}

void CppEngine::initialize() {
  if (impl_) {
    return;
  }
  static std::once_flag initialized;
  std::call_once(initialized, [] {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
  });
  auto args = profile_arguments(arguments_);
  std::vector<const char *> argv;
  for (const auto &arg : args) {
    argv.push_back(arg.c_str());
  }
  clang::IncrementalCompilerBuilder compiler;
  compiler.SetCompilerArgs(argv);
  auto instance = compiler.CreateCpp();
  if (!instance) {
    throw std::runtime_error(llvm::toString(instance.takeError()));
  }
  auto builder = std::make_unique<llvm::orc::LLJITBuilder>();
  auto jit = std::make_shared<llvm::orc::LLJIT *>(nullptr);
  builder->setNotifyCreatedCallback([jit](llvm::orc::LLJIT &created) {
    *jit = &created;
    return llvm::Error::success();
  });
#if LLVM_VERSION_MAJOR >= 22
  auto executor = std::make_unique<clang::IncrementalExecutorBuilder>();
  executor->JITBuilder = std::move(builder);
  auto interpreter =
      clang::Interpreter::create(std::move(*instance), std::move(executor));
#else
  auto interpreter =
      clang::Interpreter::create(std::move(*instance), std::move(builder));
#endif
  if (!interpreter) {
    throw std::runtime_error(llvm::toString(interpreter.takeError()));
  }
  auto engine = (*interpreter)->getExecutionEngine();
  if (!engine) {
    throw std::runtime_error(llvm::toString(engine.takeError()));
  }
  std::vector<std::string> initial_cells;
  // The public factory parses its own runtime declarations before user input.
  // Replay those exact buffers in the syntax-only checker as well, retaining
  // their macros and incremental source numbering on both Clang 21 and 22.
  const auto &sources =
      (*interpreter)->getCompilerInstance()->getSourceManager();
  for (unsigned index = 0; index < sources.local_sloc_entry_size(); ++index) {
    const auto &entry = sources.getLocalSLocEntry(index);
    if (!entry.isFile())
      continue;
    const auto buffer = entry.getFile().getContentCache().getBufferIfLoaded();
    if (buffer && buffer->getBufferIdentifier().starts_with("input_line_"))
      initial_cells.push_back(buffer->getBuffer().str());
  }
#if LLVM_VERSION_MAJOR == 21
  // Clang 21 does not emit its value-capture runtime tag until explicitly
  // asked.
  const auto tag = "__clang_Interpreter_NewTag __tracer_repl_newtag asm(\"" +
                   (*jit)->mangle("__ci_newtag") + "\");";
  if (auto error = (*interpreter)->ParseAndExecute(tag)) {
    throw std::runtime_error(llvm::toString(std::move(error)));
  }
  initial_cells.push_back(tag);
#endif
  const auto path = api_header();
  std::ostringstream prelude;
  prelude << "#include \"" << path << "\"\n#include <iostream>\n"
          << "auto &tracer = *reinterpret_cast<tracer_repl::Facade *>("
          << reinterpret_cast<std::uintptr_t>(&facade_) << "ULL);\n"
          << "const auto &session = tracer.selected_session();\n"
          << "using tracer_repl::take; using tracer_repl::drop; "
             "using tracer_repl::filter; using tracer_repl::take_while; "
             "using tracer_repl::first; using tracer_repl::transform; "
             "using tracer_repl::drop_while; using tracer_repl::stride; "
             "using tracer_repl::enumerate; using tracer_repl::reverse; "
             "using tracer_repl::to_vector;\n";
  if (auto error = (*interpreter)->ParseAndExecute(prelude.str())) {
    throw std::runtime_error("cannot initialize C++ facade: " +
                             llvm::toString(std::move(error)));
  }
  impl_ = std::make_unique<Impl>(path);
  impl_->interpreter = std::move(*interpreter);
  impl_->arguments = std::move(args);
  impl_->history = prelude.str() + "\n";
  for (const auto &cell : initial_cells)
    impl_->remember(cell);
  impl_->remember(prelude.str());
}

CppCompletions CppEngine::complete(const std::string &source) {
  if (auto directives = complete_cpp_directive(source)) return std::move(*directives);
  initialize();
  impl_->verify_inputs();
  if (!impl_->synchronized)
    throw std::runtime_error(
        "C++ completion requires %cpp reset after a parse error; tracing "
        "commands and explicitly terminated statements remain available");
  if (source.size() > 65536)
    return {};
  const auto helper = std::filesystem::path(executable_path()).parent_path() /
                      "clang-tracer-repl-check";
  const auto reply = run_cpp_analysis(helper.string(), "complete",
                                      impl_->arguments, impl_->history, source);
  if (reply.status != 0)
    throw std::runtime_error(reply.output);
  CppCompletions result;
  std::istringstream lines(reply.output);
  std::getline(lines, result.prefix);
  for (std::string item; std::getline(lines, item);)
    result.items.push_back(std::move(item));
  return result;
}

std::string CppEngine::describe_api(const std::string &topic) {
  initialize();
  return describe_cpp_api(*impl_->interpreter->getCompilerInstance(), topic);
}

std::string CppEngine::describe_symbol(const std::string &symbol) {
  initialize();
  impl_->verify_inputs();
  if (!impl_->synchronized)
    throw std::runtime_error("%type requires %cpp reset to restore C++ analysis");
  return describe_cpp_symbol(*impl_->interpreter->getCompilerInstance(), symbol);
}

bool CppEngine::evaluate(const std::string &source) {
  initialize();
  impl_->verify_inputs();
  // Clang's value printer drops a void expression when its semicolon is
  // omitted. Classify without running code, then execute the original once.
  if (!impl_->synchronized)
    throw std::runtime_error(
        "C++ compiler state diverged; use %cpp reset to recover");
  const auto helper = std::filesystem::path(executable_path()).parent_path() /
                      "clang-tracer-repl-check";
  const auto checked = impl_->checker.check(
      helper.string(), impl_->arguments, impl_->check_history, source);
  if (checked.status != 0 && checked.status != 2) {
    llvm::errs() << checked.output;
    llvm::errs().flush();
    return false;
  }
  const bool terminate = checked.status == 2;
  impl_->verify_inputs();
  const auto report = [](llvm::Error error) {
    llvm::logAllUnhandledErrors(std::move(error), llvm::errs(), "C++ input: ");
    return false;
  };
  const auto input = terminate ? source + "\n;" : source;
  auto parsed = impl_->interpreter->Parse(input);
  if (!parsed) {
    impl_->synchronized = false;
    return report(parsed.takeError());
  }
  impl_->user_cells = true;
  const auto history_size = impl_->history.size();
  const auto check_history_size = impl_->check_history.size();
  impl_->history += "\n#line 1\n" + source + "\n;\n";
  impl_->remember(input);
  if (parsed->TheModule) {
    const bool initializer = is_single_initializer(*parsed->TUPart, input);
    const bool unload = initializer && can_unload_initializer(*parsed->TheModule);
    const auto rollback_initializer = [&] {
      if (!initializer)
        return;
      if (!unload) {
        impl_->synchronized = false;
        llvm::errs() << "C++ initializer may have registered cleanup; "
                        "use %cpp reset to recover\n";
        return;
      }
      if (auto error = impl_->interpreter->Undo()) {
        impl_->synchronized = false;
        llvm::logAllUnhandledErrors(std::move(error), llvm::errs(),
                                    "C++ input rollback: ");
        return;
      }
      impl_->history.resize(history_size);
      impl_->check_history.resize(check_history_size);
      // Undo removes the declaration, but Clang still advances the input
      // number. Replay a blank cell in the syntax-only checker.
      impl_->remember("");
    };
    llvm::Error error = llvm::Error::success();
    try {
      error = impl_->interpreter->Execute(*parsed);
    } catch (...) {
      rollback_initializer();
      throw;
    }
    if (error) {
      rollback_initializer();
      return report(std::move(error));
    }
  }
  // The public ParseAndExecute interface transfers Clang's pending captured
  // value. An empty input retrieves it without executing the user's cell again.
  clang::Value value;
  impl_->remember("");
  if (auto error = impl_->interpreter->ParseAndExecute("", &value))
    return report(std::move(error));
  if (value.hasValue()) {
#if LLVM_VERSION_MAJOR >= 22
    value.print(llvm::outs());
#else
    llvm::outs() << "(" << value.getType().getAsString() << ") ";
    switch (value.getKind()) {
    case clang::Value::K_Bool:
      llvm::outs() << (value.getBool() ? "true" : "false");
      break;
#define TRACER_REPL_INTEGER(Name)                                              \
  case clang::Value::K_##Name:                                                 \
    llvm::outs() << +value.get##Name();                                        \
    break
      TRACER_REPL_INTEGER(Char_S);
      TRACER_REPL_INTEGER(Char_U);
      TRACER_REPL_INTEGER(SChar);
      TRACER_REPL_INTEGER(UChar);
      TRACER_REPL_INTEGER(Short);
      TRACER_REPL_INTEGER(UShort);
      TRACER_REPL_INTEGER(Int);
      TRACER_REPL_INTEGER(UInt);
      TRACER_REPL_INTEGER(Long);
      TRACER_REPL_INTEGER(ULong);
      TRACER_REPL_INTEGER(LongLong);
      TRACER_REPL_INTEGER(ULongLong);
#undef TRACER_REPL_INTEGER
    case clang::Value::K_Float:
      llvm::outs() << value.getFloat();
      break;
    case clang::Value::K_Double:
      llvm::outs() << value.getDouble();
      break;
    case clang::Value::K_LongDouble: {
      std::ostringstream printed;
      printed << std::setprecision(
                     std::numeric_limits<long double>::max_digits10)
              << value.getLongDouble();
      llvm::outs() << printed.str();
      break;
    }
    case clang::Value::K_PtrOrObj:
      llvm::outs() << value.getPtr();
      break;
    default:
      llvm::outs() << "<object>";
      break;
    }
    llvm::outs() << '\n';
#endif
  }
  llvm::outs().flush();
  return true;
}
} // namespace tracer_repl

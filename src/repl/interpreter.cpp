#include "interpreter.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Interpreter/Interpreter.h>
#include <clang/Interpreter/Value.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

#include "repl_config.hpp"
#include "server_process.hpp"
#if LLVM_VERSION_MAJOR >= 22
#include <clang/Interpreter/IncrementalExecutor.h>
#endif
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace tracer_repl {
struct CppEngine::Impl {
  std::unique_ptr<clang::Interpreter> interpreter;
};

CppEngine::CppEngine(Facade &facade, std::vector<std::string> arguments)
    : facade_(facade), arguments_(std::move(arguments)) {}

CppEngine::~CppEngine() = default;

void CppEngine::reset() { impl_.reset(); }

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
  const auto binary_directory =
      std::filesystem::path(executable_path()).parent_path();
  auto resource = binary_directory / TRACER_REPL_RESOURCE_RELATIVE;
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
  std::vector<std::string> args = {"-std=c++17", "-fPIC", "-O0",
                                   "-resource-dir", resource.string()};
  for (const auto *arg : repl_compiler_arguments) {
    args.emplace_back(arg);
  }
  args.insert(args.end(), arguments_.begin(), arguments_.end());
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
#if LLVM_VERSION_MAJOR == 21
  // Clang 21 does not emit its value-capture runtime tag until explicitly
  // asked.
  const auto tag = "__clang_Interpreter_NewTag __tracer_repl_newtag asm(\"" +
                   (*jit)->mangle("__ci_newtag") + "\");";
  if (auto error = (*interpreter)->ParseAndExecute(tag)) {
    throw std::runtime_error(llvm::toString(std::move(error)));
  }
#endif
  auto header = std::filesystem::path(executable_path()).parent_path() /
                TRACER_REPL_API_RELATIVE;
  if (!std::filesystem::is_regular_file(header)) {
    header = TRACER_REPL_API_HEADER;
  }
  const auto path = header.string();
  if (path.find_first_of("\"\r\n") != std::string::npos) {
    throw std::runtime_error("unsupported character in facade header path");
  }
  std::ostringstream prelude;
  prelude << "#include \"" << path << "\"\n#include <iostream>\n"
          << "auto &tracer = *reinterpret_cast<tracer_repl::Facade *>("
          << reinterpret_cast<std::uintptr_t>(&facade_) << "ULL);\n"
          << "const auto &session = tracer.selected_session();\n";
  if (auto error = (*interpreter)->ParseAndExecute(prelude.str())) {
    throw std::runtime_error("cannot initialize C++ facade: " +
                             llvm::toString(std::move(error)));
  }
  impl_ = std::make_unique<Impl>();
  impl_->interpreter = std::move(*interpreter);
}

bool CppEngine::evaluate(const std::string &source) {
  initialize();
  clang::Value value;
  if (auto error = impl_->interpreter->ParseAndExecute(source, &value)) {
    llvm::logAllUnhandledErrors(std::move(error), llvm::errs(), "C++ input: ");
    llvm::errs() << "Use %cpp reset if earlier C++ declarations no longer "
                    "resolve; tracing commands remain available.\n";
    return false;
  }
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

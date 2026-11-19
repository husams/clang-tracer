#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/PCHContainerOperations.h>
#include <clang/Interpreter/Interpreter.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>
#include <signal.h>
#include <sys/resource.h>
#include <unistd.h>

#include "callback_pch.hpp"
#include "tracer_runtime_config.hpp"

namespace {
namespace fs = std::filesystem;
using tracer::callback_pch::Artifact;
using tracer::callback_pch::Mode;

void require(bool condition, const char *message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

class TemporaryDirectory {
public:
  TemporaryDirectory() {
    constexpr std::string_view prefix = "/tmp/tracer-pch-test-XXXXXX";
    std::array<char, prefix.size() + 1> pattern{};
    std::copy(prefix.begin(), prefix.end(), pattern.begin());
    auto *created = ::mkdtemp(pattern.data());
    require(created != nullptr, "cannot create private test directory");
    path = created;
  }

  ~TemporaryDirectory() {
    std::error_code ignored;
    fs::remove_all(path, ignored);
  }

  fs::path path;
};

std::unique_ptr<clang::CompilerInstance>
make_compiler(const std::vector<std::string> &arguments) {
  std::vector<const char *> pointers;
  for (const auto &argument : arguments) {
    pointers.push_back(argument.c_str());
  }
  clang::IncrementalCompilerBuilder builder;
  builder.SetCompilerArgs(pointers);
  auto compiler = builder.CreateCpp();
  if (!compiler) {
    throw std::runtime_error(llvm::toString(compiler.takeError()));
  }
  return std::move(*compiler);
}

bool syntax_ok(const std::vector<std::string> &arguments,
               const std::string &source, const Artifact *artifact = nullptr) {
  auto syntax_arguments = arguments;
  syntax_arguments.push_back("-fsyntax-only");
  if (artifact) {
    tracer::callback_pch::append_syntax_args(syntax_arguments, *artifact);
  }
  auto ast = clang::tooling::buildASTFromCodeWithArgs(source, syntax_arguments,
                                                      "callback-pch-test.cc");
  return ast && !ast->getDiagnostics().hasErrorOccurred();
}

void check_fresh_interpreters(const std::vector<std::string> &arguments) {
  auto compiler = make_compiler(arguments);
  auto artifact = tracer::callback_pch::prepare(compiler->getInvocation(),
                                                Mode::Interpreter);
  require(artifact.has_value(), "interpreter PCH was not built");
  for (int repeat = 0; repeat != 2; ++repeat) {
    auto fresh = make_compiler(arguments);
    tracer::callback_pch::attach(fresh->getInvocation(), *artifact);
    auto interpreter = clang::Interpreter::create(std::move(fresh));
    if (!interpreter) {
      throw std::runtime_error(llvm::toString(interpreter.takeError()));
    }
    if (auto error =
            (*interpreter)
                ->ParseAndExecute(
                    "#include <string_view>\n"
                    "static int private_counter = 0;\n"
                    "extern \"C\" int callback_pch_next() {\n"
                    " return ++private_counter + TRACER_PCH_TEST_SENTINEL +\n"
                    "   static_cast<int>(std::string_view(\"ok\").size());\n"
                    "}\n")) {
      throw std::runtime_error(llvm::toString(std::move(error)));
    }
    auto address = (*interpreter)->getSymbolAddress("callback_pch_next");
    if (!address) {
      throw std::runtime_error(llvm::toString(address.takeError()));
    }
    auto next = reinterpret_cast<int (*)()>(
        static_cast<std::uintptr_t>(address->getValue()));
    require(next() == 5,
            "fresh interpreter shared another user's static state");
    require(next() == 6, "private interpreter state did not advance");
  }
}

void check_dependencies(const std::vector<std::string> &base_arguments) {
  TemporaryDirectory temporary;
  const auto wrapper = temporary.path / "cstddef";
  const auto earlier = temporary.path / "earlier";
  fs::create_directory(earlier);
  const auto write_wrapper = [&](int sentinel) {
    std::ofstream output(wrapper);
    output << "#pragma once\n#define TRACER_PCH_TEST_SENTINEL " << sentinel
           << "\n#include_next <cstddef>\n"
              "#if __has_include(\"appearing.hpp\")\n"
              "#include \"appearing.hpp\"\n"
              "#else\n#define TRACER_PCH_TEST_APPEARED 0\n#endif\n";
    output.close();
    require(static_cast<bool>(output),
            "cannot write controlled include wrapper");
  };
  write_wrapper(1);
  auto arguments = base_arguments;
  arguments.push_back("-I" + earlier.string());
  arguments.push_back("-I" + temporary.path.string());
  auto compiler = make_compiler(arguments);
  struct sigaction handler {}, previous {}, current {};
  handler.sa_handler = [](int) {};
  require(::sigemptyset(&handler.sa_mask) == 0 &&
              ::sigaction(SIGUSR1, &handler, &previous) == 0,
          "cannot install the application's SIGUSR1 handler");
  auto artifact =
      tracer::callback_pch::prepare(compiler->getInvocation(), Mode::Syntax);
  const bool handler_preserved = ::sigaction(SIGUSR1, nullptr, &current) == 0 &&
                                 current.sa_handler == handler.sa_handler;
  require(::sigaction(SIGUSR1, &previous, nullptr) == 0,
          "cannot restore the application's SIGUSR1 handler");
  require(handler_preserved, "PCH generation replaced the application's SIGUSR1 handler");
  require(artifact.has_value(), "syntax PCH was not built");
  require(syntax_ok(arguments, "static_assert(TRACER_PCH_TEST_SENTINEL == 1);",
                    &*artifact),
          "syntax PCH did not restore the original header");
  auto reused =
      tracer::callback_pch::prepare(compiler->getInvocation(), Mode::Syntax);
  require(reused && reused->pch_path == artifact->pch_path,
          "unchanged headers did not reuse their artifact");

  require(syntax_ok(arguments, "static_assert(TRACER_PCH_TEST_APPEARED == 0);",
                    &*artifact),
          "missing include branch was not captured");
  {
    std::ofstream appearing(temporary.path / "appearing.hpp");
    appearing << "#define TRACER_PCH_TEST_APPEARED 1\n";
    appearing.close();
    require(static_cast<bool>(appearing),
            "cannot create previously missing input");
  }
  auto appeared =
      tracer::callback_pch::prepare(compiler->getInvocation(), Mode::Syntax);
  require(appeared && syntax_ok(arguments,
                                "static_assert(TRACER_PCH_TEST_APPEARED == 1);",
                                &*appeared),
          "a newly available __has_include input kept stale semantics");

  const auto previous_time = fs::last_write_time(wrapper);
  write_wrapper(2);
  fs::last_write_time(wrapper, previous_time + std::chrono::seconds(2));
  auto rebuilt =
      tracer::callback_pch::prepare(compiler->getInvocation(), Mode::Syntax);
  require(rebuilt.has_value(), "changed header could not rebuild its PCH");
  require(syntax_ok(arguments, "static_assert(TRACER_PCH_TEST_SENTINEL == 2);",
                    &*rebuilt),
          "changed header restored stale semantics");
  require(!syntax_ok(arguments, "static_assert(TRACER_PCH_TEST_SENTINEL == 1);",
                     &*rebuilt),
          "stale header semantics were accepted");
  check_fresh_interpreters(arguments);

  // A previously absent earlier search result must supersede the old header.
  {
    std::ofstream shadow(earlier / "cstddef");
    shadow << "#pragma once\n#include_next <cstddef>\n"
              "#undef TRACER_PCH_TEST_SENTINEL\n"
              "#define TRACER_PCH_TEST_SENTINEL 3\n";
    shadow.close();
    require(static_cast<bool>(shadow), "cannot create earlier search input");
  }
  auto shadowed =
      tracer::callback_pch::prepare(compiler->getInvocation(), Mode::Syntax);
  require(shadowed && syntax_ok(arguments,
                                "static_assert(TRACER_PCH_TEST_SENTINEL == 3);",
                                &*shadowed),
          "a newly available include shadow kept stale semantics");

  auto forced = make_compiler(arguments);
  forced->getInvocation().getPreprocessorOpts().Includes.push_back(
      wrapper.string());
  require(!tracer::callback_pch::prepare(forced->getInvocation(),
                                         Mode::Interpreter),
          "user forced include was captured in the common cache");

  // Header initialization must not move ahead of callback syntax validation.
  {
    std::ofstream shadow(earlier / "cstddef");
    shadow << "#pragma once\n#include_next <cstddef>\n"
              "extern int initialize_pch_user_state();\n"
              "inline int pch_user_state = initialize_pch_user_state();\n";
    shadow.close();
    require(static_cast<bool>(shadow), "cannot create dynamic header input");
  }
  require(
      !tracer::callback_pch::prepare(compiler->getInvocation(),
                                     Mode::Interpreter),
      "header runtime initialization was cached before callback validation");
  for (const auto unsafe :
       {"__attribute__((constructor)) void header_ctor() {}\n",
        "__attribute__((destructor)) void header_dtor() {}\n",
        "struct HeaderCleanup { ~HeaderCleanup(); };\n"
        "inline HeaderCleanup cleanup;\n",
        "extern int initialize_template_state();\n"
        "template<class T> inline int template_state = "
        "initialize_template_state();\n"
        "inline int* use_template = &template_state<int>;\n",
        "extern int initialize_template_state();\n"
        "template<class T> struct Holder {\n"
        " inline static int value = initialize_template_state();\n"
        "};\ninline int* use_template = &Holder<int>::value;\n",
        "asm(\"\");\n"}) {
    std::ofstream shadow(earlier / "cstddef");
    shadow << "#pragma once\n#include_next <cstddef>\n" << unsafe;
    shadow.close();
    require(static_cast<bool>(shadow), "cannot create header cleanup input");
    if (tracer::callback_pch::prepare(compiler->getInvocation(),
                                      Mode::Interpreter))
      throw std::runtime_error(std::string("header effects were cached: ") +
                               unsafe);
  }
}

void check_unavailable_cache(const std::vector<std::string> &arguments) {
  // A separate process starts with no cache. Failed writes are deterministic
  // even as root, while the original syntax-only frontend remains usable.
  require(::signal(SIGXFSZ, SIG_IGN) != SIG_ERR,
          "cannot ignore file-size signal");
  const rlimit no_files{0, 0};
  require(::setrlimit(RLIMIT_FSIZE, &no_files) == 0,
          "cannot restrict cache output files");
  auto compiler = make_compiler(arguments);
  require(
      !tracer::callback_pch::prepare(compiler->getInvocation(), Mode::Syntax),
      "unwritable cache should not produce an artifact");
  require(
      syntax_ok(arguments, "#include <stdexcept>\nint value() { return 7; }"),
      "uncached valid source failed");
  require(
      !syntax_ok(arguments, "int value() { return missing_callback_value; }"),
      "uncached invalid source was accepted");
}
} // namespace

int main(int argc, char **argv) {
  try {
    require(argc >= 2, "pass the matching Clang resource directory");
    require(!llvm::InitializeNativeTarget(),
            "cannot initialize the native target");
    require(!llvm::InitializeNativeTargetAsmPrinter(),
            "cannot initialize the native assembler");
    std::vector<std::string> arguments(tracer::runtime::jit_flags.begin(),
                                       tracer::runtime::jit_flags.end());
    arguments.insert(arguments.end(),
                     {"-std=c++17", "-x", "c++", "-fPIC", "-O0",
                      std::string("-resource-dir=") + argv[1]});
    if (argc > 2 && std::string_view(argv[2]) == "--unavailable-cache") {
      check_unavailable_cache(arguments);
    } else {
      check_dependencies(arguments);
    }
    std::puts("callback PCH checks passed");
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "callback PCH check failed: %s\n", error.what());
    return 1;
  }
}

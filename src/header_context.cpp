#include "header_context.hpp"

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#if __has_include(<llvm/TargetParser/Triple.h>)
#include <llvm/TargetParser/Triple.h>
#else
#include <llvm/ADT/Triple.h>
#endif
#if __has_include(<llvm/TargetParser/Host.h>)
#include <llvm/TargetParser/Host.h>
#else
#include <llvm/Support/Host.h>
#endif
#include <llvm/Support/VirtualFileSystem.h>

#include "tracer/compiler_options.hpp"

namespace tracer::client {
namespace {

bool same_engine_target(const std::string& target) {
  const llvm::Triple requested(llvm::Triple::normalize(target));
  const llvm::Triple engine(
      llvm::Triple::normalize(llvm::sys::getDefaultTargetTriple()));
  return requested.getArch() == engine.getArch() &&
         requested.getOS() == engine.getOS() &&
         requested.getEnvironment() == engine.getEnvironment();
}

} // namespace

void resolve_header_context(clangtracer::v1::ProbeSpec& spec, const std::string& database_path) {
  namespace tooling = clang::tooling;
  if (spec.include_headers().empty()) {
    if (!database_path.empty()) {
      throw std::runtime_error("--compdb requires --include HEADER");
    }
    return;
  }
  std::string error;
  std::unique_ptr<tooling::CompilationDatabase> database;
  if (database_path.empty()) {
    database = tooling::CompilationDatabase::autoDetectFromSource(spec.include_headers(0), error);
    // Standalone headers still work with the default C++20 context.
    if (!database) {
      return;
    }
  } else {
    const auto path = std::filesystem::absolute(database_path);
    if (std::filesystem::is_directory(path)) {
      database = tooling::CompilationDatabase::loadFromDirectory(path.string(), error);
    } else {
      database = tooling::JSONCompilationDatabase::loadFromFile(
          path.string(), error, tooling::JSONCommandLineSyntax::AutoDetect);
    }
    if (!database) {
      throw std::runtime_error("cannot load compilation database: " + error);
    }
  }
  database = tooling::inferMissingCompileCommands(tooling::inferTargetAndDriverMode(
      tooling::expandResponseFiles(std::move(database), llvm::vfs::getRealFileSystem())));
  const auto commands = database->getCompileCommands(spec.include_headers(0));
  if (commands.empty()) {
    throw std::runtime_error("compilation database has no usable header command");
  }
  // Clang ranks donors using filename and directory similarity. All requested
  // headers form one translation unit, using the first header's configuration.
  auto command = tooling::transferCompileCommand(commands.front(), "clang-tracer-debug-types.cc");
  auto arguments =
      tooling::getClangStripDependencyFileAdjuster()(command.CommandLine, command.Filename);
  arguments = tooling::getClangStripOutputAdjuster()(arguments, command.Filename);
  arguments = tooling::getClangSyntaxOnlyAdjuster()(arguments, command.Filename);
  if (arguments.size() < 3 || arguments[arguments.size() - 2] != "--" ||
      arguments.back() != "clang-tracer-debug-types.cc") {
    throw std::runtime_error("cannot transfer the header compilation command");
  }
  auto* context = spec.mutable_header_context();
  context->set_working_directory(
      std::filesystem::absolute(command.Directory).lexically_normal().string());
  context->set_inference(commands.front().Heuristic.empty() ? "exact header command"
                                                            : commands.front().Heuristic);
  // Omit argv[0] and the transferred input: only flags cross the wire.
  std::size_t bytes = 0;
  for (std::size_t i = 1; i + 2 < arguments.size(); ++i) {
    const auto& argument = arguments[i];
    if (argument == "-target" || argument == "--target") {
      if (i + 1 >= arguments.size() - 2 ||
          !same_engine_target(arguments[i + 1])) {
        throw std::invalid_argument(
            "compilation database target does not match the engine target");
      }
      ++i; // The target process and engine already use this verified target.
      continue;
    }
    if (argument.rfind("-target=", 0) == 0 ||
        argument.rfind("--target=", 0) == 0) {
      const auto equal = argument.find('=');
      if (!same_engine_target(argument.substr(equal + 1))) {
        throw std::invalid_argument(
            "compilation database target does not match the engine target");
      }
      continue;
    }
    if (argument == "--driver-mode") {
      if (i + 1 >= arguments.size() - 2 ||
          (arguments[i + 1] != "g++" && arguments[i + 1] != "gcc" &&
           arguments[i + 1] != "c++" && arguments[i + 1] != "cpp")) {
        throw std::invalid_argument(
            "compilation database driver mode is not a supported C++ driver");
      }
      ++i;
      continue;
    }
    if (argument.rfind("--driver-mode=", 0) == 0) {
      const auto mode = argument.substr(sizeof("--driver-mode=") - 1);
      if (mode != "g++" && mode != "gcc" && mode != "c++" && mode != "cpp") {
        throw std::invalid_argument(
            "compilation database driver mode is not a supported C++ driver");
      }
      continue;
    }
    if (argument == "-x") {
      if (i + 1 >= arguments.size() - 2 ||
          (arguments[i + 1] != "c++" &&
           arguments[i + 1] != "c++-header" &&
           arguments[i + 1] != "c++-cpp-output")) {
        throw std::invalid_argument(
            "compilation database header language is not C++");
      }
      ++i;
      continue;
    }
    if (argument == "-xc++" || argument == "-xc++-header" ||
        argument == "-xc++-cpp-output")
      continue;
    if (argument == "-c" || argument == "-S" || argument == "-E" ||
        argument == "-fsyntax-only" || argument.rfind("-emit-", 0) == 0)
      continue;
    if (argument == "-o") {
      if (i + 1 < arguments.size() - 2)
        ++i;
      continue;
    }
    // Syntax-only/action options are discarded because the engine prepares
    // the callback in memory instead of replaying the donor build action.
    if (argument == "-fsyntax-only")
      continue;
    bytes += argument.size();
    if (argument.find('\0') != std::string::npos || argument.size() > 65536 || bytes > 65536 ||
        context->compiler_arguments_size() >= 512) {
      throw std::invalid_argument(
          "header compilation command exceeds limits");
    }
    context->add_compiler_arguments(argument);
  }
  for (const auto& argument : spec.compile_options()) {
    bytes += argument.size();
    if (argument.find('\0') != std::string::npos || argument.size() > 65536 ||
        bytes > 65536 || context->compiler_arguments_size() >= 512) {
      throw std::invalid_argument(
          "combined header compiler command exceeds limits");
    }
    context->add_compiler_arguments(argument);
  }
  std::vector<std::string> effective_arguments(
      context->compiler_arguments().begin(),
      context->compiler_arguments().end());
  std::string unsupported_argument;
  if (!safe_preflight_compiler_arguments(effective_arguments.begin(),
                                        effective_arguments.end(), true,
                                        &unsupported_argument)) {
    throw std::invalid_argument("header compiler command contains unsupported option: " +
                                unsupported_argument);
  }
}
} // namespace tracer::client

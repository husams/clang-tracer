#include "header_context.hpp"

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <llvm/Support/VirtualFileSystem.h>

namespace tracer::client {
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
    bytes += argument.size();
    if (argument.find('\0') != std::string::npos || argument.size() > 65536 || bytes > 65536 ||
        context->compiler_arguments_size() >= 512) {
      throw std::runtime_error("header compilation command exceeds limits");
    }
    context->add_compiler_arguments(argument);
  }
}
} // namespace tracer::client

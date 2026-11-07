#include "header_context.hpp"
#include "prepared_contract.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include <clang/Driver/Driver.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <llvm/ADT/Twine.h>
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
namespace fs = std::filesystem;
namespace tooling = clang::tooling;

bool same_engine_target(const std::string& target) {
  const llvm::Triple requested(llvm::Triple::normalize(target));
  const llvm::Triple engine(
      llvm::Triple::normalize(llvm::sys::getDefaultTargetTriple()));
  return requested.getArch() == engine.getArch() &&
         requested.getOS() == engine.getOS() &&
         requested.getEnvironment() == engine.getEnvironment();
}

std::unique_ptr<tooling::CompilationDatabase>
load_database(const std::string& header, const std::string& database_path) {
  std::string error;
  std::unique_ptr<tooling::CompilationDatabase> database;
  if (database_path.empty()) {
    database = tooling::CompilationDatabase::autoDetectFromSource(header, error);
    // Standalone headers still work with the default C++20 context.
    if (!database) {
      return {};
    }
  } else {
    const auto path = fs::absolute(database_path);
    if (fs::is_directory(path)) {
      database = tooling::CompilationDatabase::loadFromDirectory(path.string(), error);
    } else {
      database = tooling::JSONCompilationDatabase::loadFromFile(
          path.string(), error, tooling::JSONCommandLineSyntax::AutoDetect);
    }
    if (!database) {
      throw std::runtime_error("cannot load compilation database: " + error);
    }
  }
  return tooling::inferTargetAndDriverMode(
      tooling::expandResponseFiles(std::move(database), llvm::vfs::getRealFileSystem()));
}

tooling::CompileCommand select_prepared_command(tooling::CompilationDatabase& database,
                                               const std::string& header,
                                               const std::string& translation_unit) {
  if (!translation_unit.empty()) {
    auto commands = database.getCompileCommands(
        fs::absolute(translation_unit).lexically_normal().string());
    if (commands.size() != 1)
      throw std::runtime_error("--translation-unit must have exactly one compile command");
    return std::move(commands.front());
  }
  const auto directory = fs::canonical(header).parent_path();
  auto commands = database.getAllCompileCommands();
  const auto match = std::find_if(commands.begin(), commands.end(), [&](const auto& candidate) {
    const auto source = fs::path(candidate.Directory) / candidate.Filename;
    std::error_code path_error;
    const auto source_directory = fs::weakly_canonical(source.parent_path(), path_error);
    return !path_error && source_directory == directory;
  });
  if (match == commands.end())
    throw std::runtime_error("no compile command for a source in " + directory.string() +
                             ". Pass --translation-unit FILE.");
  return std::move(*match);
}

bool selects_toolchain(llvm::StringRef argument) {
  return argument.starts_with("--sysroot=") ||
         argument.starts_with("-isysroot") ||
         argument.starts_with("--gcc-toolchain=") ||
         argument.starts_with("--gcc-install-dir=") || argument.starts_with("-stdlib=") ||
         argument == "-nostdinc" || argument == "-nostdinc++";
}

bool prepared_driver_option(llvm::StringRef argument, bool clang_cl) {
  return argument.starts_with("--target=") ||
         argument.starts_with("--gcc-install-dir=") || argument.starts_with("-stdlib=") ||
         argument == "-nostdinc" || argument == "-nostdinc++" ||
         argument == "-m32" || argument == "-m64" || argument == "-mx32" ||
         argument == "-fshort-enums" || argument == "-fshort-wchar" ||
         (clang_cl && (argument == "/nologo" || argument == "/TP" || argument == "/EHsc" ||
                       argument == "/GR" || argument == "/GR-" ||
                       argument == "/MD" || argument == "/MDd" ||
                       argument == "/MT" || argument == "/MTd" ||
                       argument == "/W0" || argument == "/W1" || argument == "/W2" ||
                       argument == "/W3" || argument == "/W4" || argument == "/WX"));
}

std::vector<std::string> normalize_driver_options(const std::vector<std::string>& command) {
  std::vector<std::string> result{command.front()};
  for (std::size_t i = 1; i < command.size(); ++i) {
    const auto& argument = command[i];
    if (argument == "--") {
      result.insert(result.end(), command.begin() + i, command.end());
      break;
    }
    if (argument == "-target" || argument == "--target" || argument == "--driver-mode" ||
        argument == "--sysroot" || argument == "--gcc-toolchain" ||
        argument == "--gcc-install-dir") {
      if (++i == command.size() || command[i].empty() || command[i].front() == '-')
        throw std::invalid_argument("compilation database option is missing its value: " + argument);
      result.push_back((argument == "-target" ? "--target" : argument) + "=" + command[i]);
    } else {
      result.push_back(argument);
    }
    if (result.back() == "--driver-mode=c++")
      result.back() = "--driver-mode=g++";
  }
  return result;
}

std::string preflight_spelling(llvm::StringRef argument, bool clang_cl) {
  if (clang_cl) {
    for (const auto& [prefix, replacement] : {
             std::pair{"/external:I", "-isystem"}, {"/imsvc", "-isystem"},
             {"/FI", "-include"}, {"/I", "-I"}, {"/D", "-D"}, {"/U", "-U"}}) {
      auto value = argument;
      if (value.consume_front(prefix))
        return (llvm::Twine(replacement) + value).str();
    }
  }
  return argument.str();
}

} // namespace

PreparedHeaderCommand resolve_prepared_header_command(
    const std::string& header, const std::string& database_path,
    const std::string& translation_unit) {
  auto database = load_database(header, database_path);
  if (!database)
    throw std::runtime_error("compilation database was not found");
  auto donor = select_prepared_command(*database, header, translation_unit);
  if (donor.CommandLine.empty() || donor.CommandLine.front().empty())
    throw std::runtime_error("compilation database has no compiler executable");
  donor.CommandLine = normalize_driver_options(donor.CommandLine);
  std::vector<const char*> argv;
  for (const auto& argument : donor.CommandLine)
    argv.push_back(argument.c_str());
  const auto mode = clang::driver::getDriverMode(argv.front(), llvm::ArrayRef(argv).drop_front());
  if (mode == "cpp")
    throw std::invalid_argument("compilation database driver mode cpp only preprocesses input");
  const bool clang_cl = clang::driver::IsClangCL(mode);
  if (clang_cl) {
    for (std::size_t i = 1; i < donor.CommandLine.size(); ++i) {
      auto& argument = donor.CommandLine[i];
      if (argument == "--")
        break;
      auto standard = llvm::StringRef(argument);
      if (standard.consume_front("/std:")) {
        if (standard == "c++latest")
          standard = "c++2c";
        if (standard == "c++23preview")
          standard = "c++23";
        argument = ("/clang:-std=" + standard).str();
      } else if (selects_toolchain(argument) ||
                 argument == "-fshort-enums" || argument == "-fshort-wchar") {
        if (argument == "-isysroot" && i + 1 < donor.CommandLine.size())
          donor.CommandLine[i + 1].insert(0, "/clang:");
        argument.insert(0, "/clang:");
      }
    }
  }
  auto command = tooling::transferCompileCommand(std::move(donor), "clang-tracer-prepared-headers.cc");
  auto arguments = tooling::getClangStripDependencyFileAdjuster()(command.CommandLine,
                                                                  command.Filename);
  const auto directory = fs::absolute(command.Directory).lexically_normal();
  const fs::path driver = arguments.front();
  PreparedHeaderCommand result{directory.string(), driver.string(), {}, {}, clang_cl};
  if (driver.is_relative() && driver.has_parent_path())
    result.driver = (directory / driver).lexically_normal().string();
  std::vector<std::string> preflight;
  std::size_t bytes = 0;
  for (std::size_t i = 1; i + 2 < arguments.size(); ++i) {
    auto argument = llvm::StringRef(arguments[i]);
    const bool forwarded = clang_cl && argument.consume_front("/clang:");
    bytes += arguments[i].size();
    if (argument.empty() || argument.contains('\0') || bytes > 65536 || i > 512)
      throw std::invalid_argument("header compilation command exceeds limits or has an empty option");
    if (!forwarded && (argument == "-c" || argument == "-S" || argument == "-E" ||
                       argument == "-fsyntax-only" ||
                       (clang_cl && (argument == "/c" || argument == "/Zs"))))
      continue;
    if (selects_toolchain(argument))
      result.use_default_toolchain = false;
    auto spelling = preflight_spelling(argument, clang_cl && !forwarded);
    auto abi_option = spelling;
    if (abi_option == "-D" && i + 3 < arguments.size()) {
      auto value = llvm::StringRef(arguments[i + 1]);
      if (forwarded)
        value.consume_front("/clang:");
      abi_option += value.str();
    }
    if (tracer::prepared::abi_option(abi_option))
      result.adapter_abi_options.push_back(std::move(abi_option));
    if ((!forwarded && (argument == "--driver-mode=cl" || argument == "--driver-mode=g++" ||
                        argument == "--driver-mode=gcc" || argument == "--driver-mode=cpp")) ||
        prepared_driver_option(argument, clang_cl && !forwarded)) {
      result.arguments.push_back(arguments[i]);
      continue;
    }
    if (argument == "-isysroot") {
      preflight.emplace_back("--sysroot");
    } else if (argument.starts_with("-isysroot")) {
      preflight.push_back(("--sysroot=" + argument.drop_front(9)).str());
    } else if (clang_cl && !forwarded && argument.starts_with("/FI") && argument.size() > 3) {
      preflight.emplace_back("-include");
      preflight.push_back(argument.drop_front(3).str());
    } else {
      preflight.push_back(std::move(spelling));
    }
    result.arguments.push_back(arguments[i]);
  }
  std::string unsupported;
  if (!safe_preflight_compiler_arguments(preflight.begin(), preflight.end(), true, &unsupported))
    throw std::invalid_argument("header compiler command contains unsupported option: " + unsupported);
  return result;
}

void resolve_header_context(clangtracer::v1::ProbeSpec& spec, const std::string& database_path) {
  if (spec.include_headers().empty()) {
    if (!database_path.empty())
      throw std::runtime_error("--compdb requires --include HEADER");
    return;
  }
  auto database = load_database(spec.include_headers(0), database_path);
  if (!database)
    return;
  database = tooling::inferMissingCompileCommands(std::move(database));
  const auto commands = database->getCompileCommands(spec.include_headers(0));
  if (commands.empty())
    throw std::runtime_error("compilation database has no usable header command");
  const auto& donor = commands.front();
  // All requested headers share the selected command's configuration.
  auto command = tooling::transferCompileCommand(donor, "clang-tracer-debug-types.cc");
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
  context->set_inference(donor.Heuristic.empty() ? "exact header command" : donor.Heuristic);
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

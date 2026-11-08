#include "header_context.hpp"

#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include <clang/Driver/Driver.h>

#include "prepared_contract.hpp"
#if __has_include(<clang/Options/Options.h>)
#include <clang/Options/Options.h>
#define TRACER_CLANG_OPTIONS_V22 1
#else
#include <clang/Driver/Options.h>
#endif
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Option/ArgList.h>
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
#include <llvm/Option/Arg.h>
#include <llvm/Support/VirtualFileSystem.h>

#include "tracer/compiler_options.hpp"

namespace tracer::client {
namespace {
namespace fs = std::filesystem;
namespace tooling = clang::tooling;

bool same_engine_target(const std::string& target) {
  const llvm::Triple requested(llvm::Triple::normalize(target));
  const llvm::Triple engine(llvm::Triple::normalize(llvm::sys::getDefaultTargetTriple()));
  return requested.getArch() == engine.getArch() && requested.getOS() == engine.getOS() &&
         requested.getEnvironment() == engine.getEnvironment();
}

std::unique_ptr<tooling::CompilationDatabase> load_database(const std::string& header,
                                                            const std::string& database_path) {
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
    const auto source = fs::absolute(translation_unit).lexically_normal();
    auto commands = database.getCompileCommands(source.string());
    // FileMatchTrie may need an existing file to match a symlinked directory.
    // Resolve the existing parent path when the donor itself is absent.
    std::error_code error;
    if (commands.empty()) {
      const auto canonical_source = fs::weakly_canonical(source, error);
      if (!error)
        for (auto& candidate : database.getAllCompileCommands()) {
          const auto candidate_source = fs::weakly_canonical(
              fs::path(candidate.Directory) / candidate.Filename, error);
          if (!error && candidate_source == canonical_source)
            commands.push_back(std::move(candidate));
        }
    }
    if (commands.size() != 1) {
      throw std::runtime_error("--translation-unit must have exactly one compile command");
    }
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
  if (match == commands.end()) {
    throw std::runtime_error("no compile command for a source in " + directory.string() +
                             ". Pass --translation-unit FILE.");
  }
  return std::move(*match);
}

bool selects_toolchain(llvm::StringRef argument) {
  return argument.starts_with("--sysroot=") || argument.starts_with("-isysroot") ||
         argument.starts_with("--gcc-toolchain=") || argument.starts_with("--gcc-install-dir=") ||
         argument.starts_with("-stdlib=") || argument == "-nostdinc" || argument == "-nostdinc++";
}

bool starts_with_any(llvm::StringRef value, std::initializer_list<llvm::StringRef> prefixes) {
  return std::any_of(prefixes.begin(), prefixes.end(),
                     [&](llvm::StringRef prefix) { return value.starts_with(prefix); });
}

bool unsafe_prepared_option(const llvm::opt::Option& option) {
  const auto name = option.getUnaliasedOption().getName();
  // These options cross the compiler-driver boundary, load code, or write
  // files/caches as a side effect even for a syntax-only invocation.
  return starts_with_any(name, {
      "X", "cc1", "mllvm", "Wp,", "Wa,", "Wl,", "fplugin", "fpass-plugin",
      "fmodule", "fimplicit-module", "fprebuilt-", "fcxx-modules", "include-pch",
      "include-pth", "Yc", "Yu", "Fp", "ivfsoverlay", "vfsoverlay", "config",
      "fconfig", "fprofile", "fcoverage", "ftime-trace", "serialize-diagnostics",
      "diagnostic-log-file", "serialize-diagnostic-file", "verify",
      "dependency-dot", "module-dependency-dir", "header-include-file",
      "fproc-stat-report", "fcrash-diagnostics-dir", "fdiagnostics-file",
      "fdiagnostics-serialization-file", "foptimization-record-file",
      "save-temps", "save-stats", "dump", "rewrite", "analyze", "emit-",
      "print-", "gen-reproducer", "gen-cdb-fragment-path", "offload", "###", "dry-run"});
}

bool safe_clang_cl_forwarded_option(llvm::StringRef argument) {
  return safe_preflight_compiler_argument(argument.str()) ||
         selects_toolchain(argument) || argument.starts_with("--target=") ||
         argument == "-m32" || argument == "-m64" || argument == "-mx32" ||
         argument == "-fshort-enums" || argument == "-fshort-wchar";
}

bool forwarded_option_takes_operand(llvm::StringRef argument) {
  return argument == "-I" || argument == "-isystem" || argument == "-iquote" ||
         argument == "-idirafter" || argument == "-include" || argument == "-D" ||
         argument == "-U" || argument == "-std" || argument == "-isysroot" ||
         argument == "--sysroot" || argument == "--gcc-toolchain";
}

bool removable_prepared_action(const llvm::opt::Option& option) {
  const auto name = option.getUnaliasedOption().getName();
  return name == "c" || name == "S" || name == "E" || name == "fsyntax-only" || name == "o" ||
         name == "MF" || name == "MT" || name == "MQ" || name == "MJ" || name == "MD" ||
         name == "MMD" || name == "MM" || name == "MG" || name == "MP" || name == "M" ||
         name == "dependency-file" || name == "Fo" || name == "Fe" || name == "Fd" ||
         name == "Fa" || name == "Fi" || name == "FR" ||
         name == "sourceDependencies" || name == "Zs";
}

bool same_source_argument(llvm::StringRef argument, const tooling::CompileCommand& command) {
  return (fs::path(command.Directory) / fs::path(argument.str())).lexically_normal() ==
         (fs::path(command.Directory) / command.Filename).lexically_normal();
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
      if (++i == command.size() || command[i].empty() || command[i].front() == '-') {
        throw std::invalid_argument("compilation database option is missing its value: " +
                                    argument);
      }
      result.push_back((argument == "-target" ? "--target" : argument) + "=" + command[i]);
    } else {
      result.push_back(argument);
    }
    if (result.back() == "--driver-mode=c++") {
      result.back() = "--driver-mode=g++";
    }
  }
  return result;
}

bool is_input_option(const llvm::opt::Option& option) {
#if defined(TRACER_CLANG_OPTIONS_V22)
  return option.matches(clang::options::OPT_INPUT);
#else
  return option.matches(clang::driver::options::OPT_INPUT);
#endif
}

const llvm::opt::OptTable& prepared_driver_opt_table() {
#if defined(TRACER_CLANG_OPTIONS_V22)
  return clang::getDriverOptTable();
#else
  return clang::driver::getDriverOptTable();
#endif
}

unsigned prepared_driver_visibility(bool clang_cl) {
#if defined(TRACER_CLANG_OPTIONS_V22)
  return clang_cl ? clang::options::CLOption : clang::options::ClangOption;
#else
  return clang_cl ? clang::driver::options::CLOption : clang::driver::options::ClangOption;
#endif
}

} // namespace

static PreparedHeaderCommand normalize_prepared_header_command(tooling::CompileCommand donor) {
  if (donor.CommandLine.empty() || donor.CommandLine.front().empty() ||
      donor.CommandLine.front().find('\0') != std::string::npos) {
    throw std::runtime_error("compilation database has no compiler executable");
  }
  for (const auto& argument : donor.CommandLine) {
    if (argument.find('\0') != std::string::npos) {
      throw std::invalid_argument("header compilation command contains a NUL byte");
    }
  }
  donor.CommandLine = normalize_driver_options(donor.CommandLine);
  std::vector<const char*> argv;
  for (const auto& argument : donor.CommandLine) {
    argv.push_back(argument.c_str());
  }
  const auto mode = clang::driver::getDriverMode(argv.front(), llvm::ArrayRef(argv).drop_front());
  if (mode == "cpp") {
    throw std::invalid_argument("compilation database driver mode cpp only preprocesses input");
  }
  const bool clang_cl = clang::driver::IsClangCL(mode);
  if (clang_cl) {
    for (std::size_t i = 1; i < donor.CommandLine.size(); ++i) {
      auto& argument = donor.CommandLine[i];
      if (argument == "--") {
        break;
      }
      auto standard = llvm::StringRef(argument);
      if (standard.consume_front("/std:")) {
        if (standard == "c++latest") {
          standard = "c++2c";
        }
        if (standard == "c++23preview") {
          standard = "c++23";
        }
        argument = ("/clang:-std=" + standard).str();
      } else if (selects_toolchain(argument) || argument == "-fshort-enums" ||
                 argument == "-fshort-wchar") {
        if (argument == "-isysroot" && i + 1 < donor.CommandLine.size()) {
          donor.CommandLine[i + 1].insert(0, "/clang:");
        }
        argument.insert(0, "/clang:");
      }
    }
  }
  const auto directory = fs::absolute(donor.Directory).lexically_normal();
  const fs::path driver = donor.CommandLine.front();
  PreparedHeaderCommand result{directory.string(), driver.string(), {}, {}, clang_cl, true,
                               (directory / donor.Filename).lexically_normal().string()};
  if (driver.is_relative() && driver.has_parent_path()) {
    result.driver = (directory / driver).lexically_normal().string();
  }
  std::vector<const char*> option_argv;
  option_argv.reserve(donor.CommandLine.size() - 1);
  std::size_t raw_bytes = 0;
  bool clang_cl_forwarded_operand = false;
  for (std::size_t i = 1; i < donor.CommandLine.size(); ++i) {
    const llvm::StringRef raw(donor.CommandLine[i]);
    if (raw.empty() || raw.front() == '@') {
      throw std::invalid_argument(
          "header compilation command contains unsupported option: @response-file");
    }
    raw_bytes += raw.size();
    if (raw_bytes > 65536 || i > 512) {
      throw std::invalid_argument("header compilation command exceeds limits");
    }
    if (raw == "--") {
      if (i + 2 != donor.CommandLine.size() ||
          !same_source_argument(donor.CommandLine[i + 1], donor)) {
        throw std::invalid_argument("header compilation command contains unsupported option: --");
      }
      ++i;
      continue;
    }
    if (raw.starts_with_insensitive("/link") || raw.starts_with_insensitive("-link") ||
        raw.starts_with_insensitive("-clang:") || raw.starts_with("-X") ||
        raw.starts_with("-mllvm") || raw.starts_with("-Wp,") || raw.starts_with("-Wa,") ||
        raw.starts_with("-Wl,")) {
      throw std::invalid_argument("header compilation command contains unsupported option: " +
                                  raw.str());
    }
    if (raw.starts_with_insensitive("/clang:")) {
      const auto forwarded = raw.drop_front(7);
      const bool safe = clang_cl_forwarded_operand
          ? !forwarded.empty() && forwarded.front() != '-' && forwarded.front() != '@'
          : safe_clang_cl_forwarded_option(forwarded);
      if (!clang_cl || !raw.starts_with("/clang:") || !safe) {
        throw std::invalid_argument(
            "header compilation command contains unsupported option: /clang:");
      }
      clang_cl_forwarded_operand = !clang_cl_forwarded_operand &&
                                   forwarded_option_takes_operand(forwarded);
    }
    option_argv.push_back(donor.CommandLine[i].c_str());
  }
  if (clang_cl_forwarded_operand)
    throw std::invalid_argument("header compilation command option is missing a /clang: operand");
  unsigned missing_index = 0;
  unsigned missing_count = 0;
  const auto all_visibility = prepared_driver_opt_table().ParseArgs(
      llvm::ArrayRef<const char*>(option_argv), missing_index, missing_count);
  if (missing_count != 0) {
    const auto index = std::min<std::size_t>(missing_index, option_argv.size() - 1);
    throw std::invalid_argument("header compilation command option is missing a value: " +
                                std::string(option_argv[index]));
  }
  for (const auto* argument : all_visibility) {
    if (unsafe_prepared_option(argument->getOption()) &&
        !removable_prepared_action(argument->getOption())) {
      throw std::invalid_argument("header compilation command contains unsupported option: " +
                                  argument->getSpelling().str());
    }
  }
  missing_index = 0;
  missing_count = 0;
  const auto parsed = prepared_driver_opt_table().ParseArgs(
      llvm::ArrayRef<const char*>(option_argv), missing_index, missing_count,
      llvm::opt::Visibility(prepared_driver_visibility(clang_cl)));
  if (missing_count != 0) {
    const auto index = std::min<std::size_t>(missing_index, option_argv.size() - 1);
    throw std::invalid_argument("header compilation command option is missing a value: " +
                                std::string(option_argv[index]));
  }
  std::vector<bool> remove(option_argv.size(), false);
  std::vector<const llvm::opt::Arg*> parsed_arguments;
  for (const auto* argument : parsed) {
    parsed_arguments.push_back(argument);
  }
  for (std::size_t parsed_index = 0; parsed_index < parsed_arguments.size(); ++parsed_index) {
    const auto* argument = parsed_arguments[parsed_index];
    const auto& option = argument->getOption();
    if (unsafe_prepared_option(option) && !removable_prepared_action(option)) {
      throw std::invalid_argument("header compilation command contains unsupported option: " +
                                  argument->getSpelling().str());
    }
    const auto index = argument->getIndex();
    if (index >= remove.size()) {
      continue;
    }
    if (removable_prepared_action(option)) {
      const auto end =
          parsed_index + 1 < parsed_arguments.size()
              ? std::min(remove.size(),
                         static_cast<std::size_t>(parsed_arguments[parsed_index + 1]->getIndex()))
              : remove.size();
      std::fill(remove.begin() + index, remove.begin() + end, true);
    } else if (is_input_option(option) && argument->getNumValues() > 0 &&
               same_source_argument(argument->getValue(), donor)) {
      remove[index] = true;
    }
  }

  for (std::size_t i = 0; i < option_argv.size(); ++i) {
    if (remove[i]) {
      continue;
    }
    auto argument = llvm::StringRef(option_argv[i]);
    if (clang_cl)
      argument.consume_front("/clang:");
    if (selects_toolchain(argument)) {
      result.use_default_toolchain = false;
    }
    // Preserve every remaining original token, including unknown GCC flags,
    // so Clang can issue the authoritative driver/parser diagnostic.
    result.arguments.emplace_back(option_argv[i]);
  }
  for (const auto* argument : parsed) {
    const auto& option = argument->getOption();
    const auto name = option.getUnaliasedOption().getName();
    if (name.starts_with("std=") && argument->getNumValues() == 1 &&
        argument->getIndex() < option_argv.size() &&
        (llvm::StringRef(option_argv[argument->getIndex()]).starts_with("-std=") ||
         llvm::StringRef(option_argv[argument->getIndex()]).starts_with("--std="))) {
      auto value = llvm::StringRef(argument->getValue());
      if (value == "gnu++2a") {
        value = "gnu++20";
      }
      if (value == "c++2a") {
        value = "c++20";
      }
      if (value == "gnu++2b") {
        value = "gnu++23";
      }
      if (value == "c++2b") {
        value = "c++23";
      }
      const auto normalized = (llvm::Twine("-std=") + value).str();
      if (tracer::prepared::abi_option(normalized)) {
        result.adapter_abi_options.push_back(normalized);
      }
    } else if ((name == "D" || name == "D_" || name == "D__" || argument->getSpelling() == "-D") &&
               argument->getNumValues() == 1) {
      const auto value = llvm::StringRef(argument->getValue());
      const auto definition = value == "_GLIBCXX_USE_CXX11_ABI" ? "-D_GLIBCXX_USE_CXX11_ABI=1"
                                                                 : (llvm::Twine("-D") + value).str();
      if (tracer::prepared::abi_option(definition)) {
        result.adapter_abi_options.push_back(definition);
      }
    } else if ((name == "fshort-enums" || name == "fshort-wchar") &&
               tracer::prepared::abi_option(argument->getSpelling().str())) {
      result.adapter_abi_options.push_back(argument->getSpelling().str());
    }
  }
  for (const auto& raw : donor.CommandLine) {
    llvm::StringRef value(raw);
    if (value.consume_front("/clang:-std=")) {
      if (value == "gnu++2a") {
        value = "gnu++20";
      }
      if (value == "c++2a") {
        value = "c++20";
      }
      if (value == "gnu++2b") {
        value = "gnu++23";
      }
      if (value == "c++2b") {
        value = "c++23";
      }
      const auto normalized = (llvm::Twine("-std=") + value).str();
      if (tracer::prepared::abi_option(normalized)) {
        result.adapter_abi_options.push_back(normalized);
      }
    } else if (value.consume_front("/clang:-D")) {
      const auto definition = value == "_GLIBCXX_USE_CXX11_ABI" ? "-D_GLIBCXX_USE_CXX11_ABI=1"
                                                                 : (llvm::Twine("-D") + value).str();
      if (tracer::prepared::abi_option(definition)) {
        result.adapter_abi_options.push_back(definition);
      }
    } else if (value == "/clang:-fshort-enums" || value == "/clang:-fshort-wchar") {
      const auto option = value.drop_front(7).str();
      if (tracer::prepared::abi_option(option)) {
        result.adapter_abi_options.push_back(option);
      }
    }
  }
  return result;
}

PreparedHeaderCommand resolve_prepared_header_command(const std::string& header,
                                                      const std::string& database_path,
                                                      const std::string& translation_unit) {
  auto database = load_database(header, database_path);
  if (!database)
    throw std::runtime_error("compilation database was not found");
  return normalize_prepared_header_command(select_prepared_command(*database, header, translation_unit));
}

namespace {
tooling::CompileCommand select_callback_command(tooling::CompilationDatabase& database,
                                                const clangtracer::v1::ProbeSpec& spec,
                                                const std::string& translation_unit) {
  if (!translation_unit.empty())
    return select_prepared_command(database, {}, translation_unit);
  const auto& hint = spec.prepared().source_translation_unit();
  if (!hint.empty()) {
    auto exact = database.getCompileCommands(hint);
    if (exact.size() == 1)
      return std::move(exact.front());
    if (exact.size() > 1)
      throw std::invalid_argument("multiple commands for the prepared translation unit; pass --translation-unit FILE with a unique command");
  }
  auto commands = database.getAllCompileCommands();
  if (commands.size() == 1)
    return std::move(commands.front());

  const auto unique_match = [&](const auto& matches) -> tooling::CompileCommand* {
    tooling::CompileCommand* selected = nullptr;
    for (auto& command : commands) {
      if (!matches(fs::path(command.Filename)))
        continue;
      if (selected)
        return nullptr;
      selected = &command;
    }
    return selected;
  };
  if (!hint.empty()) {
    const auto filename = fs::path(hint).filename();
    if (auto* command = unique_match([&](const fs::path& source) { return source.filename() == filename; }))
      return std::move(*command);
  }
  auto owner = spec.debug_info().record_name();
  if (owner.empty()) {
    owner = spec.prepared().selector();
    owner = owner.substr(0, owner.find('('));
  }
  owner = owner.substr(0, owner.find('<'));
  const auto scope = owner.rfind("::");
  const auto stem = scope == owner.npos ? owner : owner.substr(scope + 2);
  if (auto* command = unique_match([&](const fs::path& source) { return source.stem() == stem; }))
    return std::move(*command);
  throw std::invalid_argument("cannot select a callback compile command; pass --translation-unit FILE");
}

void resolve_prepared_callback_context(clangtracer::v1::ProbeSpec& spec, const std::string& database_path,
                                       const std::string& translation_unit) {
  auto database = load_database({}, database_path);
  const auto command = normalize_prepared_header_command(
      select_callback_command(*database, spec, translation_unit));
  std::vector<const char*> arguments;
  for (const auto& argument : command.arguments)
    arguments.push_back(command.clang_cl && llvm::StringRef(argument).starts_with("/clang:")
                            ? argument.c_str() + 7 : argument.c_str());
  unsigned missing_index = 0, missing_count = 0;
  const auto parsed = prepared_driver_opt_table().ParseArgs(arguments, missing_index, missing_count);
  if (missing_count)
    throw std::invalid_argument("callback compilation command option is missing a value");

  const auto& adapter_options = spec.prepared().adapter_abi_options();
  const auto adapter_flag = [&](llvm::StringRef flag) {
    return std::find(adapter_options.begin(), adapter_options.end(), flag.str()) != adapter_options.end();
  };
  std::string adapter_abi_macro;
  for (const auto& option : adapter_options)
    if (llvm::StringRef(option).starts_with("-D_GLIBCXX_USE_CXX11_ABI="))
      adapter_abi_macro = option.substr(2);

  auto* context = spec.mutable_header_context();
  context->set_working_directory(command.directory);
  context->set_inference(command.translation_unit);
  bool relative_forced_include = false;
  for (const auto* argument : parsed) {
    const auto name = argument->getOption().getUnaliasedOption().getName();
    if (name == "m32" || name == "mx32" || name.starts_with("mabi=") ||
        (name == "target=" && !same_engine_target(argument->getValue())))
      throw std::invalid_argument("compilation database target does not match the engine target");
    if ((name == "fshort-enums" && !adapter_flag("-fshort-enums")) ||
        (name == "fshort-wchar" && !adapter_flag("-fshort-wchar")) ||
        (name == "fno-short-enums" && adapter_flag("-fshort-enums")) ||
        (name == "fno-short-wchar" && adapter_flag("-fshort-wchar")))
      throw std::invalid_argument("callback compilation database disagrees with prepared ABI option " +
                                  argument->getSpelling().str());
    const auto option = ("-" + name).str();
    if (argument->getNumValues() != 1)
      continue;
    const auto value = std::string(argument->getValue());
    if (option == "-D" || option == "-U") {
      if (prepared_callback_abi_macro(value)) {
        const auto definition = value == "_GLIBCXX_USE_CXX11_ABI" ? value + "=1" : value;
        if (option == "-U" || definition != adapter_abi_macro)
          throw std::invalid_argument("callback compilation database disagrees with prepared _GLIBCXX_USE_CXX11_ABI");
        continue;
      }
    } else if (option == "-include") {
      relative_forced_include |= fs::path(value).is_relative();
    } else if (!prepared_callback_include_directory(option)) {
      continue;
    }
    context->add_compiler_arguments(option);
    if (prepared_callback_include_directory(option) && !value.empty() && value.front() != '=' &&
        !llvm::StringRef(value).starts_with("$SYSROOT")) {
      context->add_compiler_arguments((fs::path(command.directory) / value).lexically_normal().string());
    } else {
      context->add_compiler_arguments(value);
    }
  }
  if (relative_forced_include) {
    context->add_compiler_arguments("-iquote");
    context->add_compiler_arguments(command.directory);
    auto* options = context->mutable_compiler_arguments();
    std::rotate(options->begin(), options->end() - 2, options->end());
  }
  if (context->ByteSizeLong() > 64 * 1024 || context->compiler_arguments_size() > 512)
    throw std::invalid_argument("callback compilation command exceeds limits");
  if (!safe_prepared_callback_compiler_arguments(context->compiler_arguments().begin(),
                                                 context->compiler_arguments().end()))
    throw std::invalid_argument("invalid callback include or macro option in compilation database");
}
} // namespace

void resolve_header_context(clangtracer::v1::ProbeSpec& spec, const std::string& database_path,
                            const std::string& translation_unit) {
  if (tracer::prepared::is_typed_version(spec.prepared().format_version())) {
    if (!database_path.empty())
      resolve_prepared_callback_context(spec, database_path, translation_unit);
    return;
  }
  if (spec.include_headers().empty()) {
    if (!database_path.empty())
      throw std::runtime_error("--compdb requires --include HEADER");
    return;
  }
  auto database = load_database(spec.include_headers(0), database_path);
  if (!database)
    return;
  const auto donor = [&] {
    if (!translation_unit.empty())
      return select_prepared_command(*database, {}, translation_unit);
    database = tooling::inferMissingCompileCommands(std::move(database));
    auto commands = database->getCompileCommands(spec.include_headers(0));
    if (commands.empty())
      throw std::runtime_error("compilation database has no usable header command");
    return std::move(commands.front());
  }();
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

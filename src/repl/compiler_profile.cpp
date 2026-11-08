#include "compiler_profile.hpp"

#include <algorithm>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <filesystem>
#include <llvm/Config/llvm-config.h>
#include <stdexcept>
#if LLVM_VERSION_MAJOR >= 22
#include <clang/Options/Options.h>
#else
#include <clang/Driver/Options.h>
#endif
#include <llvm/Option/ArgList.h>
#include <llvm/Option/OptTable.h>

namespace tracer_repl {
namespace {
namespace fs = std::filesystem;
#if LLVM_VERSION_MAJOR >= 22
namespace options = clang::options;
using clang::getDriverOptTable;
#else
namespace options = clang::driver::options;
using clang::driver::getDriverOptTable;
#endif
fs::path absolute(const fs::path &path, const fs::path &directory) {
  return fs::weakly_canonical(path.is_absolute() ? path : directory / path);
}
bool cpp_source(const fs::path &path) {
  const auto ext = path.extension().string();
  return ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c++" ||
         ext == ".C";
}
} // namespace

DatabaseProfile compilation_database_profile(const std::string &requested,
                                             const std::string &file) {
  if (requested.empty() || file.empty())
    throw std::invalid_argument(
        "database and source file paths must be nonempty");
  auto path = fs::absolute(requested);
  if (fs::is_directory(path))
    path /= "compile_commands.json";
  if (fs::file_size(path) > 16 * 1024 * 1024)
    throw std::invalid_argument("compilation database exceeds 16 MiB");
  std::string error;
  auto database = clang::tooling::JSONCompilationDatabase::loadFromFile(
      path.string(), error, clang::tooling::JSONCommandLineSyntax::Gnu);
  if (!database)
    throw std::runtime_error("cannot load compilation database: " + error);
  auto commands = database->getAllCompileCommands();
  const auto target = absolute(file, fs::current_path());
  if (fs::is_directory(target))
    throw std::invalid_argument("--file must name a source or header file");
  const auto command_file = [&](const auto &command) {
    return absolute(command.Filename,
                    absolute(command.Directory, path.parent_path()));
  };
  auto chosen =
      std::find_if(commands.begin(), commands.end(), [&](const auto &command) {
        return command_file(command) == target;
      });
  if (chosen == commands.end()) {
    for (auto it = commands.begin(); it != commands.end(); ++it) {
      const auto candidate = command_file(*it);
      if (!cpp_source(candidate) ||
          candidate.parent_path() != target.parent_path())
        continue;
      if (chosen == commands.end() || candidate < command_file(*chosen))
        chosen = it;
    }
  }
  if (chosen == commands.end())
    throw std::runtime_error(
        "no exact entry or C++ source entry in the same directory: " +
        target.string());
  auto argv = clang::tooling::getClangStripDependencyFileAdjuster()(
      chosen->CommandLine, chosen->Filename);
  argv = clang::tooling::getClangStripOutputAdjuster()(argv, chosen->Filename);
  argv = clang::tooling::getStripPluginsAdjuster()(argv, chosen->Filename);
  if (argv.empty())
    throw std::runtime_error("empty compilation database command");
  std::size_t first = 1;
  const auto executable = fs::path(argv[0]).filename().string();
  if (executable == "ccache" || executable == "sccache" ||
      executable == "distcc")
    first = 2;
  if (first > argv.size())
    throw std::runtime_error("missing compiler after command launcher");
  std::vector<const char *> flags;
  for (std::size_t i = first; i < argv.size(); ++i) {
    if (!argv[i].empty() && argv[i][0] == '@')
      throw std::runtime_error(
          "expand response files before loading compilation database options");
    flags.push_back(argv[i].c_str());
  }
  unsigned missing_index = 0, missing_count = 0;
  const auto parsed =
      getDriverOptTable().ParseArgs(flags, missing_index, missing_count);
  if (missing_count)
    throw std::runtime_error("compilation database flag is missing its value");
  DatabaseProfile result{
      command_file(*chosen).string(),
      {"-working-directory",
       absolute(chosen->Directory, path.parent_path()).string()}};
  for (const auto *argument : parsed) {
    const auto &option = argument->getOption();
    const auto name = option.getName();
    if (name == "working-directory") {
      result.arguments.emplace_back("-working-directory");
      result.arguments.push_back(
          absolute(argument->getValue(),
                   absolute(chosen->Directory, path.parent_path()))
              .string());
      continue;
    }
    if (option.matches(options::OPT_UNKNOWN))
      throw std::runtime_error("unsupported compilation database option: " +
                               argument->getAsString(parsed));
    if (option.matches(options::OPT_INPUT) || option.matches(options::OPT_x) ||
        option.hasFlag(options::LinkOption) || name == "c" || name == "S" ||
        name == "E" || name == "fsyntax-only" || name == "emit-llvm" ||
        name == "save-temps" || name == "save-temps=" ||
        name == "serialize-diagnostics")
      continue;
    llvm::opt::ArgStringList rendered;
    argument->render(parsed, rendered);
    for (const auto *value : rendered)
      result.arguments.emplace_back(value);
  }
  return result;
}
} // namespace tracer_repl

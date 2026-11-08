#pragma once

#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace tracer {

inline bool clang_cl_driver(const std::vector<std::string> &arguments) {
  return std::find(arguments.begin(), arguments.end(), "--driver-mode=cl") != arguments.end();
}

// Apply after validating user options. Clang's CL driver accepts GNU options
// through /clang:, while driver selection and installation paths are parsed
// before those forwarded options.
inline std::vector<std::string> clang_driver_arguments(std::vector<std::string> arguments) {
  if (!clang_cl_driver(arguments))
    return arguments;
  for (auto argument = arguments.begin(); argument != arguments.end(); ++argument) {
    if ((*argument == "-ccc-install-dir" || *argument == "-x") &&
        std::next(argument) != arguments.end()) {
      ++argument;
      continue;
    }
    if (*argument == "-fcxx-exceptions") {
      *argument = "/EHs";
      continue;
    }
    if (*argument != "--driver-mode=cl" && *argument != "--no-default-config" &&
        argument->rfind("--target=", 0) != 0 && *argument != "-fsyntax-only")
      argument->insert(0, "/clang:");
  }
  return arguments;
}

// Only these argument families may reach a compiler running in the attached
// target. In particular, plugin loading and raw driver/frontend forwarding
// are excluded by construction.
inline bool safe_preflight_compiler_argument(std::string_view argument) {
  if (argument.empty() || argument.front() == '@' ||
      argument.rfind("/clang:", 0) == 0 || argument.rfind("-Wp,", 0) == 0 ||
      argument.rfind("-Wa,", 0) == 0 || argument.rfind("-Wl,", 0) == 0)
    return false;
  if (argument == "-I" || argument == "-isystem" ||
      argument == "-iquote" || argument == "-idirafter" ||
      argument == "-include" ||
      argument == "-D" || argument == "-U" || argument == "-std" ||
      argument == "--sysroot" || argument == "--gcc-toolchain")
    return true;
  if (argument.rfind("-I", 0) == 0 || argument.rfind("-D", 0) == 0 ||
      argument.rfind("-U", 0) == 0 || argument.rfind("-isystem", 0) == 0 ||
      argument.rfind("-iquote", 0) == 0 ||
      argument.rfind("-idirafter", 0) == 0 ||
      argument.rfind("-std=", 0) == 0 ||
      argument.rfind("--sysroot=", 0) == 0 ||
      argument.rfind("--gcc-toolchain=", 0) == 0)
    return argument.size() > 2;
  if (argument == "-Wall" || argument == "-w" ||
      argument.rfind("-W", 0) == 0)
    return true;
  if (argument == "-O0" || argument == "-O1" || argument == "-O2" ||
      argument == "-O3" || argument == "-Os" || argument == "-Oz" ||
      argument == "-Og" || argument == "-Ofast" || argument == "-g" ||
      argument == "-g0" || argument == "-g1" || argument == "-g2" ||
      argument == "-g3" || argument.rfind("-ggdb", 0) == 0 ||
      argument == "-gline-tables-only" || argument == "-fno-elide-constructors" ||
      argument == "-fno-access-control" ||
      argument == "-fno-omit-frame-pointer" ||
      argument == "-fomit-frame-pointer" || argument == "-fPIC" ||
      argument == "-fpic" || argument == "-fno-PIC" ||
      argument == "-fno-pic" || argument == "-pthread" ||
      argument == "-fno-stack-protector" ||
      argument == "-fstack-protector" ||
      argument == "-fstack-protector-strong" ||
      argument == "-fstack-protector-all" ||
      argument == "-fstack-protector-explicit" ||
      argument == "-fexceptions" || argument == "-fno-exceptions" ||
      argument == "-frtti" || argument == "-fno-rtti" ||
      argument == "-fvisibility-inlines-hidden" ||
      argument.rfind("-fcf-protection=", 0) == 0 ||
      argument.rfind("-fvisibility=", 0) == 0 ||
      argument == "-fcolor-diagnostics" ||
      argument == "-fno-color-diagnostics" ||
      argument.rfind("-fdiagnostics-color=", 0) == 0)
    return true;
  return false;
}

inline std::string preflight_option_key(std::string_view argument) {
  if (argument.rfind("-D", 0) == 0)
    return "-D";
  if (argument.rfind("-U", 0) == 0)
    return "-U";
  if (argument.rfind("-I", 0) == 0)
    return "-I";
  for (const auto prefix : {"-isystem", "-iquote", "-idirafter", "-include",
                            "-imacros"}) {
    if (argument.rfind(prefix, 0) == 0)
      return prefix;
  }
  const auto equals = argument.find('=');
  if (equals != std::string_view::npos)
    return std::string(argument.substr(0, equals));
  return std::string(argument);
}

template <class Iterator>
inline bool safe_preflight_compiler_arguments(Iterator first, Iterator last,
                                             bool allow_toolchain = false,
                                             std::string *unsupported = nullptr) {
  const auto reject = [&](std::string_view value) {
    if (unsupported)
      *unsupported = preflight_option_key(value);
    return false;
  };
  for (auto current = first; current != last; ++current) {
    const std::string_view argument(*current);
    if (argument == "-I" || argument == "-isystem" ||
        argument == "-iquote" || argument == "-idirafter" ||
        argument == "-include" ||
        argument == "-D" || argument == "-U" || argument == "-std" ||
        argument == "--sysroot" || argument == "--gcc-toolchain") {
      if ((argument == "--sysroot" || argument == "--gcc-toolchain") &&
          !allow_toolchain)
        return reject(argument);
      ++current;
      if (current == last || std::string_view(*current).empty() ||
          std::string_view(*current).front() == '-')
        return reject(argument);
      continue;
    }
    if ((argument.rfind("--sysroot=", 0) == 0 ||
         argument.rfind("--gcc-toolchain=", 0) == 0) &&
        !allow_toolchain)
      return reject(argument);
    if (!safe_preflight_compiler_argument(argument))
      return reject(argument);
  }
  return true;
}

inline bool prepared_callback_include_directory(std::string_view option) {
  return option == "-I" || option == "-isystem" || option == "-iquote" ||
         option == "-idirafter";
}

inline std::string_view skip_compiler_macro_trivia(std::string_view value) {
  for (;;) {
    const auto first = value.find_first_not_of(" \t\v\f");
    if (first == value.npos)
      return {};
    value.remove_prefix(first);
    if (value.rfind("/*", 0) != 0)
      return value;
    const auto end = value.find("*/", 2);
    if (end == value.npos)
      return {};
    value.remove_prefix(end + 2);
  }
}

inline bool prepared_callback_abi_macro(std::string_view value) {
  value = skip_compiler_macro_trivia(value);
  constexpr std::string_view name = "_GLIBCXX_USE_CXX11_ABI";
  if (value.substr(0, name.size()) != name)
    return false;
  if (value.size() == name.size())
    return true;
  return std::string_view(" \t\v\f=(/").find(value[name.size()]) != value.npos;
}

template <class Iterator>
inline bool safe_prepared_callback_compiler_arguments(Iterator first, Iterator last) {
  while (first != last) {
    const std::string_view option(*first++);
    if (first == last)
      return false;
    const std::string_view value(*first++);
    if (value.empty() || value.front() == '-' || value.front() == '@' ||
        value.find_first_of("\0\r\n", 0, 3) != value.npos)
      return false;
    if (prepared_callback_include_directory(option)) {
      if (value.front() != '/' && value.front() != '=' &&
          value.rfind("$SYSROOT", 0) != 0)
        return false;
    } else if (option == "-include") {
      if (value.find('"') != value.npos)
        return false;
    } else if ((option != "-D" && option != "-U") || prepared_callback_abi_macro(value)) {
      return false;
    }
  }
  return true;
}

inline void check_extra_compile_argument_limits(const std::vector<std::string> &arguments) {
  std::size_t bytes = 0;
  if (arguments.size() > 128)
    throw std::invalid_argument("extra compiler arguments exceed limits");
  for (const auto &argument : arguments) {
    if (argument.empty() || argument.find_first_of("\0\r\n", 0, 3) != argument.npos)
      throw std::invalid_argument("invalid extra compiler argument");
    if (argument.size() > 65536 - bytes)
      throw std::invalid_argument("extra compiler arguments exceed limits");
    bytes += argument.size();
  }
}

namespace compiler_options_detail {
inline std::string_view macro_name(std::string_view value) {
  constexpr std::string_view letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_";
  if (value.empty() || letters.find(value.front()) == letters.npos)
    return {};
  return value.substr(0, value.find_first_not_of(
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789"));
}

inline std::string_view unwrap_clang_cl_argument(std::string_view argument) {
  constexpr std::string_view forwarded = "/clang:";
  if (argument.substr(0, forwarded.size()) == forwarded)
    argument.remove_prefix(forwarded.size());
  return argument;
}

inline std::optional<std::string_view> primary_option_value(
    const std::vector<std::string_view> &arguments, std::size_t &index,
    std::initializer_list<std::string_view> prefixes) {
  const auto argument = arguments[index];
  for (auto prefix : prefixes) {
    if (argument.substr(0, prefix.size()) != prefix)
      continue;
    if (argument.size() > prefix.size())
      return argument.substr(prefix.size());
    if (prefix.back() != '=' && index + 1 < arguments.size())
      return arguments[++index];
    return std::string_view{};
  }
  return std::nullopt;
}

inline std::string include_path_identity(const std::filesystem::path &path) {
  std::error_code error;
  const auto canonical = std::filesystem::weakly_canonical(path, error);
  return (error ? path : canonical).lexically_normal().string();
}
} // namespace compiler_options_detail

// Extras have a separate wire field and only permit fallback include directories
// and new object-like macros. Validate them again before compiling in the target.
inline void validate_extra_compile_arguments(
    const std::vector<std::string> &arguments, const std::vector<std::string> &primary,
    const std::filesystem::path &primary_directory = {}) {
  if (arguments.empty())
    return;
  check_extra_compile_argument_limits(arguments);
  using namespace compiler_options_detail;
  std::unordered_set<std::string_view> macros;
  std::unordered_set<std::string> include_paths;
  for (std::string_view argument : arguments) {
    if (argument.rfind("-idirafter/", 0) == 0) {
      include_paths.insert(include_path_identity(argument.substr(sizeof("-idirafter") - 1)));
      continue;
    }
    if (argument.rfind("-D", 0) != 0)
      throw std::invalid_argument("unsupported extra compiler option: " + preflight_option_key(argument));
    auto definition = argument.substr(2);
    const auto name = macro_name(definition);
    if (name.empty() || (definition.size() > name.size() && definition[name.size()] != '='))
      throw std::invalid_argument("extra compiler definitions require an object-like macro name");
    if (name.find("__") != name.npos ||
        (name.size() > 1 && name.front() == '_' && name[1] >= 'A' && name[1] <= 'Z'))
      throw std::invalid_argument("reserved extra compiler macro: " + std::string(name));
    const auto last = definition.find_last_not_of(" \t\v\f");
    if (definition[last] == '\\' || (last >= 2 && definition.substr(last - 2, 3) == "?" "?/"))
      throw std::invalid_argument("extra compiler macro must end on its own line");
    if (!macros.insert(name).second)
      throw std::invalid_argument("duplicate extra compiler macro: " + std::string(name));
  }

  // Clang's CL driver parses the /clang: tokens together as a separate
  // argument sequence, so the operand need not immediately follow the flag.
  std::vector<std::string_view> native, forwarded, primary_includes;
  for (std::string_view argument : primary) {
    const auto unwrapped = unwrap_clang_cl_argument(argument);
    (unwrapped.size() == argument.size() ? native : forwarded).push_back(unwrapped);
  }
  std::filesystem::path sysroot, header_sysroot, working_directory;
  for (const auto* stream : {&native, &forwarded}) {
    for (std::size_t index = 0; index < stream->size(); ++index) {
      const auto argument = (*stream)[index];
      if (const auto macro = primary_option_value(*stream, index,
              {"-D", "-U", "/D", "/U", "--define-macro=", "--undefine-macro=",
               "--define-macro", "--undefine-macro"})) {
        const auto name = macro_name(skip_compiler_macro_trivia(*macro));
        if (!macros.empty() && macro->find_first_of("\r\n") != macro->npos)
          throw std::invalid_argument("cannot add macros to a compiler command with multiline macro options");
        if (macros.count(name))
          throw std::invalid_argument(
              "extra compiler macro conflicts with existing compiler arguments: " + std::string(name));
      } else if (!include_paths.empty()) {
        if (const auto path = primary_option_value(*stream, index, {"--sysroot=", "--sysroot"})) {
          sysroot = *path;
        } else if (const auto path = primary_option_value(*stream, index, {"-isysroot=", "-isysroot"})) {
          header_sysroot = *path;
        } else if (const auto path = primary_option_value(*stream, index,
                       {"-working-directory=", "-working-directory"})) {
          working_directory = *path;
        } else if (const auto path = primary_option_value(*stream, index,
                       {"-I", "-isystem-after", "-isystem", "-iquote", "-idirafter", "-cxx-isystem",
                        "-stdlib++-isystem", "-iframeworkwithsysroot", "-iframework", "-F", "/I",
                        "-imsvc", "/imsvc", "-external:I", "/external:I", "--include-directory-after=",
                        "--include-directory-after", "--include-directory=", "--include-directory"})) {
          primary_includes.push_back(*path);
        } else if (argument.rfind("-iwithprefix", 0) == 0 ||
                   argument.rfind("--include-with-prefix", 0) == 0) {
          throw std::invalid_argument(
              "cannot add include paths to a compiler command that uses -iwithprefix");
        }
      }
    }
  }
  if (!header_sysroot.empty())
    sysroot = header_sysroot;
  for (auto path : primary_includes) {
    if (path.empty())
      continue;
    std::filesystem::path primary_path;
    if (path.front() == '=' || path.rfind("$SYSROOT", 0) == 0) {
      if (sysroot.empty())
        throw std::invalid_argument(
            "cannot add include paths to a compiler command with an unresolved sysroot include");
      path.remove_prefix(path.front() == '=' ? 1 : sizeof("$SYSROOT") - 1);
      primary_path = sysroot / std::filesystem::path(path).relative_path();
    } else {
      primary_path = path;
    }
    if (include_paths.count(include_path_identity(
            std::filesystem::absolute(primary_directory / working_directory / primary_path))))
      throw std::invalid_argument(
          "extra compiler include duplicates an existing compiler include: " + primary_path.string());
  }
}

inline std::vector<std::string> normalize_extra_compile_arguments(
    const std::vector<std::string> &extra, const std::vector<std::string> &primary,
    const std::filesystem::path &primary_directory = {},
    const std::filesystem::path &extra_directory = {}) {
  check_extra_compile_argument_limits(extra);
  std::vector<std::string> normalized;
  for (std::size_t index = 0; index < extra.size(); ++index) {
    const auto argument = std::string_view(extra[index]);
    auto option = preflight_option_key(argument);
    if (argument.rfind("/I", 0) == 0)
      option = "-I";
    else if (argument.rfind("/D", 0) == 0)
      option = "-D";
    if (option != "-D" && option != "-I" && option != "-isystem" && option != "-idirafter")
      throw std::invalid_argument("unsupported extra compiler option: " + option);
    auto value = argument.substr(option.size());
    if (value.empty()) {
      if (++index == extra.size())
        throw std::invalid_argument("missing value for extra compiler option: " + option);
      value = extra[index];
    }
    if (option == "-D") {
      normalized.push_back("-D" + std::string(value));
    } else {
      if (value.front() == '-' || value.front() == '@' || value.front() == '=' ||
          value.rfind("$SYSROOT", 0) == 0)
        throw std::invalid_argument("invalid include directory for extra compiler option: " + option);
      normalized.push_back("-idirafter" +
                           std::filesystem::absolute(extra_directory / value).string());
    }
  }
  validate_extra_compile_arguments(normalized, primary, primary_directory);
  return normalized;
}

} // namespace tracer

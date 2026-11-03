#pragma once

#include <string_view>
#include <string>

namespace tracer {

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

} // namespace tracer

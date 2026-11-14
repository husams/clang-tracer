#pragma once
#include "tracer/compat.hpp"

#include "tracer.pb.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace tracer::prepared {

inline constexpr unsigned legacy_typed_version = 4;
inline constexpr unsigned typed_version = 5;
inline constexpr unsigned max_replay_type_bytes = 64 * 1024;
inline bool is_typed_version(unsigned version) {
  return version == legacy_typed_version || version == typed_version;
}

inline bool valid_include_name(std::string_view name) {
  if (name.size() > 512 || name.find_first_not_of(
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_./+-") != name.npos)
    return false;
  while (true) {
    const auto slash = name.find('/');
    const auto component = name.substr(0, slash);
    if (component.empty() || component == "." || component == "..")
      return false;
    if (slash == name.npos)
      return true;
    name.remove_prefix(slash + 1);
  }
}

inline bool toolchain_path(std::string_view path) {
  return !path.empty() && path.front() == '/' &&
         path.find_first_of("\r\n") == path.npos && path.find('\0') == path.npos;
}

inline std::string_view toolchain_path_option(std::string_view option) {
  for (const std::string_view prefix : {"--sysroot=", "--gcc-toolchain=",
                                      "--gcc-install-dir=", "-isysroot"})
    if (option.substr(0, prefix.size()) == prefix)
      return prefix;
  return {};
}

inline bool toolchain_switch(std::string_view option) {
  return option == "-stdlib=libstdc++" || option == "-stdlib=libc++" ||
         option == "-nostdinc" || option == "-nostdinc++" || option == "-nostdlibinc" ||
         option == "-nobuiltininc" || option == "-ibuiltininc";
}

inline bool supported_driver_mode(std::string_view mode) {
  return mode == "gcc" || mode == "g++" || mode == "cl";
}

inline std::vector<std::string> toolchain_arguments(
    const clangtracer::v1::PreparedInjector &package, std::vector<std::string> defaults) {
  const auto &options = package.adapter_toolchain_options();
  if (options.empty() && !package.adapter_uses_default_toolchain())
    return defaults;
  if (package.format_version() != typed_version || options.empty() || options.size() > 32 ||
      options.Get(0) != "--no-default-config")
    throw std::invalid_argument("invalid prepared toolchain profile");
  bool has_driver_mode = false;
  for (int index = 1; index < options.size(); ++index) {
    const std::string_view option = options.Get(index);
    constexpr std::string_view driver_prefix = "--driver-mode=";
    if (option.rfind(driver_prefix, 0) == 0) {
      if (has_driver_mode || !supported_driver_mode(option.substr(driver_prefix.size())))
        throw std::invalid_argument("invalid prepared driver mode");
      has_driver_mode = true;
      continue;
    }
    const auto prefix = toolchain_path_option(option);
    if (option.size() <= 4096 && !prefix.empty() && toolchain_path(option.substr(prefix.size())))
      continue;
    if (option == "-ccc-install-dir" && index + 1 < options.size() &&
        options.Get(index + 1).size() <= 4096 && toolchain_path(options.Get(index + 1))) {
      ++index;
      continue;
    }
    if (toolchain_switch(option))
      continue;
    const auto target = option.substr(std::min(option.size(), std::string_view("--target=").size()));
    const bool aarch64_snapshot = !package.receiver_storage().empty() &&
        ::tracer::compat::starts_with(package.target_triple(), "aarch64-") && ::tracer::compat::starts_with(target, "aarch64-");
    if (option.substr(0, 9) == "--target=" && (::tracer::compat::starts_with(target, "x86_64-") || aarch64_snapshot) &&
        target.find("-linux") != target.npos && target.size() <= 128 &&
        target.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-") == target.npos)
      continue;
    throw std::invalid_argument("unsupported prepared toolchain option");
  }
  std::vector<std::string> result{"--no-default-config"};
  if (package.adapter_uses_default_toolchain())
    result.insert(result.end(), defaults.begin(), defaults.end());
  result.insert(result.end(), std::next(options.begin()), options.end());
  return result;
}

inline bool abi_option(std::string_view option) {
  return option == "-std=c++17" || option == "-std=gnu++17" ||
         option == "-std=c++20" || option == "-std=gnu++20" ||
         option == "-std=c++23" || option == "-std=gnu++23" ||
         option == "-D_GLIBCXX_USE_CXX11_ABI=0" ||
         option == "-D_GLIBCXX_USE_CXX11_ABI=1" ||
         option == "-fshort-enums" || option == "-fshort-wchar";
}

inline std::string adapter_source(const clangtracer::v1::PreparedInjector &package) {
  const auto &declarations = package.adapter_declarations();
  if (declarations.empty() ||
      declarations.find_first_of("#\\\r") != std::string::npos ||
      declarations.find('\0') != std::string::npos ||
      declarations.find("%:") != std::string::npos)
    throw std::invalid_argument("invalid prepared adapter declarations");
  std::string source;
  (void)toolchain_arguments(package, {});
  for (const auto &header : package.adapter_standard_headers()) {
    if (!valid_include_name(header))
      throw std::invalid_argument("invalid prepared toolchain header name");
    source += "#include <" + header + ">\n";
  }
  for (const auto &option : package.adapter_abi_options())
    if (!abi_option(option))
      throw std::invalid_argument("unsupported prepared adapter ABI option");
  return source + declarations;
}

} // namespace tracer::prepared

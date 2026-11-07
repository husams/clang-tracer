#pragma once

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace tracer::proc {

inline std::uint64_t start_time(const std::filesystem::path &stat_file) {
  std::ifstream input(stat_file);
  std::string line;
  // comm can contain spaces, parentheses and line breaks.
  if (!std::getline(input, line, '\0'))
    throw std::runtime_error("target process is no longer available");
  const auto close = line.rfind(')');
  if (close == std::string::npos || close + 2 >= line.size())
    throw std::runtime_error("cannot read target process identity");
  std::istringstream fields(line.substr(close + 2));
  std::string field;
  // The first token is field 3 (state); starttime is field 22.
  for (unsigned number = 3; number <= 22; ++number) {
    if (!(fields >> field))
      throw std::runtime_error("cannot read target process identity");
  }
  std::uint64_t ticks = 0;
  const auto end = field.data() + field.size();
  const auto parsed = std::from_chars(field.data(), end, ticks);
  if (parsed.ec != std::errc{} || parsed.ptr != end || !ticks)
    throw std::runtime_error("cannot read target process identity");
  return ticks;
}

inline std::filesystem::path directory(std::uint32_t pid) {
  return "/proc/" + std::to_string(pid);
}

} // namespace tracer::proc

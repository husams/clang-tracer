#pragma once

#include <cstddef>

// Verbosity of the injected agent's diagnostics log. This is independent of the
// public callback tracer::LogLevel, which only classifies callback messages.
namespace tracer::agent_log {

// A higher value is more verbose. A record is written when its level is not
// above the configured threshold.
enum class Level : int { Error = 0, Warning = 1, Info = 2, Debug = 3, Trace = 4 };

inline constexpr Level kDefaultLevel = Level::Info;

struct ParsedLevel {
  Level level = kDefaultLevel;
  // False when the text named no level. The level is then the default.
  bool valid = true;
};

constexpr const char *level_name(Level level) noexcept {
  switch (level) {
  case Level::Error:
    return "error";
  case Level::Warning:
    return "warning";
  case Level::Info:
    return "info";
  case Level::Debug:
    return "debug";
  case Level::Trace:
    return "trace";
  }
  return "info";
}

namespace detail {
// ASCII-only comparison keeps the result independent of the locale. The
// expected name is lower case.
constexpr bool equals_ignore_case(const char *text, const char *name) noexcept {
  for (; *name; ++text, ++name) {
    char c = *text;
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
    if (c != *name)
      return false;
  }
  return *text == '\0';
}
} // namespace detail

// Parses CLANG_TRACER_AGENT_LOG_LEVEL. Null and empty text mean the default
// level and are valid. Unrecognized text selects the default and is invalid.
constexpr ParsedLevel parse_level(const char *text) noexcept {
  if (!text || !*text)
    return {kDefaultLevel, true};
  struct Name {
    const char *text;
    Level level;
  };
  constexpr Name names[] = {{"error", Level::Error},   {"warning", Level::Warning},
                            {"warn", Level::Warning},  {"info", Level::Info},
                            {"debug", Level::Debug},   {"trace", Level::Trace}};
  for (const auto &name : names)
    if (detail::equals_ignore_case(text, name.text))
      return {name.level, true};
  return {kDefaultLevel, false};
}

} // namespace tracer::agent_log

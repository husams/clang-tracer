#include <cstdlib>
#include <iostream>

#include "agent_log_level.hpp"

namespace {
using tracer::agent_log::Level;
using tracer::agent_log::parse_level;

int failures = 0;

void expect(const char *text, Level level, bool valid) {
  const auto parsed = parse_level(text);
  if (parsed.level != level || parsed.valid != valid) {
    std::cerr << "parse_level(" << (text ? text : "<null>") << ") returned "
              << tracer::agent_log::level_name(parsed.level)
              << (parsed.valid ? "" : " (invalid)") << '\n';
    ++failures;
  }
}
} // namespace

int main() {
  expect(nullptr, Level::Info, true);
  expect("", Level::Info, true);
  expect("error", Level::Error, true);
  expect("warning", Level::Warning, true);
  expect("warn", Level::Warning, true);
  expect("info", Level::Info, true);
  expect("debug", Level::Debug, true);
  expect("trace", Level::Trace, true);
  expect("ERROR", Level::Error, true);
  expect("Warn", Level::Warning, true);
  expect("TrAcE", Level::Trace, true);
  expect("verbose", Level::Info, false);
  expect("debugx", Level::Info, false);
  expect("deb", Level::Info, false);
  expect(" info", Level::Info, false);
  expect("info ", Level::Info, false);
  expect("0", Level::Info, false);

  static_assert(Level::Error < Level::Warning && Level::Warning < Level::Info &&
                Level::Info < Level::Debug && Level::Debug < Level::Trace);
  static_assert(parse_level("trace").level == Level::Trace);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tracer_repl {
enum class CommandKind {
  server_start,
  server_connect,
  server_status,
  server_stop,
  attach,
  processes,
  sessions,
  use,
  status,
  detach,
  modules,
  symbols,
  resolve,
  inject,
  validate,
  probes,
  remove,
  draft_load,
  draft_edit,
  draft_show,
  draft_upload,
  events,
  event_list,
  event_show,
  event_calls,
  event_stats,
  event_clear,
  event_open,
  event_export,
  collect,
  record,
  cpp_load,
  cpp_reset,
  complete,
  help,
  quit
};

struct Command {
  CommandKind kind = CommandKind::help;
  std::vector<std::string> arguments;
  std::map<std::string, std::vector<std::string>> options;
  bool has(const std::string &name) const;
  std::string option(const std::string &name,
                     const std::string &fallback = {}) const;
  std::uint64_t number(const std::string &name, std::uint64_t fallback) const;
};

Command parse_command(const std::string &input);
std::uint64_t unsigned_number(const std::string &text, const std::string &name,
                              std::uint64_t maximum = UINT64_MAX);
const std::vector<std::string> &command_names();
} // namespace tracer_repl

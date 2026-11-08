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
  launch,
  children,
  resume,
  terminate,
  target_call,
  target_call_result,
  syscalls,
  sample_cpu,
  sample_allocations,
  faults,
  watchpoint,
  instructions_start,
  instructions_stop,
  instructions_replay,
  library_load,
  processes,
  sessions,
  use,
  status,
  detach,
  modules,
  symbols,
  memory_maps,
  memory_read,
  memory_scan,
  memory_patch,
  memory_write,
  memory_allocate,
  memory_free,
  memory_restore,
  memory_import_hook,
  page_monitor_start,
  page_monitor_stop,
  probe_entry,
  probe_reload,
  prepare,
  resolve,
  inject,
  validate,
  probes,
  remove,
  draft_load,
  draft_edit,
  draft_show,
  draft_validate,
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
  cpp_import,
  cpp_generate,
  cpp_options,
  shell,
  complete,
  type,
  help,
  quit,
  threads,
  bulk_preview,
  bulk_apply,
  bulk_remove
};

struct Command {
  CommandKind kind = CommandKind::help;
  std::vector<std::string> arguments;
  std::vector<std::size_t> shell_arguments;
  std::map<std::string, std::vector<std::string>> options;
  bool has(const std::string &name) const;
  std::string option(const std::string &name,
                     const std::string &fallback = {}) const;
  std::uint64_t number(const std::string &name, std::uint64_t fallback) const;
};

Command parse_command(const std::string &input);
std::uint64_t unsigned_number(const std::string &text, const std::string &name,
                              std::uint64_t maximum = UINT64_MAX);
std::uint64_t address_number(const std::string &text);
const std::vector<std::string> &command_names();
} // namespace tracer_repl

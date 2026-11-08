#include "commands.hpp"
#include "local_perf.hpp"
#include "tracer/instruction_trace.h"

#include <charconv>
#include <limits>
#include <set>
#include <stdexcept>

#include "TracerCommandsBaseVisitor.h"
#include "TracerCommandsLexer.h"
#include "TracerCommandsParser.h"

namespace tracer_repl {
namespace {
class Errors final : public antlr4::BaseErrorListener {
public:
  std::string message;

  void syntaxError(antlr4::Recognizer *, antlr4::Token *, std::size_t,
                   std::size_t column, const std::string &text,
                   std::exception_ptr) override {
    if (message.empty()) {
      message = "command column " + std::to_string(column + 1) + ": " + text;
    }
  }
};

std::string decode(const std::string &s) {
  if (s.empty() || (s.front() != '\'' && s.front() != '"')) {
    return s;
  }
  std::string out;
  for (std::size_t i = 1; i + 1 < s.size(); ++i) {
    char c = s[i];
    if (c == '\\') {
      c = s[++i];
      if (c == 'n') {
        c = '\n';
      } else if (c == 'r') {
        c = '\r';
      } else if (c == 't') {
        c = '\t';
      }
    }
    out += c;
  }
  return out;
}

using K = CommandKind;
const std::map<std::string, K> kinds = {{"server start", K::server_start},
                                        {"server connect", K::server_connect},
                                        {"server status", K::server_status},
                                        {"server stop", K::server_stop},
                                        {"attach", K::attach},
                                        {"launch", K::launch},
                                        {"children", K::children},
                                        {"resume", K::resume},
                                        {"terminate", K::terminate},
                                        {"call", K::target_call},
                                        {"call-result", K::target_call_result},
                                        {"syscalls", K::syscalls},
                                        {"sample cpu", K::sample_cpu},
                                        {"sample allocations", K::sample_allocations},
                                        {"faults", K::faults},
                                        {"watchpoint", K::watchpoint},
                                        {"instructions start", K::instructions_start},
                                        {"instructions stop", K::instructions_stop},
                                        {"instructions replay", K::instructions_replay},
                                        {"library load", K::library_load},
                                        {"processes", K::processes},
                                        {"sessions", K::sessions},
                                        {"use", K::use},
                                        {"status", K::status},
                                        {"detach", K::detach},
                                        {"modules", K::modules},
                                        {"threads", K::threads},
                                        {"symbols", K::symbols},
                                        {"memory maps", K::memory_maps},
                                        {"memory read", K::memory_read},
                                        {"memory scan", K::memory_scan},
                                        {"memory patch", K::memory_patch},
                                        {"memory write", K::memory_write},
                                        {"memory allocate", K::memory_allocate},
                                        {"memory free", K::memory_free},
                                        {"memory restore", K::memory_restore},
                                        {"memory import-hook", K::memory_import_hook},
                                        {"memory monitor-start", K::page_monitor_start},
                                        {"memory monitor-stop", K::page_monitor_stop},
                                        {"trace preview", K::bulk_preview},
                                        {"trace apply", K::bulk_apply},
                                        {"trace remove", K::bulk_remove},
                                        {"probe prepare", K::prepare},
                                        {"probe entry", K::probe_entry},
                                        {"probe reload", K::probe_reload},
                                        {"probe resolve", K::resolve},
                                        {"probe inject", K::inject},
                                        {"probe validate", K::validate},
                                        {"probes", K::probes},
                                        {"probe remove", K::remove},
                                        {"interceptor load", K::draft_load},
                                        {"interceptor edit", K::draft_edit},
                                        {"interceptor show", K::draft_show},
                                        {"interceptor validate", K::draft_validate},
                                        {"interceptor upload", K::draft_upload},
                                        {"events", K::events},
                                        {"events list", K::event_list},
                                        {"events show", K::event_show},
                                        {"events calls", K::event_calls},
                                        {"events stats", K::event_stats},
                                        {"events clear", K::event_clear},
                                        {"events open", K::event_open},
                                        {"events export", K::event_export},
                                        {"collect", K::collect},
                                        {"record", K::record},
                                        {"cpp load", K::cpp_load},
                                        {"cpp reset", K::cpp_reset},
                                        {"cpp import", K::cpp_import},
                                        {"cpp generate", K::cpp_generate},
                                        {"cpp options", K::cpp_options},
                                        {"shell", K::shell},
                                        {"complete", K::complete},
                                        {"type", K::type},
                                        {"help", K::help},
                                        {"quit", K::quit},
                                        {"exit", K::quit}};

// This visitor runs only after the generated lexer/parser accepted the entire
// input. It translates tokens into data; it never performs tracing operations.
class Builder final : public TracerCommandsBaseVisitor {
  antlr4::CommonTokenStream &tokens_;

public:
  Command result;

  explicit Builder(antlr4::CommonTokenStream &tokens) : tokens_(tokens) {}

  std::any visitLine(TracerCommandsParser::LineContext *) override {
    const auto ts = tokens_.getTokens();
    std::size_t i = 1;
    std::string name = ts.at(i++)->getText();
    if (i + 1 < ts.size() && kinds.count(name + " " + ts[i]->getText())) {
      name += " " + ts[i++]->getText();
    }
    result.kind = kinds.at(name);
    if (result.kind == K::help || result.kind == K::type) {
      if (ts[i]->getText() == "%")
        ++i;
      for (; i + 1 < ts.size(); ++i)
        result.arguments.push_back(decode(ts[i]->getText()));
      return {};
    }
    const std::set<std::string> flags = {
        "--all", "--metadata-only", "--print-arguments", "--print-result",
        "--details", "--addresses", "--sqlite", "--allow-arg-overrides", "--allow-result-override",
        "--registers", "--discover", "--watch", "--supervise", "--gate-children", "--allow-replacement", "--allow-call-original",
        "--trace-exceptions", "--reclaimable"};
    const std::set<std::string> repeated = {"--include", "--compile-option",
                                           "--name", "--type", "--arg", "--env"};
    const std::set<std::string> bulk_repeated = {"--module", "--exclude", "--exact"};
    for (; i + 1 < ts.size(); ++i) {
      const auto text = ts[i]->getText();
      const bool profile_add = result.kind == K::cpp_options &&
                               !result.arguments.empty() && result.arguments.front() == "add";
      if (!profile_add && ts[i]->getType() != TracerCommandsLexer::STRING &&
          text.rfind("--", 0) == 0) {
        const bool bulk_repeat = result.kind == K::bulk_preview && bulk_repeated.count(text);
        if (result.has(text) && !repeated.count(text) && !bulk_repeat) {
          throw std::invalid_argument("duplicate option: " + text);
        }
        result.options[text].push_back(
            flags.count(text) ? "true" : decode(ts[++i]->getText()));
      } else {
        if (ts[i]->getType() == TracerCommandsLexer::SHELL) {
          result.shell_arguments.push_back(result.arguments.size());
          result.arguments.push_back(text.substr(1, text.size() - 2));
        } else result.arguments.push_back(decode(text));
      }
    }
    return {};
  }
};
} // namespace

bool Command::has(const std::string &name) const {
  return options.count(name) != 0;
}

std::string Command::option(const std::string &name,
                            const std::string &fallback) const {
  const auto it = options.find(name);
  return it == options.end() ? fallback : it->second.front();
}

std::uint64_t unsigned_number(const std::string &text, const std::string &name,
                              std::uint64_t maximum) {
  std::uint64_t n = 0;
  const auto r = std::from_chars(text.data(), text.data() + text.size(), n);
  if (text.empty() || r.ec != std::errc{} ||
      r.ptr != text.data() + text.size() || n > maximum) {
    throw std::invalid_argument("invalid " + name + ": " + text);
  }
  return n;
}

std::uint64_t Command::number(const std::string &name,
                              std::uint64_t fallback) const {
  return has(name) ? unsigned_number(option(name), name) : fallback;
}

std::uint64_t address_number(const std::string &text) {
  const bool hex = text.size() >= 2 && text[0] == '0' &&
                   (text[1] == 'x' || text[1] == 'X');
  const auto begin = text.data() + (hex ? 2 : 0);
  const auto end = text.data() + text.size();
  std::uint64_t address = 0;
  const auto parsed = std::from_chars(begin, end, address, hex ? 16 : 10);
  if (parsed.ec != std::errc{} || parsed.ptr != end || !address)
    throw std::invalid_argument("invalid nonzero address: " + text);
  return address;
}

Command parse_command(const std::string &input) {
  if (input.size() > 65536 || input.find('\0') != std::string::npos) {
    throw std::invalid_argument("command is too large or contains a null byte");
  }
  antlr4::ANTLRInputStream stream(input);
  TracerCommandsLexer lexer(&stream);
  Errors errors;
  lexer.removeErrorListeners();
  lexer.addErrorListener(&errors);
  antlr4::CommonTokenStream tokens(&lexer);
  TracerCommandsParser parser(&tokens);
  parser.removeErrorListeners();
  parser.addErrorListener(&errors);
  auto *tree = parser.line();
  if (!errors.message.empty()) {
    throw std::invalid_argument(errors.message);
  }
  Builder visitor(tokens);
  visitor.visit(tree);
  auto command = std::move(visitor.result);
  if (command.kind == K::bulk_preview) {
    const bool has_rules = command.has("--include") || command.has("--exact");
    if (command.has("--manifest")) {
      if (has_rules || command.has("--module") || command.has("--exclude"))
        throw std::invalid_argument("--manifest cannot be combined with selection rules");
    } else if (!has_rules) {
      throw std::invalid_argument("%trace preview requires --include, --exact or --manifest");
    }
    for (const auto &[option, values] : command.options)
      for (const auto &value : values)
        if (value.empty())
          throw std::invalid_argument(option + " requires a nonempty value");
  }
  if (command.kind == K::prepare) {
    for (const auto* option : {"--include", "--output"})
      if (!command.has(option) || command.option(option).empty())
        throw std::invalid_argument(std::string("%probe prepare requires ") + option);
    for (const auto* option : {"--compdb", "--translation-unit", "--module"})
      if (command.has(option) && command.option(option).empty())
        throw std::invalid_argument(std::string(option) + " requires a path");
  }
  const auto mode = command.option("--prepared-mode", "prefer");
  if (mode != "off" && mode != "prefer" && mode != "require")
    throw std::invalid_argument("--prepared-mode must be off, prefer or require");
  if (mode != "off" && command.has("--prepared") && command.option("--prepared").empty())
    throw std::invalid_argument("--prepared requires a directory");
  for (const auto &key : {"--count", "--timeout", "--after", "--probe",
                          "--index", "--limit", "--stack-depth"}) {
    if (command.has(key)) {
      command.number(key, 0);
    }
  }
  if (command.number("--limit", 0) > UINT32_MAX) {
    throw std::invalid_argument("--limit must fit a 32-bit page size");
  }
  if (command.kind == K::launch) {
    if (command.arguments[0].empty())
      throw std::invalid_argument("%launch requires an executable path");
    for (const auto *key : {"--cwd", "--stdin", "--stdout", "--stderr", "--preload"})
      if (command.has(key) && command.option(key).empty())
        throw std::invalid_argument(std::string(key) + " requires a path");
  }
  if ((command.kind == K::attach || command.kind == K::resume ||
       command.kind == K::terminate) && !command.arguments.empty()) {
    if (!unsigned_number(command.arguments[0], "PID", INT32_MAX)) {
      throw std::invalid_argument("PID must be positive");
    }
    if (command.has("--start-time") && !command.number("--start-time", 0))
      throw std::invalid_argument("--start-time must be positive");
  }
  if (command.has("--address")) {
    address_number(command.option("--address"));
    if (command.option("--mapping").empty() || command.has("--page") ||
        command.number("--limit", 0))
      throw std::invalid_argument("--address requires --mapping and does not support paging");
  }
  if (command.has("--mapping") && command.option("--mapping").empty()) {
    throw std::invalid_argument("--mapping requires a snapshot token");
  }
  if (command.kind == K::memory_read || command.kind == K::memory_scan)
    address_number(command.arguments.at(0));
  if ((command.kind == K::remove || command.kind == K::probe_reload) &&
      !unsigned_number(command.arguments[0], "probe ID")) {
    throw std::invalid_argument("probe ID must be positive");
  }
  for (const auto &key : {"--count", "--timeout"}) {
    if (command.has(key) && command.number(key, 0) == 0) {
      throw std::invalid_argument(std::string(key) + " must be positive");
    }
  }
  if (command.number("--timeout", 0) > 86400) {
    throw std::invalid_argument("--timeout must be at most 86400 seconds");
  }
  if ((command.kind == K::modules || command.kind == K::probe_reload) && command.has("--watch") &&
      (command.number("--timeout", 0) > 60 || command.number("--count", 0) > 256))
    throw std::invalid_argument("watch is capped at 60 seconds and 256 changes");
  if (command.kind == K::modules && command.has("--elf") && command.option("--elf").empty())
    throw std::invalid_argument("--elf requires an exact mapped path");
  if (command.kind == K::syscalls || command.kind == K::sample_cpu ||
      command.kind == K::sample_allocations || command.kind == K::faults ||
      command.kind == K::watchpoint) {
    if (command.number("--timeout", 0) > max_local_perf_seconds)
      throw std::invalid_argument("local perf capture timeout is capped at 60 seconds");
    if (command.number("--count", 0) > max_local_perf_events)
      throw std::invalid_argument("local perf capture is capped at 8192 events");
  }
  if (command.kind == K::instructions_start || command.kind == K::watchpoint) {
    if (!command.has("--address") || command.option("--mapping").empty() ||
        !unsigned_number(command.option("--tid"), "thread ID", INT32_MAX) ||
        !unsigned_number(command.option("--size"), "instruction range size", TRACER_INSTRUCTION_MAX_BYTES))
      throw std::invalid_argument("capture requires --tid, --address, --size and --mapping");
  }
  if (command.kind == K::watchpoint) {
    const auto size = command.number("--size", 0);
    if ((size != 1 && size != 2 && size != 4 && size != 8) ||
        address_number(command.option("--address")) % size != 0)
      throw std::invalid_argument("watchpoints require a naturally aligned size of 1, 2, 4 or 8 bytes");
    const auto access = command.option("--access");
    if (access != "write" && access != "read-write")
      throw std::invalid_argument("watchpoint --access must be write or read-write");
  }
  if (command.kind == K::instructions_start) {
    if (command.number("--timeout", 0) > TRACER_INSTRUCTION_MAX_SECONDS ||
        command.number("--count", 0) > TRACER_INSTRUCTION_MAX_RECORDS)
      throw std::invalid_argument("instruction capture is capped at 60 seconds and 8192 records");
    if (command.has("--memory-address") != command.has("--memory-size"))
      throw std::invalid_argument("memory monitoring requires both --memory-address and --memory-size");
    if (command.has("--memory-address")) {
      address_number(command.option("--memory-address"));
      if (!unsigned_number(command.option("--memory-size"), "monitored memory size",
                           TRACER_INSTRUCTION_MAX_BYTES))
        throw std::invalid_argument("monitored memory size must be positive");
    }
    if (command.has("--callback-library") && command.option("--callback-library").empty())
      throw std::invalid_argument("--callback-library requires an absolute shared library path");
  }
  if (command.number("--stack-depth", 0) > 64) {
    throw std::invalid_argument("--stack-depth must be at most 64");
  }
  if (command.has("--probe") && !command.number("--probe", 0)) {
    throw std::invalid_argument("probe ID must be positive");
  }
  if (command.has("--all") && command.has("--session")) {
    throw std::invalid_argument("--all conflicts with --session");
  }
  if (command.kind == K::record && command.has("--as")) {
    throw std::invalid_argument("--as is for in-memory collections");
  }
  if (command.kind == K::record && command.arguments.at(0).empty()) {
    throw std::invalid_argument("recording path must not be empty");
  }
  return command;
}

const std::vector<std::string> &command_names() {
  static const auto names = [] {
    std::vector<std::string> result;
    for (const auto &entry : kinds) {
      result.push_back("%" + entry.first);
    }
    return result;
  }();
  return names;
}
} // namespace tracer_repl

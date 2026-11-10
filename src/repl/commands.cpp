#include "commands.hpp"

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
                                        {"processes", K::processes},
                                        {"sessions", K::sessions},
                                        {"use", K::use},
                                        {"status", K::status},
                                        {"detach", K::detach},
                                        {"modules", K::modules},
                                        {"module index", K::module_index},
                                        {"module use", K::module_use},
                                        {"module types", K::module_types},
                                        {"module symbols", K::module_symbols},
                                        {"module show", K::module_show},
                                        {"module emit", K::module_emit},
                                        {"threads", K::threads},
                                        {"symbols", K::symbols},
                                        {"memory maps", K::memory_maps},
                                        {"memory read", K::memory_read},
                                        {"memory scan", K::memory_scan},
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
                                        {"events stack", K::event_stack},
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
  std::map<std::size_t, std::size_t> extra_compile_options_;

  void add_extra_flag_list(antlr4::Token *first, antlr4::Token *last) {
    // Keep quotes and spacing in unquoted groups for the argv splitter.
    const auto span = antlr4::misc::Interval(first->getStartIndex(), last->getStopIndex());
    const bool quoted_list = first == last && first->getType() == TracerCommandsLexer::STRING;
    const auto text = quoted_list ? decode(first->getText())
                                  : first->getInputStream()->getText(span);
    result.extra_compile_argument_inputs.push_back(
        {ExtraCompileArgumentInput::Kind::list, text});
  }

  void add_extra_compile_option(const std::vector<antlr4::Token *> &tokens,
                                std::size_t option, std::size_t last) {
    auto first = option + 1;
    if (tokens[option]->getText() == "--extra-arg") {
      result.extra_compile_argument_inputs.push_back(
          {ExtraCompileArgumentInput::Kind::argument, decode(tokens[first]->getText())});
      return;
    }
    for (auto i = first; i <= last; ++i) {
      if (tokens[i]->getType() != TracerCommandsLexer::SHELL)
        continue;
      if (i != first)
        add_extra_flag_list(tokens[first], tokens[i - 1]);
      const auto text = tokens[i]->getText();
      result.extra_compile_argument_inputs.push_back(
          {ExtraCompileArgumentInput::Kind::shell, text.substr(1, text.size() - 2)});
      first = i + 1;
    }
    if (first <= last)
      add_extra_flag_list(tokens[first], tokens[last]);
  }

public:
  Command result;

  explicit Builder(antlr4::CommonTokenStream &tokens) : tokens_(tokens) {}

  std::any visitExtraCompileOption(
      TracerCommandsParser::ExtraCompileOptionContext *option) override {
    extra_compile_options_.emplace(option->getStart()->getTokenIndex(),
                                   option->getStop()->getTokenIndex());
    return {};
  }

  std::any visitLine(TracerCommandsParser::LineContext *line) override {
    visitChildren(line);
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
        "--snapshot-return-pointee",
        "--details", "--addresses", "--sqlite", "--member", "--exported"};
    const std::set<std::string> repeated = {"--include", "--compile-option",
                                           "--name", "--type"};
    const std::set<std::string> bulk_repeated = {"--module", "--exclude", "--exact"};
    for (; i + 1 < ts.size(); ++i) {
      if (const auto extra = extra_compile_options_.find(i);
          extra != extra_compile_options_.end()) {
        add_extra_compile_option(ts, i, extra->second);
        i = extra->second;
        continue;
      }
      const auto text = ts[i]->getText();
      const bool profile_add = result.kind == K::cpp_options &&
                               !result.arguments.empty() && result.arguments.front() == "add";
      if (!profile_add && ts[i]->getType() != TracerCommandsLexer::STRING &&
          text.rfind("--", 0) == 0) {
        const bool bulk_repeat = result.kind == K::bulk_preview && bulk_repeated.count(text);
        const bool module_repeat = result.kind == K::module_emit &&
                                   (text == "--symbol" || text == "--id");
        if (result.has(text) && !repeated.count(text) && !bulk_repeat && !module_repeat) {
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
  if (!has(name))
    return fallback;
  const auto value = option(name);
  if (name == "--stack-depth" && value == "full")
    return UINT32_MAX;
  return unsigned_number(value, name);
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
    const bool elf_mode = command.has("--signature");
    const bool source_mode = command.has("--include");
    if (command.option("--output").empty())
      throw std::invalid_argument("%probe prepare requires --output");
    if (elf_mode && (command.option("--signature").empty() || command.option("--module").empty() ||
                     source_mode || command.has("--compdb") || command.has("--translation-unit")))
      throw std::invalid_argument("--signature requires a C++ type, --module and no source options");
    if (!elf_mode && command.has("--member"))
      throw std::invalid_argument("--member requires --signature");
    if ((source_mode || elf_mode) &&
        (command.has("--module-index") || command.has("--generated") || command.has("--id")))
      throw std::invalid_argument("indexed preparation options cannot be combined with headers or a signature");
    if (!elf_mode && !source_mode &&
        (command.has("--compdb") || command.has("--translation-unit") ||
         !command.extra_compile_argument_inputs.empty()))
      throw std::invalid_argument("source preparation requires --include");
    for (const auto *option : {"--include", "--compdb", "--translation-unit", "--module",
                               "--module-index", "--generated", "--id", "--snapshot-return-type"})
      if (command.has(option) && command.option(option).empty())
        throw std::invalid_argument(std::string(option) + " requires a value");
    if (command.has("--snapshot-return-type") &&
        (elf_mode || !source_mode || command.has("--module-index") ||
         command.has("--generated") || command.has("--id")))
      throw std::invalid_argument("--snapshot-return-type requires source preparation with --include");
  }
  if (command.has("--cwd") && command.option("--cwd").empty())
    throw std::invalid_argument("--cwd requires a directory");
  const auto mode = command.option("--prepared-mode", "prefer");
  if (mode != "off" && mode != "prefer" && mode != "require")
    throw std::invalid_argument("--prepared-mode must be off, prefer or require");
  if (mode != "off" && command.has("--prepared") && command.option("--prepared").empty())
    throw std::invalid_argument("--prepared requires a directory");
  for (const auto &key : {"--count", "--timeout", "--after", "--probe",
                          "--limit", "--stack-depth"}) {
    if (command.has(key)) {
      command.number(key, 0);
    }
  }
  if ((command.kind == K::event_show || command.kind == K::event_stack) && command.has("--index"))
    command.number("--index", 0);
  if (command.has("--context-event-bytes")) {
    const auto bytes = command.number("--context-event-bytes", 0);
    if (!bytes || bytes > UINT32_MAX)
      throw std::invalid_argument("--context-event-bytes must be a positive 32-bit byte count");
  }
  if (command.has("--context-transport-memory-bytes")) {
    const auto bytes = command.number("--context-transport-memory-bytes", 0);
    if (!bytes || bytes > INT64_MAX)
      throw std::invalid_argument("--context-transport-memory-bytes must be a positive byte count");
  }
  if (command.number("--limit", 0) > UINT32_MAX) {
    throw std::invalid_argument("--limit must fit a 32-bit page size");
  }
  if (command.kind == K::attach && !command.arguments.empty()) {
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
  const auto application_only = command.option("--stack-application-only", "true");
  if (application_only != "true" && application_only != "false") {
    throw std::invalid_argument("--stack-application-only must be true or false");
  }
  if (command.number("--stack-depth", 0) > UINT32_MAX) {
    throw std::invalid_argument("--stack-depth must fit a 32-bit depth");
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

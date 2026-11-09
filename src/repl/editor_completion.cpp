#include "editor_completion.hpp"

#include "help.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <map>

namespace tracer_repl {
namespace {
using std::string_view;
namespace fs = std::filesystem;
constexpr string_view whitespace = " \t\r\n";

struct Word {
  string_view text;
  char quote = 0;
  bool closed = true;
};

std::vector<Word> words(string_view input) {
  std::vector<Word> result;
  std::size_t pos = 0;
  while ((pos = input.find_first_not_of(whitespace, pos)) != input.npos) {
    const auto begin = pos++;
    const char first = input[begin];
    if (first == '\'' || first == '"' || first == '`') {
      while (pos < input.size() && input[pos] != first) {
        if (first != '`' && input[pos] == '\\' && pos + 1 < input.size()) ++pos;
        ++pos;
      }
      const bool closed = pos < input.size();
      pos += closed;
      result.push_back({input.substr(begin, pos - begin), first, closed});
    } else {
      pos = std::min(input.find_first_of(whitespace, begin), input.size());
      result.push_back({input.substr(begin, pos - begin)});
    }
  }
  if (result.empty() || (result.back().closed && !input.empty() &&
      whitespace.find(input.back()) != whitespace.npos))
    result.push_back({});
  return result;
}

std::string word_value(Word word) {
  if (!word.quote) return std::string(word.text);
  word.text.remove_prefix(1);
  if (word.closed) word.text.remove_suffix(1);
  std::string value;
  for (std::size_t i = 0; i < word.text.size(); ++i) {
    char c = word.text[i];
    if (word.quote != '`' && c == '\\' && i + 1 < word.text.size()) {
      c = word.text[++i];
      if (c == 'n') c = '\n';
      else if (c == 'r') c = '\r';
      else if (c == 't') c = '\t';
    }
    value += c;
  }
  return value;
}

std::vector<std::string> paths(std::string prefix, const fs::path& base,
                               bool directories_only) {
  if (prefix == "~" || prefix.starts_with("~/")) {
    const auto* home = std::getenv("HOME");
    if (!home || !*home) return {};
    prefix.replace(0, 1, home);
    if (!prefix.ends_with('/') && prefix == home) prefix += '/';
  }
  const auto slash = prefix.rfind('/');
  const auto parent = prefix.substr(0, slash == prefix.npos ? 0 : slash + 1);
  const auto leaf = string_view(prefix).substr(parent.size());
  std::vector<std::string> result;
  std::error_code error;
  fs::directory_iterator it((base.empty() ? fs::path(".") : base) / parent, error), end;
  for (; it != end && !error; it.increment(error)) {
    const auto name = it->path().filename().string();
    if (!string_view(name).starts_with(leaf) ||
        (name.starts_with('.') && !leaf.starts_with('.')) ||
        name.find_first_of("\r\n\t") != name.npos)
      continue;
    std::error_code status;
    const bool directory = it->is_directory(status);
    if (!status && (directory || !directories_only))
      result.push_back(parent + name + (directory ? "/" : ""));
  }
  std::sort(result.begin(), result.end());
  return result;
}

bool path_argument(string_view type) {
  return type.ends_with("FILE") || type.ends_with("PATH") || type.ends_with("DIRECTORY") ||
         type.ends_with("HEADER") || type == "ELF" || type == "DATABASE" ||
         type.starts_with("NEW_FILE.");
}

std::string quote_path(const std::string& path, char quote, string_view after) {
  if (!quote && !path.starts_with('-') && path.find_first_of(" \t'\"\\%`") == path.npos) return path;
  if (!quote) quote = '"';
  std::string result(1, quote);
  for (char c : path) {
    if (c == quote || c == '\\') result += '\\';
    result += c;
  }
  if (!path.ends_with('/') && !after.starts_with(quote)) result += quote;
  return result;
}

std::map<string_view, string_view> options(string_view usage) {
  std::map<string_view, string_view> result;
  for (std::size_t at = 0; (at = usage.find("--", at)) != usage.npos;) {
    const auto end = std::min(usage.find_first_of(" \t\r\n])|", at), usage.size());
    const auto option = usage.substr(at, end - at);
    at = end;
    const auto argument = std::min(usage.find_first_not_of(" \t", end), usage.size());
    const auto stop = std::min(usage.find_first_of(" \t\r\n])", argument), usage.size());
    const auto type = usage.substr(argument, stop - argument);
    result.emplace(option, !type.empty() && ((type.front() >= 'A' && type.front() <= 'Z') ||
                   option == "--format" || option == "--abi" || type.find('|') != type.npos)
                   && !type.starts_with('|') && !type.starts_with('[') ? type : string_view{});
  }
  return result;
}

void add_choice(EditorCompletions& result, string_view choice, string_view prefix) {
  if (choice.starts_with(prefix)) result.items.emplace_back(choice);
}

void add_values(EditorCompletions& result, string_view type, string_view prefix) {
  if (type.find('|') == type.npos) return;
  for (;;) {
    const auto split = type.find('|');
    const auto choice = type.substr(0, split);
    if (choice != "N") add_choice(result, choice, prefix);
    if (split == type.npos) return;
    type.remove_prefix(split + 1);
  }
}
} // namespace

EditorCompletions complete_command(string_view input, string_view after) {
  const auto tokens = words(input);
  std::string entered;
  for (const auto& token : tokens) {
    if (!entered.empty() && entered != "%") entered += ' ';
    entered += token.text;
  }
  EditorCompletions names{input.size(), {}};
  for (const auto& name : command_completions()) {
    if (string_view(name).starts_with(entered) && name != entered) names.items.push_back(name);
  }

  if (tokens.size() < 2 || (tokens[0].text == "%help" && !names.items.empty()))
    return names;

  std::string command;
  std::string usage;
  std::string command_name;
  std::size_t arguments_begin = 1;
  for (std::size_t i = 0; i + 1 < tokens.size() && !tokens[i].text.starts_with('-'); ++i) {
    if (i) command += ' ';
    command += i ? tokens[i].text : tokens[i].text.substr(1);
    if (auto candidate = command_usage(command); !candidate.empty()) {
      usage = std::move(candidate);
      command_name = command;
      arguments_begin = i + 1;
    }
  }
  if (usage.empty()) return names;
  const auto available = options(usage);
  const auto current = tokens.back();
  EditorCompletions result{current.text.size(), {}};
  if (current.closed && current.quote) return result;
  const auto prefix = word_value(current);
  string_view argument;
  string_view option;
  fs::path base;
  std::size_t positionals = 0;
  for (auto i = arguments_begin; i + 1 < tokens.size(); ++i) {
    const auto found = available.find(tokens[i].text);
    if (found == available.end()) {
      ++positionals;
      continue;
    }
    if (found->second.empty()) continue;
    if (++i + 1 == tokens.size()) {
      option = found->first;
      argument = found->second;
      break;
    }
    if (found->first == "--cwd") base = word_value(tokens[i]);
  }
  if (option.empty() && positionals == 0) {
    const auto positional = usage.find_first_not_of(whitespace, command_name.size() + 1);
    if (positional != usage.npos) {
      argument = string_view(usage).substr(positional);
      argument = argument.substr(0, argument.find_first_of(" \t\r\n"));
    }
  }

  if (current.quote != '`' && (path_argument(argument) ||
      (option == "--module" && prefix.find('/') != prefix.npos))) {
    for (const auto& path : paths(prefix, option == "--cwd" ? fs::path{} : base,
                                  argument.ends_with("DIRECTORY")))
      result.items.push_back(quote_path(path, current.quote, after));
    return result;
  }
  if (!current.quote) {
    if (option == "--format" || option == "--abi") add_choice(result, argument, prefix);
    else if (option == "--type" && command_name == "memory read") {
      for (auto type : {"u8", "u16", "u32", "u64", "i8", "i16", "i32", "i64", "f32", "f64"})
        add_choice(result, type, prefix);
    } else {
      add_values(result, argument, prefix);
    }
    if (option.empty() && (prefix.empty() || prefix.starts_with('-'))) {
      for (const auto& [name, type] : available)
        if (name.starts_with(prefix)) result.items.emplace_back(std::string(name) + ' ');
      if (prefix.empty() && !names.items.empty()) {
        for (const auto& name : names.items)
          result.items.push_back(name.substr(entered.size()));
      }
    }
  }
  return result.items.empty() ? std::move(names) : std::move(result);
}

std::optional<EditorCompletions> complete_include(string_view input, string_view after) {
  const auto newline = input.rfind('\n');
  if (newline != input.npos) input.remove_prefix(newline + 1);
  const auto hash = input.find_first_not_of(" \t");
  if (hash == input.npos || input[hash] != '#') return {};
  input.remove_prefix(hash + 1);
  const auto start = input.find_first_not_of(" \t");
  if (start == input.npos) return {};
  input.remove_prefix(start);
  if (input.starts_with("include_next")) input.remove_prefix(12);
  else if (input.starts_with("include")) input.remove_prefix(7);
  else return {};
  const auto opening = input.find_first_not_of(" \t");
  if (opening == input.npos || input[opening] != '"') return {};
  input.remove_prefix(opening + 1);
  if (input.find('"') != input.npos) return {};
  EditorCompletions result{input.size(), {}};
  for (auto path : paths(std::string(input), {}, false)) {
    if (path.find('"') != path.npos) continue;
    if (!path.ends_with('/') && !after.starts_with('"')) path += '"';
    result.items.push_back(std::move(path));
  }
  return result;
}
} // namespace tracer_repl

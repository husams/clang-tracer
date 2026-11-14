#include "editor.hpp"

#include "completion.hpp"
#include "controller.hpp"
#include "editor_completion.hpp"
#include "syntax.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <thread>
#include <unistd.h>

namespace tracer_repl {
namespace {
using std::string_view;
using Color = replxx::Replxx::Color;
constexpr string_view whitespace = " \t\r\n";
constexpr string_view indent = "  ";

bool command(string_view text) {
  const auto first = text.find_first_not_of(whitespace);
  return first != text.npos && text[first] == '%';
}

bool leading_byte(unsigned char byte) { return (byte & 0xc0) != 0x80; }

int characters(string_view text) {
  return static_cast<int>(std::count_if(text.begin(), text.end(), leading_byte));
}

std::size_t byte_offset(string_view text, int cursor) {
  std::size_t byte = 0;
  for (; byte < text.size(); ++byte)
    if (leading_byte(text[byte]) && cursor-- == 0) break;
  return byte;
}

std::size_t line_start(string_view text) {
  const auto last = text.rfind('\n');
  return last == text.npos ? 0 : last + 1;
}

string_view indentation(string_view line) {
  return line.substr(0, line.find_first_not_of(" \t"));
}

struct Snippet {
  string_view trigger;
  string_view body;
  const char* final_field_hint = nullptr;
};

constexpr Snippet snippets[] = {
    {"#include", "#include <@header@>"},
    {"if", "if (@condition@) {\n  @statement@\n}"},
    {"for", "for (@init@; @condition@; @step@) {\n  @statement@\n}"},
    {"while", "while (@condition@) {\n  @statement@\n}"},
    {"%cpp load", "%cpp load \"@file@\""},
    {"%events open", "%events open \"@file@\" --as @name@"},
    {"%collect", "%collect --count @count@ --as @name@"},
    {"%probe inject", "%probe inject \"@selector@\" @@",
     "  [optional: --callback FILE or --prepared DIRECTORY]"},
    {"%probe inject --callback", "%probe inject \"@selector@\" --callback \"@file@\""},
    {"%probe inject --prepared", "%probe inject \"@selector@\" --prepared \"@directory@\""},
    {"%probe prepare", "%probe prepare \"@selector@\" --include \"@header@\" --output \"@directory@\""},
};

const Snippet* find_snippet(string_view text) {
  const auto start = text.find_first_not_of(" \t");
  if (start == text.npos) return nullptr;
  const auto trigger = text.substr(start);
  const auto trimmed = trigger.substr(0, trigger.find_last_not_of(" \t") + 1);
  for (const auto& snippet : snippets)
    if (snippet.trigger == trigger ||
        (snippet.final_field_hint && snippet.trigger == trimmed) ||
        (snippet.trigger == "#include" && trigger.size() >= 5 && snippet.trigger.starts_with(trigger)))
      return &snippet;
  return nullptr;
}

char closing(char opening) {
  switch (opening) {
  case '(': return ')';
  case '[': return ']';
  case '{': return '}';
  case '<': return '>';
  case '"': case '\'': case '`': return opening;
  default: return 0;
  }
}

bool inside_literal(string_view prefix, bool is_command) {
  // Extending the prefix distinguishes an open string/comment from a finished
  // token that just happens to end at the cursor.
  std::string probe(prefix);
  probe += ' ';
  const auto spans = highlight(probe, is_command ? Syntax::command : Syntax::cpp);
  return !spans.empty() && spans.back().end == probe.size() &&
      (spans.back().style == Style::literal || spans.back().style == Style::comment ||
       spans.back().style == Style::header);
}

bool escaped(string_view prefix) {
  std::size_t backslashes = 0;
  while (backslashes < prefix.size() && prefix[prefix.size() - backslashes - 1] == '\\')
    ++backslashes;
  return backslashes % 2 != 0;
}

bool identifier_byte(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_' || c >= 128;
}

EditorCompletions keywords(string_view text) {
  auto start = text.size();
  while (start > 0 && identifier_byte(text[start - 1])) --start;
  const auto left = text.substr(0, start);
  if (start == text.size() || left.ends_with('.') || left.ends_with("->") ||
      left.ends_with("::") || inside_literal(text, false)) return {};
  return {text.size() - start, cpp_keywords(text.substr(start))};
}
} // namespace

Input::Input(bool color) : terminal_(::isatty(STDIN_FILENO)) {
  editor_.set_no_color(!color);
  editor_.set_max_history_size(1000);
  if (const auto* home = std::getenv("HOME"); terminal_ && home && *home) {
    history_path_ = std::string(home) + "/.clang-tracer-history";
    editor_.history_load(history_path_);
  }
  editor_.set_double_tab_completion(false);
  if (terminal_ && ::isatty(STDOUT_FILENO)) editor_.enable_bracketed_paste();
  editor_.set_modify_callback([this](std::string& text, int& cursor) { changed(text, cursor); });
  editor_.bind_key(Key::ENTER, [this](char32_t key) { return enter(key); });
  editor_.bind_key(Key::TAB, [this](char32_t key) { return tab(key, false); });
  editor_.bind_key(Key::shift(Key::TAB), [this](char32_t key) { return tab(key, true); });
  editor_.bind_key(Key::meta('/'), [this](char32_t key) {
    return editor_.invoke(Action::COMPLETE_LINE, key);
  });
  editor_.bind_key(Key::BACKSPACE, [this](char32_t key) { return erase(key, true); });
  editor_.bind_key(Key::DELETE, [this](char32_t key) { return erase(key, false); });
  for (char key : string_view("()[]{}<>\"'`"))
    editor_.bind_key(key, [this](char32_t pressed) { return pair(pressed); });

  editor_.set_completion_callback(
      [this](const std::string& text, int& length) { return complete(text, length); });
  if (color) {
    editor_.set_highlighter_callback(
        [this](const auto& text, auto& colors) { colorize(text, colors); });
    editor_.set_hint_callback([this](const std::string& text, int& length, Color&) {
      if (selected_ && fields_[field_].hint) {
        length = 0;
        return replxx::Replxx::hints_t{fields_[field_].hint};
      }
      const string_view line(text.data() + line_start(text), text.size() - line_start(text));
      if (find_snippet(line)) {
        length = 0;
        return replxx::Replxx::hints_t{"  [Tab: insert template]"};
      }
      return replxx::Replxx::hints_t{};
    });
  }
}

void Input::changed(std::string& text, int& cursor) {
  if (fields_.empty() && closers_.empty()) {
    previous_ = text;
    return;
  }
  auto pos = byte_offset(text, cursor);
  if (text == previous_) {
    if (selected_ && pos != fields_[field_].begin) selected_ = false;
    return;
  }

  // Keep text after the active field out of the prefix match. A typed
  // separator can be identical to the first character of the template suffix.
  auto limit = pos;
  if (!fields_.empty()) {
    const auto& field = fields_[field_];
    limit = std::min(limit, selected_ ? field.begin : field.end);
  }
  std::size_t begin = 0;
  while (begin < std::min(limit, previous_.size()) && text[begin] == previous_[begin])
    ++begin;
  auto old_end = previous_.size(), new_end = text.size();
  while (old_end > begin && new_end > std::max(begin, pos) &&
         previous_[old_end - 1] == text[new_end - 1]) {
    --old_end;
    --new_end;
  }

  if (selected_ && begin == fields_[field_].begin && old_end == begin) {
    const auto length = fields_[field_].end - begin;
    text.erase(new_end, length);
    old_end += length;
    pos = new_end;
    cursor = characters(string_view(text).substr(0, pos));
  }
  selected_ = false;
  const auto shift = [old_end, new_end](std::size_t offset) {
    return new_end + (offset - old_end);
  };
  std::erase_if(closers_, [begin, old_end](std::size_t offset) {
    return offset >= begin && offset < old_end;
  });
  for (auto& offset : closers_)
    if (offset >= old_end) offset = shift(offset);
  if (!fields_.empty()) {
    auto& current = fields_[field_];
    if (begin < current.begin || old_end > current.end) {
      fields_.clear();
    } else {
      current.end = shift(current.end);
      for (auto i = field_ + 1; i < fields_.size(); ++i) {
        fields_[i].begin = shift(fields_[i].begin);
        fields_[i].end = shift(fields_[i].end);
      }
    }
  }
  previous_ = text;
}

void Input::set(std::string text, std::size_t pos) {
  int cursor = characters(string_view(text).substr(0, pos));
  changed(text, cursor);
  editor_.set_state({text.c_str(), cursor});
}

void Input::clear_selection(std::string& text, std::size_t pos) {
  if (!selected_ || pos != fields_[field_].begin) return;
  text.erase(pos, fields_[field_].end - pos);
  selected_ = false;
}

Input::Result Input::tab(char32_t key, bool backwards) {
  const auto state = editor_.get_state();
  std::string text = state.text();
  const auto pos = byte_offset(text, state.cursor_position());
  if (!fields_.empty() && pos >= fields_[field_].begin && pos <= fields_[field_].end) {
    if (backwards && field_ > 0) --field_;
    else if (!backwards) ++field_;
    const auto next = fields_[field_].begin;
    selected_ = field_ + 1 < fields_.size();
    if (!selected_) fields_.clear();
    editor_.set_state({text.c_str(), characters(string_view(text).substr(0, next))});
    return Result::CONTINUE;
  }
  if (backwards) return Result::CONTINUE;

  const auto left = string_view(text).substr(0, pos);
  const auto start = line_start(left);
  const auto line = left.substr(start);
  const auto* snippet = find_snippet(line);
  if (snippet && (pos == text.size() || text[pos] == '\n') &&
      (start == 0 || !inside_literal(left, false))) {
    const std::string spacing(indentation(line));
    std::string expansion = spacing;
    std::vector<Field> fields;
    for (std::size_t i = 0; i < snippet->body.size(); ++i) {
      const char c = snippet->body[i];
      if (c == '@') {
        const auto end = snippet->body.find('@', i + 1);
        const auto begin = start + expansion.size();
        expansion += snippet->body.substr(i + 1, end - i - 1);
        fields.push_back({begin, start + expansion.size()});
        i = end;
      } else {
        expansion += c;
        if (c == '\n') expansion += spacing;
      }
    }
    fields.back().hint = snippet->final_field_hint;
    fields.push_back({start + expansion.size(), start + expansion.size()});
    text.replace(start, pos - start, expansion);
    selected_ = false;
    set(text, fields.front().begin);
    fields_ = std::move(fields);
    field_ = 0;
    selected_ = true;
    return Result::CONTINUE;
  }
  if (!command(text) && start != 0 && line == indentation(line)) {
    text.insert(pos, indent);
    set(std::move(text), pos + indent.size());
    return Result::CONTINUE;
  }
  return editor_.invoke(Action::COMPLETE_LINE, key);
}

Input::Result Input::enter(char32_t key) {
  const auto state = editor_.get_state();
  std::string text = state.text();
  const auto pos = byte_offset(text, state.cursor_position());
  const auto left = string_view(text).substr(0, pos);
  const auto tracked = std::find(closers_.begin(), closers_.end(), pos) != closers_.end();
  const bool split_pair = tracked && pos > 0 && closing(text[pos - 1]) == text[pos] &&
                          string_view(")]}").find(text[pos]) != string_view::npos;
  if (command(text) || (!split_pair && !incomplete_cpp(text) && !incomplete_cpp(std::string(left))))
    return editor_.invoke(Action::COMMIT_LINE, key);

  const auto line = left.substr(line_start(left));
  const std::string spacing(indentation(line));
  std::string insertion = "\n" + spacing;
  const auto tail = line.find_last_not_of(" \t");
  if (tail != line.npos && string_view("({[").find(line[tail]) != line.npos &&
      !inside_literal(left, false))
    insertion += indent;
  const auto next = pos + insertion.size();
  if (split_pair) insertion += "\n" + spacing;
  text.insert(pos, insertion);
  set(std::move(text), next);
  return Result::CONTINUE;
}

Input::Result Input::pair(char32_t key) {
  const auto state = editor_.get_state();
  std::string text = state.text();
  const auto pos = byte_offset(text, state.cursor_position());
  const auto left = string_view(text).substr(0, pos);
  const auto line = left.substr(line_start(left));
  const char typed = static_cast<char>(key);
  if (!selected_ && !escaped(left)) {
    const auto close = text.find_first_not_of(whitespace, pos);
    if (close != text.npos && text[close] == typed &&
        (close == pos || (typed == '}' && line == indentation(line))) &&
        std::find(closers_.begin(), closers_.end(), close) != closers_.end()) {
      std::erase(closers_, close);
      const auto from = line_start(left);
      const auto to = line_start(string_view(text).substr(0, close));
      text.erase(from, to - from);
      set(std::move(text), close + 1 - (to - from));
      return Result::CONTINUE;
    }
  }

  const bool is_command = command(text);
  const auto next = string_view(text).substr(pos).find_first_not_of(whitespace);
  const bool before_text = next == 0 && identifier_byte(text[pos]);
  const bool in_literal = inside_literal(left, is_command);
  const auto first = line.find_first_not_of(" \t");
  const bool include = first != line.npos && line.substr(first).starts_with("#include") &&
      line.substr(first + 8).find_first_not_of(" \t") == line.npos;
  const bool quote = typed == '"' || typed == '\'';
  const bool separator = typed == '\'' && !left.empty() && identifier_byte(left.back());
  if (closing(typed) && !escaped(left) && (!in_literal || (selected_ && is_command && quote)) &&
      (!before_text || selected_) && !separator &&
      (is_command ? quote || typed == '`' : typed != '`' && (typed != '<' || include))) {
    clear_selection(text, pos);
    text.insert(pos, {typed, closing(typed)});
    set(std::move(text), pos + 1);
    closers_.push_back(pos + 1);
    return Result::CONTINUE;
  }

  if (!is_command && typed == '}' && !in_literal && !line.empty() && line == indentation(line)) {
    const auto remove = line.back() == '\t' ? 1 : std::min(line.size(), indent.size());
    text.replace(pos - remove, remove, 1, typed);
    set(std::move(text), pos + 1 - remove);
    return Result::CONTINUE;
  }
  return editor_.invoke(Action::INSERT_CHARACTER, key);
}

Input::Result Input::erase(char32_t key, bool backwards) {
  const auto state = editor_.get_state();
  std::string text = state.text();
  const auto pos = byte_offset(text, state.cursor_position());
  if (selected_) {
    clear_selection(text, pos);
    set(std::move(text), pos);
    return Result::CONTINUE;
  }
  if (backwards && pos > 0 && pos < text.size() && closing(text[pos - 1]) == text[pos] &&
      std::find(closers_.begin(), closers_.end(), pos) != closers_.end()) {
    text.erase(pos - 1, 2);
    set(std::move(text), pos - 1);
    return Result::CONTINUE;
  }
  return editor_.invoke(backwards ? Action::DELETE_CHARACTER_LEFT_OF_CURSOR :
                                    Action::DELETE_CHARACTER_UNDER_CURSOR, key);
}

replxx::Replxx::completions_t Input::complete(const std::string& text, int& length) {
  const auto state = editor_.get_state();
  const std::string full = state.text();
  const auto suffix = selected_ && fields_[field_].begin == text.size() ?
      fields_[field_].end : text.size();
  const auto after = string_view(full).substr(suffix);
  const auto first = text.find_first_not_of(whitespace);
  if (first == text.npos) return {};

  EditorCompletions results;
  if (command(text)) {
    results = complete_command(string_view(text).substr(first), after);
  } else if (auto include = complete_include(text, after)) {
    results = std::move(*include);
  } else if (cpp_) {
    results = keywords(text);
    try {
      auto cpp = cpp_->complete(text);
      if (results.items.empty())
        results = {cpp.prefix.size(), std::move(cpp.items)};
      else if (cpp.prefix.size() == results.bytes) {
        results.items.insert(results.items.end(), cpp.items.begin(), cpp.items.end());
        std::sort(results.items.begin(), results.items.end());
        results.items.erase(std::unique(results.items.begin(), results.items.end()), results.items.end());
      }
    } catch (const std::exception& error) {
      if (results.items.empty()) {
        // replxx only clears and restores the input around cross-thread output.
        std::thread([this, message = std::string(error.what())] {
          editor_.print("C++ completion: %s\n", message.c_str());
        }).join();
      }
    }
  }
  if (results.bytes > text.size()) return {};
  length = characters(string_view(text).substr(text.size() - results.bytes));
  replxx::Replxx::completions_t items;
  for (const auto& item : results.items) items.emplace_back(item);
  return items;
}

void Input::colorize(const std::string& text, replxx::Replxx::colors_t& colors) const {
  constexpr Color palette[] = {Color::BRIGHTCYAN, Color::GREEN, Color::MAGENTA,
      Color::BROWN, Color::GRAY, Color::BRIGHTBLUE, Color::BRIGHTGREEN, Color::YELLOW};
  std::size_t byte = 0, character = 0;
  for (const auto& span : highlight(text, command(text) ? Syntax::command : Syntax::cpp)) {
    for (; byte < span.end; ++byte) {
      if (!leading_byte(text[byte])) continue;
      if (character == colors.size()) return;
      if (byte >= span.begin) colors[character] = palette[static_cast<std::size_t>(span.style)];
      ++character;
    }
  }
  if (selected_ && text == previous_) {
    const auto& field = fields_[field_];
    const auto first = characters(string_view(text).substr(0, field.begin));
    const auto last = characters(string_view(text).substr(0, field.end));
    const auto selection =
        replxx::color::operator|(Color::BLACK, replxx::color::bg(Color::BRIGHTCYAN));
    std::fill(colors.begin() + first, colors.begin() + last, selection);
  }
}

std::optional<std::string> Input::read(const std::string& prompt) {
  if (!terminal_) {
    std::string line;
    if (!std::getline(std::cin, line)) return {};
    return line;
  }
  for (;;) {
    previous_.clear();
    fields_.clear();
    closers_.clear();
    selected_ = false;
    errno = 0;
    const auto* line = editor_.input(prompt);
    if (!line && (errno == EAGAIN || interrupted.load())) {
      interrupted = false;
      continue;
    }
    if (!line) return {};
    std::string result(line);
    if (!result.empty()) {
      editor_.history_add(result);
      // Merge into the shared file now so a crash or a concurrent REPL keeps it.
      if (!history_path_.empty()) editor_.history_sync(history_path_);
    }
    return result;
  }
}
} // namespace tracer_repl

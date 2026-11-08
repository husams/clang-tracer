#include "syntax.hpp"

#include "commands.hpp"

#include <clang/Basic/CharInfo.h>
#include <clang/Basic/IdentifierTable.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Lex/Lexer.h>

#include <algorithm>
#include <ostream>

namespace tracer_repl {
namespace {
std::vector<Highlight> highlight_command(const std::string& text) {
  constexpr std::string_view whitespace = " \t\r\n";
  constexpr std::string_view operators = "|=<>";
  constexpr std::string_view quotes = "\"'`";
  constexpr std::string_view delimiters = " \t\r\n|=<>[]\"'`";
  std::vector<Highlight> result;
  for (std::size_t begin = 0; begin < text.size();) {
    const char first = text[begin];
    auto end = begin + 1;
    if (quotes.find(first) != std::string_view::npos) {
      while (end < text.size()) {
        const char c = text[end++];
        if (c == first) break;
        if (c == '\\' && first != '`' && end < text.size()) ++end;
      }
      result.push_back({begin, end, Style::literal});
    } else if (operators.find(first) != std::string_view::npos) {
      result.push_back({begin, end, Style::op});
    } else if (delimiters.find(first) == std::string_view::npos) {
      end = std::min(text.find_first_of(delimiters, end), text.size());
      const auto word = std::string_view(text).substr(begin, end - begin);
      if (first == '%') {
        for (const auto& name : command_names()) {
          const auto after = begin + name.size();
          if (after > end && text.compare(begin, name.size(), name) == 0 &&
              (after == text.size() || whitespace.find(text[after]) != std::string_view::npos))
            end = after;
        }
        result.push_back({begin, end, Style::keyword});
      } else {
        auto value = word;
        if (first == '-') value.remove_prefix(1);
        if (value.size() > 1 && value.front() == '-') value.remove_prefix(1);
        if (first == '-' && !value.empty() && clang::isAsciiIdentifierStart(value.front())) {
          result.push_back({begin, end, Style::option});
        } else if (word == "PRE" || word == "POST" || word == "true" || word == "false" ||
                   (!value.empty() && std::all_of(value.begin(), value.end(), clang::isDigit))) {
          result.push_back({begin, end, Style::literal});
        }
      }
    }
    begin = end;
  }
  return result;
}

std::vector<Highlight> highlight_cpp(const std::string& text) {
  clang::LangOptions language;
  language.CPlusPlus = language.CPlusPlus11 = language.CPlusPlus14 =
      language.CPlusPlus17 = true;
  language.Bool = language.LineComment = language.Digraphs = true;
  clang::Lexer lexer({}, language, text.data(), text.data(), text.data() + text.size());
  lexer.SetCommentRetentionState(true);
  static thread_local clang::IdentifierTable identifiers(language);

  std::vector<Highlight> result;
  clang::Token token;
  for (;;) {
    lexer.LexFromRawLexer(token);
    auto kind = token.getKind();
    if (kind == clang::tok::eof) break;
    const auto begin = token.getLocation().getRawEncoding();
    const auto end = static_cast<std::size_t>(begin) + token.getLength();
    if (kind == clang::tok::raw_identifier) {
      const auto id = identifiers.find(token.getRawIdentifier());
      if (id != identifiers.end()) kind = id->second->getTokenID();
    }

    if (clang::tok::isLiteral(kind) || kind == clang::tok::kw_true ||
        kind == clang::tok::kw_false || kind == clang::tok::kw_nullptr ||
        (kind == clang::tok::unknown && (text[begin] == '"' || text[begin] == '\''))) {
      result.push_back({begin, end, Style::literal});
    } else if (kind == clang::tok::comment) {
      result.push_back({begin, end, Style::comment});
    } else if (clang::tok::getKeywordSpelling(kind)) {
      result.push_back({begin, end, Style::keyword});
    } else if (clang::tok::getPunctuatorSpelling(kind) &&
               kind != clang::tok::semi && kind != clang::tok::comma &&
               kind != clang::tok::l_paren && kind != clang::tok::r_paren &&
               kind != clang::tok::l_brace && kind != clang::tok::r_brace &&
               kind != clang::tok::l_square && kind != clang::tok::r_square) {
      result.push_back({begin, end, Style::op});
    }
  }
  return result;
}
} // namespace

std::vector<Highlight> highlight(const std::string& text, Syntax syntax) {
  return syntax == Syntax::command ? highlight_command(text) : highlight_cpp(text);
}

void print_code(std::ostream& out, std::string_view text, Syntax syntax, bool color) {
  if (!color || text.empty()) {
    out << text;
    return;
  }
  constexpr std::string_view colors[] = {
      "\033[1;36m", "\033[32m", "\033[35m", "\033[33m", "\033[90m"};
  std::size_t position = 0;
  for (const auto& span : highlight(std::string(text), syntax)) {
    out << text.substr(position, span.begin - position)
        << colors[static_cast<std::size_t>(span.style)]
        << text.substr(span.begin, span.end - span.begin) << "\033[0m";
    position = span.end;
  }
  out << text.substr(position);
}
} // namespace tracer_repl

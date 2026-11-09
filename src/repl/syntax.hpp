#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace tracer_repl {
enum class Syntax { command, cpp };
enum class Style { keyword, literal, op, option, comment, directive, header, number };

struct Highlight {
  std::size_t begin;
  std::size_t end;
  Style style;
};

std::vector<Highlight> highlight(const std::string& text, Syntax syntax);
std::vector<std::string> cpp_keywords(std::string_view prefix);
void print_code(std::ostream& out, std::string_view text, Syntax syntax, bool color);
} // namespace tracer_repl

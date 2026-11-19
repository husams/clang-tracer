#pragma once

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace tracer {

// Normalize spelling only: preserve namespaces, template arguments and every
// pointer qualifier. In particular, T const* and T* const must stay distinct.
inline std::string normalized_cpp_type_spelling(std::string_view text) {
  std::vector<std::string> tokens;
  for (std::size_t i = 0; i < text.size();) {
    const auto c = static_cast<unsigned char>(text[i]);
    if (std::isspace(c)) {
      ++i;
    } else if (std::isalnum(c) || c == '_') {
      const auto begin = i++;
      while (i < text.size() &&
             (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_'))
        ++i;
      tokens.emplace_back(text.substr(begin, i - begin));
    } else {
      tokens.emplace_back(1, text[i++]);
    }
  }
  const auto normalize = [&](const auto &self, std::size_t begin, std::size_t end) -> void {
    // Normalize each nested template argument independently.
    std::size_t depth = 0, argument = begin;
    for (auto i = begin; i < end; ++i) {
      if (tokens[i] == "<") {
        if (depth++ == 0)
          argument = i + 1;
      } else if (tokens[i] == ">" && depth) {
        if (--depth == 0)
          self(self, argument, i);
      } else if (tokens[i] == "," && depth == 1) {
        self(self, argument, i);
        argument = i + 1;
      }
    }
    // Complex declarators and expressions are deliberately left unchanged.
    for (auto i = begin; i < end; ++i)
      if (tokens[i] == "(" || tokens[i] == ")" || tokens[i] == "[" ||
          tokens[i] == "]" || tokens[i] == "\"" || tokens[i] == "'")
        return;
    depth = 0;
    auto stop = end;
    for (auto i = begin; i < end; ++i) {
      if (tokens[i] == "<")
        ++depth;
      else if (tokens[i] == ">" && depth)
        --depth;
      else if (!depth && (tokens[i] == "*" || tokens[i] == "&")) {
        stop = i;
        break;
      }
    }
    std::vector<std::string> qualifiers, base;
    depth = 0;
    for (auto i = begin; i < stop; ++i) {
      const auto &token = tokens[i];
      if (!depth && (token == "const" || token == "volatile"))
        qualifiers.push_back(token);
      else
        base.push_back(token);
      if (token == "<")
        ++depth;
      else if (token == ">" && depth)
        --depth;
    }
    if (base.empty())
      return;
    auto out = begin;
    for (const char *qualifier : {"const", "volatile"})
      for (const auto &token : qualifiers)
        if (token == qualifier)
          tokens[out++] = token;
    for (const auto &token : base)
      tokens[out++] = token;
  };
  normalize(normalize, 0, tokens.size());
  std::string result;
  const auto word = [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
  };
  for (const auto &token : tokens) {
    if (!result.empty() && word(result.back()) && word(token.front()))
      result.push_back(' ');
    result += token;
  }
  return result;
}

// GCC can omit template-parameter DIEs even for explicit library
// specializations such as char_traits<char>. Accept the fully spelled name
// only when every argument is a builtin or another reserved std type.
// In particular, std::hash<ApplicationType> is never library-only evidence.
inline bool standard_library_type_spelling(std::string_view input) {
  const auto text = normalized_cpp_type_spelling(input);
  const auto library_name = [](std::string_view name) {
    if (name.substr(0, 5) != "std::")
      return false;
    name.remove_prefix(5);
    while (!name.empty()) {
      const auto separator = name.find("::");
      const auto part = name.substr(0, separator);
      if (part.empty() || !(std::isalpha(static_cast<unsigned char>(part.front())) ||
                            part.front() == '_'))
        return false;
      for (const unsigned char c : part)
        if (!(std::isalnum(c) || c == '_'))
          return false;
      if (separator == std::string_view::npos)
        return true;
      name.remove_prefix(separator + 2);
    }
    return false;
  };
  const auto check = [&](const auto &self, std::string_view type) -> bool {
    const auto open = type.find('<');
    if (open == std::string_view::npos) {
      for (const auto builtin : {"void", "bool", "char", "signed char", "unsigned char",
           "wchar_t", "char16_t", "char32_t", "char8_t", "short", "short int",
           "unsigned short", "short unsigned int", "unsigned short int",
           "int", "unsigned int", "long", "long int", "unsigned long",
           "long unsigned int", "unsigned long int", "long long", "long long int",
           "unsigned long long", "long long unsigned int", "unsigned long long int",
           "float", "double", "long double"})
        if (type == builtin)
          return true;
      return library_name(type);
    }
    if (!library_name(type.substr(0, open)) || type.back() != '>')
      return false;
    type = type.substr(open + 1, type.size() - open - 2);
    std::size_t depth = 0, begin = 0;
    for (std::size_t i = 0; i < type.size(); ++i) {
      if (type[i] == '<')
        ++depth;
      else if (type[i] == '>') {
        if (!depth)
          return false;
        --depth;
      } else if (type[i] == ',' && !depth) {
        if (!self(self, type.substr(begin, i - begin)))
          return false;
        begin = i + 1;
      }
    }
    return depth == 0 && self(self, type.substr(begin));
  };
  return check(check, text);
}

} // namespace tracer

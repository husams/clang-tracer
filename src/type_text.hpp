#pragma once

#include <algorithm>
#include <cctype>
#include <string_view>

namespace tracer {

// What `spliceable_type_text` rejects, for the messages that report it.
inline constexpr std::string_view unspliceable_text =
    "; { } # \\ \" control characters, comments, digraphs, trigraphs or _Pragma";

// Type text from a command line, an index or a demangled symbol is spliced into generated
// source, so it must not be able to end the declaration it belongs to or inject a directive.
// Everything else is judged by parsing it, not by a spelling whitelist.
inline bool spliceable_type_text(std::string_view text) {
  const auto word = [](unsigned char c) { return std::isalnum(c) || c == '_'; };
  constexpr std::string_view pragma = "_Pragma";
  for (auto at = text.find(pragma); at != text.npos; at = text.find(pragma, at + 1))
    if ((at == 0 || !word(text[at - 1])) &&
        (at + pragma.size() == text.size() || !word(text[at + pragma.size()])))
      return false;
  return !text.empty() &&
         std::none_of(text.begin(), text.end(), [](unsigned char c) {
           return (c < 0x20 && c != '\t') || c == 0x7f || c == ';' || c == '{' || c == '}' ||
                  c == '#' || c == '\\' || c == '"';
         }) &&
         text.find("//") == text.npos && text.find("/*") == text.npos &&
         // Digraphs and trigraphs spell the same characters.
         text.find("<%") == text.npos && text.find("%>") == text.npos &&
         text.find("%:") == text.npos && text.find("??") == text.npos;
}

} // namespace tracer

#pragma once

#include <algorithm>
#include <string_view>

namespace tracer {

// Type text from a command line, an index or a demangled symbol is spliced into generated
// source, so it must not be able to end the declaration it belongs to or inject a directive.
// Everything else is judged by parsing it, not by a spelling whitelist.
inline bool spliceable_type_text(std::string_view text) {
  return !text.empty() &&
         std::none_of(text.begin(), text.end(), [](unsigned char c) {
           return (c < 0x20 && c != '\t') || c == 0x7f || c == ';' || c == '{' || c == '}' ||
                  c == '#' || c == '\\' || c == '"';
         }) &&
         text.find("//") == text.npos && text.find("/*") == text.npos &&
         text.find("_Pragma") == text.npos &&
         // Digraphs and trigraphs spell the same characters.
         text.find("<%") == text.npos && text.find("%>") == text.npos &&
         text.find("%:") == text.npos && text.find("??") == text.npos;
}

} // namespace tracer

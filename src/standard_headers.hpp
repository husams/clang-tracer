#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tracer::standard_headers {

// Maps a standard-library record recovered from DWARF to the real public header that
// defines it. The record is named by its primary template: template arguments and the
// ABI-bearing inline namespaces (std::__cxx11, std::__1, std::_V2, ...) are not part of
// the key, because the real header selects the namespace through the target's ABI macros.
// The table is a starting point; a caller extends or overrides it with "RECORD=HEADER"
// entries (see parse_entry()).
struct Entry {
  std::string record;  // For example "std::basic_string"
  std::string header;  // "<string>" or a quoted absolute path
};

// Namespaces that libstdc++/libc++ open as inline ABI namespaces. They are removed from
// the lookup key and are never a place a user-extendable entry should name.
inline bool inline_abi_namespace(std::string_view part) {
  return part == "__cxx11" || part == "__cxx98" || part == "__1" || part == "__ndk1" ||
         part == "_V2" || part == "__8" || part == "__7" || part == "__9" || part == "__10" ||
         part == "__11" || part == "__12" || part == "__13" || part == "__14" || part == "__15";
}

inline bool standard_library_scope(std::string_view name) {
  const auto starts = [&](std::string_view prefix) {
    return name.size() >= prefix.size() && name.substr(0, prefix.size()) == prefix;
  };
  return starts("std::") || starts("__gnu_cxx::") || starts("__cxxabiv1::");
}

// "std::__cxx11::basic_string<char, std::char_traits<char>, ...>" -> "std::basic_string".
// A template-argument list is taken to start at the first '<' that is not inside a
// parenthesis; DWARF record names never contain an operator name before their arguments.
inline std::string primary_name(std::string_view display) {
  const auto open = display.find('<');
  const auto scope = open == std::string_view::npos ? display : display.substr(0, open);
  std::string result;
  std::size_t begin = 0;
  while (begin <= scope.size()) {
    const auto end = scope.find("::", begin);
    const auto part = scope.substr(begin, end == std::string_view::npos ? end : end - begin);
    if (!inline_abi_namespace(part)) {
      if (!result.empty())
        result += "::";
      result += part;
    }
    if (end == std::string_view::npos)
      break;
    begin = end + 2;
  }
  return result;
}

inline const std::vector<Entry> &builtin_entries() {
  static const std::vector<Entry> entries = [] {
    std::vector<Entry> table;
    const auto add = [&](const char *header, std::initializer_list<const char *> records) {
      for (const auto *record : records)
        table.push_back({record, header});
    };
    add("<string>", {"std::basic_string", "std::char_traits"});
    add("<string_view>", {"std::basic_string_view"});
    add("<vector>", {"std::vector"});
    add("<array>", {"std::array"});
    add("<deque>", {"std::deque"});
    add("<list>", {"std::list"});
    add("<forward_list>", {"std::forward_list"});
    add("<map>", {"std::map", "std::multimap"});
    add("<set>", {"std::set", "std::multiset"});
    add("<unordered_map>", {"std::unordered_map", "std::unordered_multimap"});
    add("<unordered_set>", {"std::unordered_set", "std::unordered_multiset"});
    add("<stack>", {"std::stack"});
    add("<queue>", {"std::queue", "std::priority_queue"});
    add("<utility>", {"std::pair"});
    add("<tuple>", {"std::tuple"});
    add("<optional>", {"std::optional"});
    add("<variant>", {"std::variant"});
    add("<any>", {"std::any"});
    add("<memory>", {"std::allocator", "std::unique_ptr", "std::shared_ptr", "std::weak_ptr",
                     "std::default_delete"});
    add("<functional>", {"std::function", "std::reference_wrapper", "std::hash", "std::less",
                         "std::equal_to", "std::greater"});
    add("<mutex>", {"std::mutex", "std::recursive_mutex", "std::unique_lock", "std::lock_guard"});
    add("<shared_mutex>", {"std::shared_mutex"});
    add("<condition_variable>", {"std::condition_variable"});
    add("<thread>", {"std::thread"});
    add("<atomic>", {"std::atomic"});
    add("<chrono>", {"std::chrono::duration", "std::chrono::time_point"});
    add("<bitset>", {"std::bitset"});
    add("<complex>", {"std::complex"});
    add("<ostream>", {"std::basic_ostream"});
    add("<istream>", {"std::basic_istream"});
    add("<sstream>", {"std::basic_ostringstream", "std::basic_istringstream",
                      "std::basic_stringstream", "std::basic_stringbuf"});
    add("<fstream>", {"std::basic_ofstream", "std::basic_ifstream", "std::basic_fstream"});
    add("<exception>", {"std::exception"});
    add("<stdexcept>", {"std::runtime_error", "std::logic_error", "std::invalid_argument",
                        "std::out_of_range", "std::length_error", "std::domain_error",
                        "std::range_error", "std::overflow_error", "std::underflow_error"});
    add("<system_error>", {"std::error_code", "std::error_condition", "std::system_error"});
    add("<filesystem>", {"std::filesystem::path"});
    add("<locale>", {"std::locale"});
    add("<iterator>", {"__gnu_cxx::__normal_iterator", "std::reverse_iterator",
                       "std::move_iterator"});
    add("<cxxabi.h>", {"__cxxabiv1::__class_type_info", "__cxxabiv1::__si_class_type_info",
                       "__cxxabiv1::__vmi_class_type_info"});
    return table;
  }();
  return entries;
}

// "RECORD=HEADER". RECORD is a qualified record name without template arguments or inline
// ABI namespaces; HEADER is "<name>" or an absolute path. Returns an empty string on success.
inline std::string parse_entry(std::string_view text, Entry &entry) {
  const auto equals = text.find('=');
  if (equals == std::string_view::npos || equals == 0 || equals + 1 == text.size())
    return "expected RECORD=HEADER, for example std::flat_map=<flat_map>";
  entry.record.assign(text.substr(0, equals));
  entry.header.assign(text.substr(equals + 1));
  if (!standard_library_scope(entry.record))
    return "record '" + entry.record + "' is not a standard-library record (std::, __gnu_cxx:: or __cxxabiv1::)";
  if (entry.record.find_first_of("<>(), \"") != std::string::npos ||
      entry.record.find(":::") != std::string::npos || entry.record.back() == ':')
    return "record '" + entry.record + "' must be a qualified name without template arguments";
  if (entry.record != primary_name(entry.record))
    return "record '" + entry.record + "' names an inline ABI namespace; name the record without it";
  const auto &header = entry.header;
  const bool angled = header.size() > 2 && header.front() == '<' && header.back() == '>' &&
                      header.find_first_of("\"\r\n\0<>", 1, 6) == header.size() - 1;
  const bool path = header.size() > 1 && header.front() == '/' && header.size() <= 4095 &&
                    header.find_first_of("\"\r\n") == std::string::npos &&
                    header.find('\0') == std::string::npos;
  if (!angled && !path)
    return "header '" + header + "' must be <name> or an absolute path";
  return {};
}

// The mapped header for a display name, or nullptr. A user entry for the same record wins.
inline const std::string *find_header(std::string_view display,
                                      const std::vector<Entry> &user_entries,
                                      std::string &key) {
  key = primary_name(display);
  for (auto it = user_entries.rbegin(); it != user_entries.rend(); ++it)
    if (it->record == key)
      return &it->header;
  for (const auto &entry : builtin_entries())
    if (entry.record == key)
      return &entry.header;
  return nullptr;
}

inline std::string include_line(const std::string &header) {
  return "#include " + (header.front() == '/' ? "\"" + header + "\"" : header) + "\n";
}

}  // namespace tracer::standard_headers

#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tracer::standard_headers {

// Maps a standard-library record recovered from DWARF to the real public header that
// defines it. The record is named by its primary template: template arguments and the
// ABI-bearing inline namespaces (std::__cxx11, std::__1, std::_V2, ...) are not part of
// the key, because the real header selects the namespace through the target's ABI macros.
// The table also maps standard-library enumerations (std::errc, std::launch, ...), which a
// callable can take or return by value; they are keyed the same way, by qualified name.
// The table is a starting point; a caller extends or overrides it with "RECORD=HEADER"
// entries (see parse_entry()).
struct Entry {
  std::string record;  // For example "std::basic_string"
  std::string header;  // "<string>" or a quoted absolute path
  // The first C++ standard that has the record (17, 20, 23, ...); 0 when it is older than
  // C++20 or unknown. A target that uses the record was built in at least that mode.
  int standard = 0;
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
    const auto add = [&](const char *header, std::initializer_list<const char *> records,
                         int standard = 0) {
      for (const auto *record : records)
        table.push_back({record, header, standard});
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
    add("<utility>", {"std::pair", "std::in_place_t", "std::piecewise_construct_t",
                      "std::integer_sequence"});
    add("<tuple>", {"std::tuple"});
    add("<optional>", {"std::optional", "std::nullopt_t"});
    add("<variant>", {"std::variant", "std::monostate"});
    add("<any>", {"std::any"});
    add("<memory>", {"std::allocator", "std::unique_ptr", "std::shared_ptr", "std::weak_ptr",
                     "std::default_delete", "std::enable_shared_from_this", "std::owner_less",
                     "std::allocator_traits", "std::pointer_traits"});
    add("<functional>", {"std::function", "std::reference_wrapper", "std::hash", "std::less",
                         "std::equal_to", "std::greater", "std::not_equal_to", "std::less_equal",
                         "std::greater_equal", "std::plus", "std::minus", "std::multiplies",
                         "std::divides", "std::modulus", "std::negate", "std::logical_and",
                         "std::logical_or", "std::logical_not"});
    add("<functional>", {"std::move_only_function"}, 23);
    add("<mutex>", {"std::mutex", "std::recursive_mutex", "std::timed_mutex",
                    "std::recursive_timed_mutex", "std::unique_lock", "std::lock_guard",
                    "std::scoped_lock", "std::once_flag"});
    add("<shared_mutex>", {"std::shared_mutex", "std::shared_timed_mutex", "std::shared_lock"});
    add("<condition_variable>", {"std::condition_variable", "std::condition_variable_any",
                                 "std::cv_status"});
    add("<thread>", {"std::thread"});
    add("<thread>", {"std::jthread"}, 20);
    add("<stop_token>", {"std::stop_token", "std::stop_source", "std::stop_callback"}, 20);
    add("<future>", {"std::future", "std::shared_future", "std::promise", "std::packaged_task",
                     "std::launch", "std::future_errc", "std::future_status"});
    add("<atomic>", {"std::atomic", "std::atomic_flag", "std::memory_order"});
    add("<chrono>", {"std::chrono::duration", "std::chrono::time_point",
                     "std::chrono::system_clock", "std::chrono::steady_clock",
                     "std::chrono::high_resolution_clock"});
    add("<ratio>", {"std::ratio"});
    add("<type_traits>", {"std::integral_constant"});
    add("<initializer_list>", {"std::initializer_list"});
    add("<typeinfo>", {"std::type_info", "std::bad_cast", "std::bad_typeid"});
    add("<typeindex>", {"std::type_index"});
    add("<new>", {"std::bad_alloc", "std::bad_array_new_length", "std::nothrow_t",
                  "std::align_val_t"});
    add("<limits>", {"std::numeric_limits", "std::float_round_style",
                     "std::float_denorm_style"});
    add("<cstddef>", {"std::byte"});
    add("<charconv>", {"std::chars_format"});
    add("<bitset>", {"std::bitset"});
    add("<complex>", {"std::complex"});
    add("<valarray>", {"std::valarray"});
    add("<span>", {"std::span"}, 20);
    add("<source_location>", {"std::source_location"}, 20);
    add("<compare>", {"std::strong_ordering", "std::weak_ordering", "std::partial_ordering"}, 20);
    add("<expected>", {"std::expected", "std::unexpected", "std::bad_expected_access"}, 23);
    add("<ios>", {"std::ios_base", "std::basic_ios", "std::_Ios_Fmtflags", "std::_Ios_Openmode",
                  "std::_Ios_Iostate", "std::_Ios_Seekdir", "std::io_errc"});
    add("<streambuf>", {"std::basic_streambuf"});
    add("<ostream>", {"std::basic_ostream"});
    add("<istream>", {"std::basic_istream", "std::basic_iostream"});
    add("<sstream>", {"std::basic_ostringstream", "std::basic_istringstream",
                      "std::basic_stringstream", "std::basic_stringbuf"});
    add("<fstream>", {"std::basic_ofstream", "std::basic_ifstream", "std::basic_fstream",
                      "std::basic_filebuf"});
    add("<exception>", {"std::exception", "std::bad_exception", "std::nested_exception",
                        "std::__exception_ptr::exception_ptr"});
    add("<stdexcept>", {"std::runtime_error", "std::logic_error", "std::invalid_argument",
                        "std::out_of_range", "std::length_error", "std::domain_error",
                        "std::range_error", "std::overflow_error", "std::underflow_error"});
    add("<system_error>", {"std::error_code", "std::error_condition", "std::system_error",
                           "std::error_category", "std::errc"});
    add("<filesystem>", {"std::filesystem::path", "std::filesystem::directory_entry",
                         "std::filesystem::directory_iterator",
                         "std::filesystem::recursive_directory_iterator",
                         "std::filesystem::file_status", "std::filesystem::filesystem_error",
                         "std::filesystem::file_type", "std::filesystem::perms",
                         "std::filesystem::perm_options", "std::filesystem::copy_options",
                         "std::filesystem::directory_options"});
    add("<locale>", {"std::locale", "std::codecvt_base::result"});
    add("<regex>", {"std::basic_regex", "std::match_results", "std::sub_match",
                    "std::regex_constants::syntax_option_type",
                    "std::regex_constants::match_flag_type",
                    "std::regex_constants::error_type"});
    add("<iterator>", {"__gnu_cxx::__normal_iterator", "std::reverse_iterator",
                       "std::move_iterator", "std::back_insert_iterator",
                       "std::front_insert_iterator", "std::insert_iterator"});
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

// The mapping for a display name, or nullptr. A user entry for the same record wins.
inline const Entry *find_entry(std::string_view display, const std::vector<Entry> &user_entries,
                               std::string &key) {
  key = primary_name(display);
  for (auto it = user_entries.rbegin(); it != user_entries.rend(); ++it)
    if (it->record == key)
      return &*it;
  for (const auto &entry : builtin_entries())
    if (entry.record == key)
      return &entry;
  return nullptr;
}

// The mapped header for a display name, or nullptr.
inline const std::string *find_header(std::string_view display,
                                      const std::vector<Entry> &user_entries,
                                      std::string &key) {
  const auto *entry = find_entry(display, user_entries, key);
  return entry ? &entry->header : nullptr;
}

inline std::string include_line(const std::string &header) {
  return "#include " + (header.front() == '/' ? "\"" + header + "\"" : header) + "\n";
}

// The C++ standard a translation unit was compiled as, when its DW_AT_producer says so: GCC
// names it ("GNU C++17 8.5.0 ..."), and a recorded command line carries -std=. Two digits
// (11, 14, 17, 20, 23, 26); 0 when the producer does not say.
inline int producer_cxx_standard(const std::string &producer) {
  const auto year = [](std::string_view text) {
    static const std::pair<const char *, int> names[] = {
        {"98", 3},  {"03", 3},  {"0x", 11}, {"11", 11}, {"1y", 14}, {"14", 14}, {"1z", 17},
        {"17", 17}, {"2a", 20}, {"20", 20}, {"2b", 23}, {"23", 23}, {"2c", 26}, {"26", 26}};
    for (const auto &[name, value] : names)
      if (text.substr(0, 2) == name)
        return value;
    return 0;
  };
  int result = 0;
  for (const std::string_view marker : {"-std=c++", "-std=gnu++", "GNU C++"}) {
    for (auto at = producer.find(marker); at != std::string::npos;
         at = producer.find(marker, at + 1))
      if (const auto found = year(std::string_view(producer).substr(at + marker.size())))
        result = found;
    if (result)
      return result;
  }
  return result;
}

// Notes which libstdc++ dual-ABI classes a DWARF type name mentions, anywhere in it. Only the
// classes libstdc++ really keeps in two ABIs count: std::basic_string_view or a program type
// called basic_string is neither. A class inside another inline namespace (libc++'s
// std::__1::basic_string) is not libstdc++'s old ABI and is left out.
inline void note_dual_abi_names(std::string_view name, bool &seen_new, bool &seen_old) {
  static const std::set<std::string> dual = {
      "std::basic_string", "std::list", "std::basic_stringbuf", "std::basic_istringstream",
      "std::basic_ostringstream", "std::basic_stringstream"};
  const auto word = [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
  };
  for (std::size_t i = 0; i < name.size();) {
    if (!word(name[i]) || (name[i] >= '0' && name[i] <= '9')) {
      ++i;
      continue;
    }
    const auto begin = i;
    while (i < name.size()) {
      if (word(name[i]))
        ++i;
      else if (name[i] == ':' && i + 2 < name.size() && name[i + 1] == ':' && word(name[i + 2]))
        i += 2;
      else
        break;
    }
    // A name that continues a longer qualified name (app::std::list) is not the library's.
    if (begin >= 2 && name[begin - 1] == ':' && name[begin - 2] == ':')
      continue;
    const auto token = name.substr(begin, i - begin);
    if (token.substr(0, 5) != "std::")
      continue;
    const auto primary = primary_name(token);
    if (!dual.count(primary))
      continue;
    if (token.find("::__cxx11::") != std::string_view::npos)
      seen_new = true;
    else if (primary == token)
      seen_old = true;
  }
}

// The _GLIBCXX_USE_CXX11_ABI the caller's options set: 0 or 1, -1 when they leave it to the
// library (not mentioned, or undefined last), -2 when they set it to something else. Only the
// macro itself counts, in the -DNAME[=VALUE], -D NAME[=VALUE] and -U forms.
template <class Options> inline int explicit_cxx11_abi(const Options &options) {
  constexpr std::string_view macro = "_GLIBCXX_USE_CXX11_ABI";
  int result = -1;
  for (auto it = options.begin(); it != options.end(); ++it) {
    const std::string_view option(*it);
    const bool define = option.substr(0, 2) == "-D";
    if (!define && option.substr(0, 2) != "-U")
      continue;
    std::string_view definition = option.substr(2);
    if (definition.empty()) {
      if (std::next(it) == options.end())
        break;
      definition = *++it;
    }
    const auto equals = definition.find('=');
    if (definition.substr(0, equals) != macro)
      continue;
    if (!define) {
      result = -1;
      continue;
    }
    const auto value = equals == std::string_view::npos ? std::string_view("1")
                                                        : definition.substr(equals + 1);
    result = value == "0" ? 0 : value == "1" ? 1 : -2;
  }
  return result;
}

}  // namespace tracer::standard_headers

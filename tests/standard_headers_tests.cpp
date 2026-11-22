// The standard-library header map: how a DWARF record name becomes a lookup key, which headers
// the built-in table names, and what a caller-supplied entry may and may not say.
#include "standard_headers.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace sh = tracer::standard_headers;

namespace {

void require(bool condition, const std::string &what) {
  if (!condition)
    throw std::runtime_error(what);
}

void key(const std::string &display, const std::string &expected) {
  require(sh::primary_name(display) == expected,
          "primary_name(" + display + ") = " + sh::primary_name(display) + ", expected " + expected);
}

const std::string *header(const std::string &display, const std::vector<sh::Entry> &user = {}) {
  static std::string scratch;
  return sh::find_header(display, user, scratch);
}

}  // namespace

int main() {
  // Template arguments and ABI-bearing inline namespaces are not part of the key.
  key("std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >",
      "std::basic_string");
  key("std::basic_string<char, std::char_traits<char>, std::allocator<char> >", "std::basic_string");
  key("std::vector<int, std::allocator<int> >", "std::vector");
  key("std::array<int, 4>", "std::array");
  key("std::__1::vector<int>", "std::vector");
  key("std::filesystem::__cxx11::path", "std::filesystem::path");
  key("std::map<int, std::vector<std::string>, std::less<int>>", "std::map");
  key("stdh::Person", "stdh::Person");

  require(sh::standard_library_scope("std::vector<int>"), "std:: is the standard library");
  require(sh::standard_library_scope("__gnu_cxx::__normal_iterator<int*, std::vector<int>>"),
          "__gnu_cxx:: is the standard library");
  require(sh::standard_library_scope("__cxxabiv1::__class_type_info"), "__cxxabiv1::");
  require(!sh::standard_library_scope("stdh::Person") && !sh::standard_library_scope("stdx::y"),
          "a program record is not standard library");

  // Built-in mapping.
  require(header("std::__cxx11::basic_string<char>") && *header("std::__cxx11::basic_string<char>") == "<string>",
          "std::string maps to <string>");
  require(header("std::vector<int>") && *header("std::vector<int>") == "<vector>", "<vector>");
  require(header("std::array<int, 4>") && *header("std::array<int, 4>") == "<array>", "<array>");
  require(header("std::map<int, int>") && *header("std::map<int, int>") == "<map>", "<map>");
  require(header("std::chrono::duration<long>") && *header("std::chrono::duration<long>") == "<chrono>",
          "<chrono>");
  require(header("std::uniform_real_distribution<double>") == nullptr,
          "a record outside the table has no mapping");
  require(header("std::ratio<1, 1000>") && *header("std::ratio<1, 1000>") == "<ratio>", "<ratio>");
  require(header("std::chrono::_V2::system_clock") &&
              *header("std::chrono::_V2::system_clock") == "<chrono>",
          "the clocks come from <chrono>");
  require(header("std::basic_string_view<char>") &&
              *header("std::basic_string_view<char>") == "<string_view>",
          "<string_view>");
  // A record that only a later standard has says which one.
  {
    std::string scratch;
    const auto *expected = sh::find_entry("std::expected<int, std::error_code>", {}, scratch);
    require(expected && expected->header == "<expected>" && expected->standard == 23,
            "std::expected needs C++23");
    const auto *span = sh::find_entry("std::span<int, 18446744073709551615>", {}, scratch);
    require(span && span->header == "<span>" && span->standard == 20, "std::span is C++20");
    const auto *vector = sh::find_entry("std::vector<int>", {}, scratch);
    require(vector && vector->standard == 0, "std::vector needs no newer standard");
  }

  // A caller's entry extends the table, and overrides a built-in one.
  sh::Entry extra;
  require(sh::parse_entry("std::uniform_real_distribution=<random>", extra).empty(), "valid entry");
  require(extra.record == "std::uniform_real_distribution" && extra.header == "<random>", "fields");
  require(header("std::uniform_real_distribution<double>", {extra}) &&
              *header("std::uniform_real_distribution<double>", {extra}) == "<random>",
          "extension used");
  sh::Entry override_entry;
  require(sh::parse_entry("std::vector=/opt/vendor/vector.hpp", override_entry).empty(), "path entry");
  require(*header("std::vector<int>", {override_entry}) == "/opt/vendor/vector.hpp",
          "a user entry wins over the built-in one");

  // What an entry may not say.
  const auto rejected = [](const std::string &text, const std::string &reason) {
    sh::Entry entry;
    const auto problem = sh::parse_entry(text, entry);
    require(!problem.empty(), "accepted '" + text + "'");
    require(problem.find(reason) != std::string::npos,
            "'" + text + "' rejected for the wrong reason: " + problem);
  };
  rejected("std::vector", "RECORD=HEADER");
  rejected("=<vector>", "RECORD=HEADER");
  rejected("std::vector=", "RECORD=HEADER");
  rejected("app::Thing=<thing>", "not a standard-library record");
  rejected("std::vector<int>=<vector>", "without template arguments");
  rejected("std::__cxx11::basic_string=<string>", "inline ABI namespace");
  rejected("std::vector=vector", "must be <name> or an absolute path");
  rejected("std::vector=<vec\"tor>", "must be <name> or an absolute path");
  rejected("std::vector=relative/vector.hpp", "must be <name> or an absolute path");
  rejected("std::vector=\"/abs/vector.hpp\"", "must be <name> or an absolute path");

  // Which libstdc++ string ABI a DWARF type name shows: only the classes libstdc++ keeps in
  // two ABIs count, wherever they appear in the name.
  const auto abi = [](const std::string &name) {
    bool seen_new = false, seen_old = false;
    sh::note_dual_abi_names(name, seen_new, seen_old);
    return std::string(seen_new ? "new" : "") + (seen_old ? "old" : "");
  };
  require(abi("std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >") == "new",
          "std::__cxx11::basic_string is the C++11 ABI");
  require(abi("std::basic_string<char, std::char_traits<char>, std::allocator<char> >") == "old",
          "std::basic_string is the old ABI");
  require(abi("std::vector<std::__cxx11::basic_string<char>, std::allocator<std::__cxx11::basic_string<char> > >") == "new",
          "a string inside a container counts");
  require(abi("std::__cxx11::list<int, std::allocator<int> >") == "new", "std::list is dual-ABI");
  require(abi("std::basic_string_view<char, std::char_traits<char> >").empty(),
          "std::string_view is not a dual-ABI class");
  require(abi("std::vector<std::basic_string_view<char>, std::allocator<std::basic_string_view<char> > >").empty(),
          "a string_view inside a container says nothing either");
  require(abi("std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >").empty(),
          "libc++'s std::__1::basic_string is not libstdc++'s old ABI");
  require(abi("app::basic_string<char>").empty() && abi("app::std::list<int>").empty(),
          "a program type of the same name does not count");
  require(abi("std::vector<int, std::allocator<int> >").empty(), "no string, no evidence");

  // Which _GLIBCXX_USE_CXX11_ABI the caller's options set: the macro itself, in every form.
  using Options = std::vector<std::string>;
  require(sh::explicit_cxx11_abi(Options{}) == -1, "no option");
  require(sh::explicit_cxx11_abi(Options{"-D_GLIBCXX_USE_CXX11_ABI=0"}) == 0, "=0");
  require(sh::explicit_cxx11_abi(Options{"-D_GLIBCXX_USE_CXX11_ABI=1"}) == 1, "=1");
  require(sh::explicit_cxx11_abi(Options{"-D_GLIBCXX_USE_CXX11_ABI"}) == 1, "defined is 1");
  require(sh::explicit_cxx11_abi(Options{"-D", "_GLIBCXX_USE_CXX11_ABI=0"}) == 0, "split -D");
  require(sh::explicit_cxx11_abi(Options{"-D_GLIBCXX_USE_CXX11_ABI=2"}) == -2, "another value");
  require(sh::explicit_cxx11_abi(Options{"-D_GLIBCXX_USE_CXX11_ABI_OTHER=0"}) == -1,
          "a longer macro name is another macro");
  require(sh::explicit_cxx11_abi(Options{"-DX_GLIBCXX_USE_CXX11_ABI=0", "-O2"}) == -1,
          "a name that merely contains the macro is another macro");
  require(sh::explicit_cxx11_abi(Options{"-D_GLIBCXX_USE_CXX11_ABI=0", "-U_GLIBCXX_USE_CXX11_ABI"}) == -1,
          "-U leaves the choice to the library again");
  require(sh::explicit_cxx11_abi(Options{"-U_GLIBCXX_USE_CXX11_ABI", "-D_GLIBCXX_USE_CXX11_ABI=0"}) == 0,
          "the last option wins");
  require(sh::explicit_cxx11_abi(Options{"-D"}) == -1, "a dangling -D is not a definition");

  // The C++ standard a producer string names.
  require(sh::producer_cxx_standard("GNU C++17 8.5.0 -mtune=generic -march=x86-64 -g -std=gnu++17") == 17,
          "GCC names the standard");
  require(sh::producer_cxx_standard("GNU C++23 15.1.1 -mtune=generic -g") == 23, "GNU C++23");
  require(sh::producer_cxx_standard("GNU C++14 8.5.0 -g -std=c++2a") == 20, "-std= wins");
  require(sh::producer_cxx_standard("clang version 21.1.8") == 0, "Clang does not say");
  require(sh::producer_cxx_standard("clang version 21.1.8 -std=c++2b") == 23,
          "a recorded command line does");
  require(sh::producer_cxx_standard("GNU C17 8.5.0 -g") == 0, "C is not C++");
  require(sh::producer_cxx_standard("") == 0, "no producer");

  require(sh::include_line("<vector>") == "#include <vector>\n", "angled include");
  require(sh::include_line("/opt/vendor/v.hpp") == "#include \"/opt/vendor/v.hpp\"\n", "path include");
  std::cout << "standard header map passed\n";
}

// The standard-library header map: how a DWARF record name becomes a lookup key, which headers
// the built-in table names, and what a caller-supplied entry may and may not say.
#include "standard_headers.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

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

  require(sh::include_line("<vector>") == "#include <vector>\n", "angled include");
  require(sh::include_line("/opt/vendor/v.hpp") == "#include \"/opt/vendor/v.hpp\"\n", "path include");
  std::cout << "standard header map passed\n";
}

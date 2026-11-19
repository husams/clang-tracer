#include "cpp_type_spelling.hpp"
#include <iostream>
#include <stdexcept>

int main() {
  const auto equal = [](const char *a, const char *b, bool expected = true) {
    if ((tracer::normalized_cpp_type_spelling(a) ==
         tracer::normalized_cpp_type_spelling(b)) != expected)
      throw std::runtime_error(std::string("type identity mismatch: ") + a + " / " + b);
  };
  equal("std::allocator<std::pair<const int,std::vector<int,std::allocator<int>>>>",
        "std::allocator<std::pair<int const, std::vector<int, std::allocator<int>>>>");
  equal("const volatile long * const", "long volatile const*const");
  equal("std::pair<int const*, long volatile&>", "std::pair<const int *, volatile long &>");
  equal("const int*", "int*const", false);
  equal("const int**", "int*const*", false);
  equal("std::__cxx11::basic_string<char>", "std::basic_string<char>", false);
  equal("std::pair<const int,long>", "std::pair<const long,int>", false);
  equal("int (*)(const int*)", "int (*)(int*const)", false);
  const auto library = [](const char *type, bool expected) {
    if (tracer::standard_library_type_spelling(type) != expected)
      throw std::runtime_error(std::string("library type evidence mismatch: ") + type);
  };
  library("std::char_traits<char>", true);
  library("std::basic_ostream<char, std::char_traits<char> >", true);
  library("std::basic_ostream<wchar_t, std::char_traits<wchar_t> >", true);
  library("std::hash<NodebugHashKey>", false);
  library("std::hash<app::Key>", false);
  library("std::vector<std::hash<app::Key>>", false);
  library("app::char_traits<char>", false);
  library("std::hash<>", false);
  library("std::hash<int,>", false);
  library("std::hash<int>>", false);
  std::cout << "C++ type spelling identity checks passed\n";
}

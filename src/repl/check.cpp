#include "completion.hpp"

#include <csignal>
#include <iostream>
#include <iterator>
#include <llvm/Support/TargetSelect.h>
#include <stdexcept>
#include <unistd.h>

int main(int argc, char **argv) {
  try {
    // Bound its lifetime even if the console disappears unexpectedly.
    std::signal(SIGALRM, SIG_DFL);
    ::alarm(15);
    if (argc < 3)
      return 1;
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    const std::vector<std::string> arguments(argv + 3, argv + argc);
    if (std::string(argv[1]) == "serve") {
      tracer_repl::serve_cpp_checks(arguments);
      return 0;
    }
    const std::string content{std::istreambuf_iterator<char>(std::cin), {}};
    const auto offset = std::stoull(argv[2]);
    if (offset > content.size())
      return 1;
    if (std::string(argv[1]) == "complete") {
      const auto result = tracer_repl::complete_cpp(
          arguments, content.substr(0, offset), content.substr(offset));
      std::cout << result.prefix << '\n';
      for (const auto &item : result.items)
        std::cout << item << '\n';
      return 0;
    }
    if (std::string(argv[1]) != "check" && std::string(argv[1]) != "reflect")
      return 1;
    std::vector<std::string> history;
    std::size_t cursor = 0;
    while (cursor < offset) {
      const auto end = content.find('\n', cursor);
      if (end == std::string::npos || end >= offset)
        throw std::runtime_error("invalid checker history");
      const auto length = std::stoull(content.substr(cursor, end - cursor));
      cursor = end + 1;
      if (length > offset - cursor)
        throw std::runtime_error("invalid checker cell length");
      history.push_back(content.substr(cursor, length));
      cursor += length;
    }
    if (std::string(argv[1]) == "reflect") {
      std::cout << tracer_repl::reflect_cpp_header(arguments, history,
                                                   content.substr(offset));
      return 0;
    }
    return tracer_repl::check_cpp_source(arguments, history,
                                         content.substr(offset))
               ? 2
               : 0;
  } catch (const std::exception &error) {
    std::cerr << "C++ check: " << error.what() << '\n';
    return 1;
  }
}

#include "controller.hpp"
#include "shell.hpp"
#include <iostream>
#include <stdexcept>

int main() {
  using namespace tracer_repl;
  const auto require = [](bool ok) {
    if (!ok)
      throw std::runtime_error("compiler profile helper check failed");
  };
  const auto flags = split_compiler_flags(
      "-I\"path with spaces\" -DNAME='hello world' -DEMPTY= foo\\ bar");
  require(flags == std::vector<std::string>{"-Ipath with spaces",
                                            "-DNAME=hello world",
                                            "-DEMPTY=", "foo bar"});
  for (const auto *bad : {"\"unfinished", "trailing\\"}) {
    bool rejected = false;
    try {
      (void)split_compiler_flags(bad);
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    require(rejected);
  }
  auto output =
      run_shell("printf compiler-flags; printf diagnostics >&2; exit 7");
  require(output.status == 7 && output.output == "compiler-flags" &&
          output.error == "diagnostics");
  interrupted = true;
  bool cancelled = false;
  try {
    (void)run_shell("sleep 30");
  } catch (const std::runtime_error &) {
    cancelled = true;
  }
  interrupted = false;
  require(cancelled);
  bool bounded = false;
  try {
    (void)run_shell("yes x | head -c 65537");
  } catch (const std::runtime_error &) {
    bounded = true;
  }
  require(bounded);
  std::cout
      << "shell quoting, independent streams, cancellation and limits passed\n";
}

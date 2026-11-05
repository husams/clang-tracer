#include <iostream>
#include <stdexcept>

#include "commands.hpp"

using namespace tracer_repl;

int main() {
  auto require = [](bool ok) {
    if (!ok) {
      throw std::runtime_error("command check failed");
    }
  };
  auto bad = [&](const std::string &s) {
    try {
      parse_command(s);
    } catch (const std::exception &) {
      return;
    }
    throw std::runtime_error("accepted malformed command: " + s);
  };
  require(parse_command("%attach 12345").kind == CommandKind::attach);
  auto p = parse_command(
      "%probe inject \"Calculator::multiply(int, int) const\" --callback 'my "
      "callback.cpp' --stack-depth 16");
  require(p.kind == CommandKind::inject);
  require(p.arguments.at(0) == "Calculator::multiply(int, int) const");
  require(p.option("--callback") == "my callback.cpp");
  require(p.number("--stack-depth", 0) == 16);
  require(!parse_command("%events").has("--count"));
  require(!parse_command("%collect --as live").has("--count"));
  require(parse_command("%collect --as cpp").option("--as") == "cpp");
  require(parse_command("%help Event").arguments.at(0) == "Event");
  require(parse_command("%complete \"data.at(0).\"").kind ==
          CommandKind::complete);
  bad("%complete");
  require(
      parse_command(
          "%cpp import \"app/types.hpp\" --type app::Order --type app::Address")
          .kind == CommandKind::cpp_import);
  auto options = parse_command("%cpp options add \"-I/path with spaces\" "
                               "\"-DFEATURE=1\" --sysroot=/sdk");
  require(options.kind == CommandKind::cpp_options &&
          options.arguments.size() == 4);
  require(options.arguments.at(1) == "-I/path with spaces");
  require(parse_command("%cpp options").arguments.empty());
  bad("%cpp options add");
  bad("%cpp options clear trailing");
  bad("%cpp import");
  require(parse_command("%events show batch --index 0").kind ==
          CommandKind::event_show);
  bad("%attach 12345 trailing");
  bad("%probe inject \"unfinished --callback f.cpp");
  bad("%collect --count 3 --count 4");
  bad("%collect --count 0");
  bad("%events --timeout -1");
  bad("%events --phase UNKNOWN");
  bad("%events --all --session other");
  bad("%server stop --agent nope");
  bad("%attach 99999999999999999999999999999999999999");
  bad("%collect --count 10 %detach");
  bad("%record");
  bad("%record \"\"");
  bad("%events --probe 0");
  bad("%events --after 999999999999999999999999999999999");
  bad("%symbols --limit 4294967296");
  std::cout << "native REPL grammar checks passed\n";
}

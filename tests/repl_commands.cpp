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
  const auto launch = parse_command(
      "%launch ./app --arg \"--first\" --arg \"\" --env VAR=1 --env \"VAR=two words\" --timeout 5");
  require(launch.kind == CommandKind::launch && launch.arguments.at(0) == "./app" &&
          launch.options.at("--arg") == std::vector<std::string>{"--first", ""} &&
          launch.options.at("--env") == std::vector<std::string>{"VAR=1", "VAR=two words"} &&
          launch.number("--timeout", 0) == 5);
  require(parse_command("%resume").arguments.empty());
  require(parse_command("%terminate 123").kind == CommandKind::terminate);
  bad("%launch app --cwd here --cwd there");
  bad("%launch app --timeout 0");
  bad("%resume 0");
  bad("%terminate 2147483648");
  require(parse_command("%probe prepare \"demo::compute(int)\" --include api.hpp "
      "--compdb build --translation-unit src/api.cpp --module app --output prepared").kind
          == CommandKind::prepare);
  const auto prepare = parse_command("%probe prepare \"demo::compute(int)\" --include api.hpp "
      "--output prepared");
  require(prepare.kind == CommandKind::prepare && !prepare.has("--compdb") &&
          !prepare.has("--translation-unit") && !prepare.has("--module"));
  bad("%probe prepare f --include api.hpp --compdb \"\" "
      "--translation-unit src/api.cpp --module app --output prepared");
  bad("%probe prepare f --include api.hpp --translation-unit \"\" --module app --output prepared");
  require(parse_command("%probe inject \"demo::compute(int)\" --callback hook.cpp "
      "--prepared prepared --prepared-mode require").option("--prepared-mode") == "require");
  bad("%probe prepare \"demo::compute(int)\" --include api.hpp");
  const auto inject = parse_command("%probe inject f --callback h --prepared-mode require");
  require(inject.option("--prepared-mode") == "require" && !inject.has("--prepared"));
  bad("%probe inject f --callback h --prepared-mode require --prepared \"\"");
  bad("%probe inject f --callback h --prepared d --prepared-mode typo");
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
  require(parse_command("%help api tracer_repl::Event").arguments ==
          std::vector<std::string>{"api", "tracer_repl::Event"});
  require(parse_command("%help %probe inject").arguments ==
          std::vector<std::string>{"probe", "inject"});
  bad("%help %");
  require(parse_command("%complete \"data.at(0).\"").kind ==
          CommandKind::complete);
  bad("%complete");
  require(parse_command("%type tracer.events_async").kind == CommandKind::type);
  require(parse_command("%type std::string").arguments.at(0) == "std::string");
  bad("%type");
  bad("%type name another");
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
  require(parse_command("%cpp generate app.hpp --output mappings.hpp").kind == CommandKind::cpp_generate);
  require(parse_command("%cpp options compdb build --file src/app.hpp").option("--file") == "src/app.hpp");
  auto shell_flags = parse_command("%cpp options add `cat flags.txt` \"-DMORE=1\"");
  require(shell_flags.shell_arguments.size() == 1 && shell_flags.shell_arguments.at(0) == 1);
  require(shell_flags.arguments.at(1) == "cat flags.txt");
  require(parse_command("%shell \"printf hello\"").kind == CommandKind::shell);
  bad("%cpp generate app.hpp --output one --output two");
  bad("%cpp options compdb build");
  bad("%cpp options add `unfinished");
  bad("%cpp import `cat flags.txt`");
  require(parse_command("%events show batch --index 0").kind ==
          CommandKind::event_show);
  const auto draft_check = parse_command(
      "%interceptor validate hook --target \"demo::compute(int)\" "
      "--prepared-mode require");
  require(draft_check.kind == CommandKind::draft_validate &&
          draft_check.arguments.at(0) == "hook" &&
          draft_check.option("--target") == "demo::compute(int)" &&
          draft_check.option("--prepared-mode") == "require");
  bad("%interceptor validate hook");
  bad("%interceptor validate hook --target compute --stack-depth 4");
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
  const auto sqlite = parse_command("%record capture.db --count 10 --sqlite --name account.adjusted");
  require(sqlite.kind == CommandKind::record && sqlite.arguments.at(0) == "capture.db" &&
          sqlite.has("--sqlite") && sqlite.number("--count", 0) == 10 &&
          sqlite.option("--name") == "account.adjusted");
  bad("%record capture.db --sqlite --sqlite");
  bad("%collect --sqlite");
  bad("%events --probe 0");
  bad("%events --after 999999999999999999999999999999999");
  bad("%symbols --limit 4294967296");
  require(parse_command("%threads").kind == CommandKind::threads);
  require(parse_command("%modules --details").has("--details"));
  require(parse_command("%attach 42 --start-time 100").number("--start-time", 0) == 100);
  require(parse_command("%symbols --filter demo --addresses").has("--addresses"));
  require(address_number(parse_command("%symbols --address 0x100a --mapping s:token")
                             .option("--address")) == 0x100a);
  bad("%attach 42 --start-time 0");
  bad("%symbols --address 0x100a");
  bad("%symbols --address 0x100a --mapping token --page other");
  bad("%symbols --address 0xzz --mapping token");
  bad("%symbols --address 18446744073709551616 --mapping token");
  const auto bulk = parse_command(
      "%trace preview --module 'lib*.so' --module target --include 'app::*' "
      "--exclude 'app::detail::*' --exact 'app::run(int)' --save rules.textproto");
  require(bulk.kind == CommandKind::bulk_preview && bulk.options.at("--module").size() == 2);
  require(parse_command("%trace preview --manifest rules.textproto").has("--manifest"));
  bad("%trace preview --exclude 'app::*'");
  bad("%trace preview --manifest rules --exact 'app::run(int)'");
  bad("%trace preview --include ''");
  bad("%trace preview --include app --save one --save two");
  std::cout << "native REPL grammar checks passed\n";
}

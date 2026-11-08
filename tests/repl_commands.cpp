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
  require(parse_command("%probe prepare \"demo::compute(int)\" --include api.hpp "
      "--compdb build --translation-unit src/api.cpp --module app --output prepared").kind
          == CommandKind::prepare);
  const auto prepare = parse_command("%probe prepare \"demo::compute(int)\" --include api.hpp "
      "--output prepared");
  require(prepare.kind == CommandKind::prepare && !prepare.has("--compdb") &&
          !prepare.has("--translation-unit") && !prepare.has("--module"));
  using ExtraKind = ExtraCompileArgumentInput::Kind;
  const auto extra = parse_command(
      R"args(%probe prepare f --extra-args "-I generated -DFIRST=1" )args"
      R"args(--include api.hpp --extra-args -I'generated files' -DSECOND=2 `my-conf --cxxflags` )args"
      R"args(--extra-arg "-Ione more" --output prepared)args");
  const auto &inputs = extra.extra_compile_argument_inputs;
  require(extra.arguments == std::vector<std::string>{"f"} &&
          extra.option("--include") == "api.hpp" &&
          extra.option("--output") == "prepared" && inputs.size() == 4);
  require(inputs[0].kind == ExtraKind::list && inputs[0].text == "-I generated -DFIRST=1");
  require(inputs[1].kind == ExtraKind::list && inputs[1].text == "-I'generated files' -DSECOND=2");
  require(inputs[2].kind == ExtraKind::shell && inputs[2].text == "my-conf --cxxflags");
  require(inputs[3].kind == ExtraKind::argument && inputs[3].text == "-Ione more");
  require(parse_command("%probe inject f --callback hook.cpp --extra-arg -DHOOK=1 --print-result")
              .has("--print-result"));
  require(parse_command("%probe validate f --callback hook.cpp --extra-args -DHOOK=1")
              .extra_compile_argument_inputs.size() == 1);
  bad("%probe prepare f --include api.hpp --output prepared --extra-args");
  bad("%probe validate f --callback hook.cpp --extra-args `unfinished");
  bad("%server start --extra-arg -DHOOK=1");
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
  const auto full_stack = parse_command(
      "%interceptor upload hook --target compute --stack-depth full "
      "--stack-application-only false");
  require(full_stack.number("--stack-depth", 0) == UINT32_MAX);
  require(full_stack.option("--stack-application-only") == "false");
  require(parse_command("%server start --context-event-bytes 4194304")
              .number("--context-event-bytes", 0) == 4194304);
  bad("%server start --context-event-bytes 0");
  require(parse_command("%server start --context-event-bytes 25165824 "
                        "--context-transport-memory-bytes 234881024")
              .number("--context-transport-memory-bytes", 0) == 234881024);
  bad("%server start --context-transport-memory-bytes 0");
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
  const auto stack_view = parse_command("%events stack fullstack --index 3");
  require(stack_view.kind == CommandKind::event_stack &&
          stack_view.arguments.at(0) == "fullstack" &&
          stack_view.number("--index", 0) == 3);
  require(parse_command("%events stack stack --index 0").arguments.at(0) == "stack");
  bad("%events stack fullstack");
  bad("%events stack fullstack --index -1");
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

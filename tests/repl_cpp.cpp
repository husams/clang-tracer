#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unistd.h>

#include "check_process.hpp"
#include "completion.hpp"
#include "controller.hpp"
#include "events.hpp"
#include "interpreter.hpp"

using namespace tracer_repl;

namespace {
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
bool contains(const CppCompletions &results, const std::string &name) {
  return std::find(results.items.begin(), results.items.end(), name) !=
         results.items.end();
}
struct TemporaryDirectory {
  std::filesystem::path path;
  TemporaryDirectory() {
    char name[] = "/tmp/tracer-repl-input-test.XXXXXX";
    const auto *created = ::mkdtemp(name);
    if (!created)
      throw std::runtime_error("cannot create test directory");
    path = created;
  }
  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};
} // namespace

int main(int argc, char **argv) {
  if (argc > 1 && std::string(argv[1]) == "wait-checker") {
    for (;;)
      ::pause();
  }
  if (argc > 1 && std::string(argv[1]) == "serve") {
    if (argc > 3 && std::string(argv[3]) == "die")
      return 1;
    for (;;)
      ::pause();
  }
  try {
    std::ostringstream output;
    Controller controller(Settings{}, output);
    Facade facade(controller);
    CppEngine cpp(facade, {});
    require(incomplete_cpp("for (const auto &e : data) {"),
            "Enter continues an unfinished block");
    require(!incomplete_cpp("for (int e : {1,2}) { (void)e; }"),
            "a finished block can be submitted");
    require(!incomplete_cpp("const char *s = R\"tag({\"[)tag\";"),
            "raw strings do not open editor blocks");
    require(incomplete_cpp("/* unfinished comment"),
            "Enter continues an unfinished comment");
    require(!incomplete_cpp("#define OPEN {"),
            "macro definitions are submitted without balancing their bodies");
    require(incomplete_cpp("const char *s = \"unfinished"),
            "Enter continues an unfinished string");
    require(!incomplete_cpp("/* closed */ // not a block /*"),
            "comment contents do not create continuation blocks");
    const auto started = std::chrono::steady_clock::now();
    std::thread interrupt_check([] {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      interrupted = true;
    });
    bool cancelled = false;
    try {
      (void)run_cpp_analysis(executable_path(), "wait-checker", {}, "", "");
    } catch (const std::runtime_error &error) {
      cancelled =
          std::string(error.what()).find("cancelled") != std::string::npos;
    }
    interrupt_check.join();
    interrupted = false;
    require(cancelled && std::chrono::steady_clock::now() - started <
                             std::chrono::seconds(3),
            "interrupt cancels and reaps a stuck checker promptly");
    {
      CppCheckSession checks;
      std::thread interrupt_session([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        interrupted = true;
      });
      bool session_cancelled = false;
      try {
        (void)checks.check(executable_path(), {}, "", "1+1");
      } catch (const std::runtime_error &error) {
        session_cancelled = std::string(error.what()).find("cancelled") != std::string::npos;
      }
      interrupt_session.join();
      interrupted = false;
      require(session_cancelled, "persistent checks remain cancellable");
      bool stopped = false;
      try {
        (void)checks.check(executable_path(), {"die"}, "", "1+1");
      } catch (const std::runtime_error &) {
        stopped = true;
      }
      require(stopped, "a dead checker cannot terminate the console via SIGPIPE");
      const auto helper = std::filesystem::path(executable_path()).parent_path() / "clang-tracer-repl-check";
      require(checks.check(helper.string(), cpp.effective_compiler_options(), "", "1+1").status == 0,
              "a checker session restarts after cancellation or death");
    }
    const auto help = cpp.describe_api("api");
    require(help.find("result_as") != std::string::npos &&
                help.find("argument_as") != std::string::npos &&
                help.find("events") != std::string::npos &&
                help.find("print") != std::string::npos,
            "API help lists actual public declarations");
    require(help.find("argument_snapshot") != std::string::npos &&
                help.find("EventValue") != std::string::npos &&
                help.find("elements") != std::string::npos,
            "API help describes owned complex value inspection");
    require(help.find("scalar_as") == std::string::npos &&
                help.find("controller_") == std::string::npos,
            "API help hides implementation details");
    require(contains(cpp.complete("tracer."), "batch") &&
                contains(cpp.complete("tracer.pr"), "print") &&
                contains(cpp.complete("tracer.events"), "events_async"),
            "facade completion uses the active compiler");
    require(cpp.complete("tracer.pr").prefix == "pr",
            "only the identifier is replaced");
    require(cpp.evaluate("tracer_repl::Collection data;"),
            "declare a collection");
    auto members = cpp.complete("data.");
    require(contains(members, "events") && contains(members, "received") &&
                contains(members, "at"),
            "collection fields and methods are completed");
    require(contains(cpp.complete("if (true) {\n data.rec"), "received"),
            "multiline cursor positions use the final line and identifier");
    members = cpp.complete("data.at(0).");
    require(contains(members, "result_as") && contains(members, "describe") &&
                !contains(members, "data_"),
            "chained expression completion uses return types and hides private "
            "members");
    require(contains(cpp.complete("data.at(0).argument_snapshot(0).cpp_t"), "cpp_type") &&
                contains(cpp.complete("data.at(0).argument_snapshot(0).ele"), "elements"),
            "completion follows the owned complex-value API");
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
      bool threw = false;
      try {
        cpp.evaluate("auto e = data.at(0);");
      } catch (const std::out_of_range &) {
        threw = true;
      }
      require(threw, "each empty-collection initializer throws at runtime");
    }
    require(!contains(cpp.complete("e"), "e") && !cpp.evaluate("tracer.print(e)"),
            "a failed initializer leaves no inspectable Event");
    Collection populated;
    auto recovered_event = std::make_shared<EventData>();
    recovered_event->message.set_function_name("recovered_event");
    populated.events.emplace_back(recovered_event);
    const auto populated_address =
        std::to_string(reinterpret_cast<std::uintptr_t>(&populated));
    require(cpp.evaluate("auto e = reinterpret_cast<const tracer_repl::Collection*>(" +
                         populated_address + "ULL)->at(0);"),
            "a valid event can reuse the failed declaration's name");
    require(cpp.evaluate("tracer.print(e)") &&
                output.str().find("recovered_event") != std::string::npos,
            "printing the redeclared event keeps the REPL usable");
    {
      int destroyed = 0;
      CppEngine cleanup(facade, {});
      require(cleanup.evaluate(
                  "template<class T> int fail_after_static() { struct Live { "
                  "~Live() { ++*reinterpret_cast<int*>(" +
                  std::to_string(reinterpret_cast<std::uintptr_t>(&destroyed)) +
                  "ULL); } }; static Live object; throw std::out_of_range(\"test\"); }"),
              "declare a throwing initializer with a static local");
      try {
        cleanup.evaluate("auto failed = fail_after_static<int>();");
        throw std::logic_error("initializer did not throw");
      } catch (const std::out_of_range &) {
      }
      cleanup.reset();
      require(destroyed == 1, "reset runs registered JIT cleanup safely");
    }
    require(
        cpp.evaluate(
            "int effects = 0; struct Thing { int count; bool ready() const { "
            "return true; } }; Thing make_thing() { ++effects; return {}; }"),
        "declare custom C++ types");
    members = cpp.complete("make_thing().");
    require(contains(members, "count") && contains(members, "ready"),
            "user type members are completed");
    require(cpp.evaluate("if (effects != 0) throw "
                         "std::runtime_error(\"completion executed code\");"),
            "completion does not execute expressions");
    require(cpp.evaluate("void side_effect() { ++effects; }"),
            "declare a void function");
    require(cpp.evaluate("side_effect()"), "bare void expression executes");
    require(cpp.evaluate("if (effects != 1) throw std::runtime_error(\"void "
                         "expression did not execute once\");"),
            "void expression executes exactly once");
    require(cpp.evaluate("side_effect() // a comment ending in ;"),
            "bare void call followed by a comment is accepted");
    require(cpp.evaluate("if (effects != 2) throw std::runtime_error(\"comment "
                         "hid void call\");"),
            "a comment semicolon does not suppress the call");
    require(cpp.evaluate("int exception_effects = 0;"),
            "declare exception counter");
    try {
      cpp.evaluate("void after_throw() { ++exception_effects; } throw "
                   "std::runtime_error(\"execution failed\");");
      throw std::logic_error("runtime exception was lost");
    } catch (const std::runtime_error &error) {
      require(std::string(error.what()) == "execution failed",
              "runtime exception is preserved");
    }
    require(contains(cpp.complete("after_thr"), "after_throw"),
            "declarations committed before a runtime exception remain "
            "discoverable");
    require(cpp.evaluate("after_throw()"),
            "void call after runtime exception executes");
    require(
        cpp.evaluate("if (exception_effects != 1) throw "
                     "std::runtime_error(\"exception lost a declaration\");"),
        "runtime exceptions do not cause later calls to be silently skipped");
    require(cpp.evaluate(
                "int line_effects = 0; template<int L> auto line_run() { if "
                "constexpr (L == 1) { ++line_effects; } else return L; }"),
            "declare a line-dependent return type");
    require(cpp.evaluate("line_run<__LINE__>()"),
            "line-dependent void call executes");
    require(cpp.evaluate("if (line_effects != 1) throw "
                         "std::runtime_error(\"source lines diverged\");"),
            "classification uses the live cell's logical source line");
    require(contains(cpp.complete("std::vec"), "vector"),
            "standard library names are completed");
    (void)cpp.complete("unknown_variable.");
    require(cpp.evaluate("int after_completion = 42;"),
            "completion errors preserve the running interpreter");
    facade.print(Collection{});
    require(output.str().find("retained=0") != std::string::npos,
            "printing an empty collection is visible");
    require(!cpp.evaluate(
                "#define RETAINED_MACRO() after_throw()\nbogus compile_error;"),
            "a rejected cell is checked without touching the live compiler");
    require(cpp.evaluate(
                "#ifdef RETAINED_MACRO\n#error rejected macro leaked\n#endif"),
            "rejected preprocessor changes do not leak");
    require(!cpp.evaluate("for (int e : {1,2}) {"),
            "an unfinished block is rejected without a diagnostic loop");
    require(!cpp.evaluate("for (int e : {1,2}) { int unfinished = 0;"),
            "a semicolon does not bypass unfinished-input checks");
    require(cpp.evaluate("#define OPEN {"),
            "define a macro that opens a block");
    require(!cpp.evaluate("for (int e : {1,2}) OPEN"),
            "macro-expanded unfinished blocks cannot hang the live compiler");
    require(!cpp.evaluate(
                "#if __INCLUDE_LEVEL__ > 0\nfor (int e : {1,2}) {\n#endif"),
            "preflight and live input share the same include depth");
    require(cpp.evaluate(
                "#if __INCLUDE_LEVEL__ == 1\nint depth_kept = 17;\n#endif"),
            "conditional declarations use the live interpreter context");
    require(cpp.evaluate("if (depth_kept != 17) throw "
                         "std::runtime_error(\"wrong include depth\");"),
            "accepted conditional declarations survive subsequent checks");
    require(!cpp.evaluate("data.at(0).argument<bool>(0)"),
            "the user's non-template accessor error is rejected safely");
    require(!cpp.evaluate("data.at(0).argument_as<std::string, bool>(0)"),
            "invalid typed access does not damage earlier declarations");
    require(contains(cpp.complete("data.rec"), "received"),
            "completion works immediately after rejected input");
    require(cpp.evaluate("after_throw()"),
            "bare void expressions work after rejected input without reset");
    require(
        cpp.evaluate(
            "if (exception_effects != 2) throw "
            "std::runtime_error(\"retained macro did not execute once\");"),
        "rejected input preserves variables without replaying side effects");
    cpp.reset();
    require(!contains(cpp.complete("make_th"), "make_thing"),
            "reset removes old completion declarations");
    TemporaryDirectory directory;
    const auto header = directory.path / "forced.hpp";
    const std::string contents =
        "#pragma once\n#include <iostream>\ninline int forced_initializer = [] "
        "{ std::cout << \"forced-once\\n\"; return 9; }();\n";
    {
      std::ofstream(header) << contents;
    }
    CppEngine forced(facade, {"-include", header.string()});
    std::ostringstream initializers;
    auto *saved_output = std::cout.rdbuf(initializers.rdbuf());
    try {
      require(forced.evaluate("if (forced_initializer != 9) throw "
                              "std::runtime_error(\"initializer skipped\");"),
              "forced include initializes in the live interpreter");
      require(forced.evaluate("if (forced_initializer != 9) throw "
                              "std::runtime_error(\"initializer repeated\");"),
              "checks do not execute forced-header initializers");
    } catch (...) {
      std::cout.rdbuf(saved_output);
      throw;
    }
    std::cout.rdbuf(saved_output);
    require(initializers.str() == "forced-once\n",
            "syntax checks never repeat forced-header side effects");
    const auto stamp = std::filesystem::last_write_time(header);
    {
      std::ofstream(header, std::ios::app) << "#define BODY {}\n";
    }
    bool changed = false;
    try {
      (void)forced.evaluate("for (int n : {1,2}) BODY");
    } catch (const std::runtime_error &error) {
      changed =
          std::string(error.what()).find("header changed") != std::string::npos;
    }
    require(changed, "changed headers cannot bypass the parser safety check");
    {
      std::ofstream(header) << contents;
    }
    std::filesystem::last_write_time(header, stamp);
    const auto original = std::filesystem::current_path();
    std::filesystem::current_path(directory.path);
    changed = false;
    try {
      (void)forced.evaluate("for (int n : {1,2}) BODY");
    } catch (const std::runtime_error &error) {
      changed = std::string(error.what()).find("working directory changed") !=
                std::string::npos;
    }
    std::filesystem::current_path(original);
    require(changed,
            "changed directories cannot bypass the parser safety check");
    require(forced.evaluate("if (forced_initializer != 9) throw "
                            "std::runtime_error(\"lost state\");"),
            "restoring compiler inputs preserves variables without a reset");
    std::cout << "C++ discovery checks passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

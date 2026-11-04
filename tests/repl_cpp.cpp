#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "controller.hpp"
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
} // namespace

int main() {
  try {
    std::ostringstream output;
    Controller controller(Settings{}, output);
    Facade facade(controller);
    CppEngine cpp(facade, {});
    const auto help = cpp.describe_api("api");
    require(help.find("result_as") != std::string::npos &&
                help.find("argument_as") != std::string::npos &&
                help.find("events") != std::string::npos &&
                help.find("print") != std::string::npos,
            "API help lists actual public declarations");
    require(help.find("scalar_as") == std::string::npos &&
                help.find("controller_") == std::string::npos,
            "API help hides implementation details");
    require(contains(cpp.complete("tracer."), "batch") &&
                contains(cpp.complete("tracer.pr"), "print"),
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
            "a rejected cell can retain preprocessor state");
    try {
      cpp.evaluate("RETAINED_MACRO()");
      throw std::logic_error("bare call silently accepted after a parse error");
    } catch (const std::runtime_error &error) {
      require(std::string(error.what()).find("%cpp reset") != std::string::npos,
              "ambiguous state gives explicit recovery guidance");
    }
    require(cpp.evaluate("RETAINED_MACRO();"),
            "explicitly terminated calls use the live macro state");
    require(cpp.evaluate(
                "if (exception_effects != 2) throw "
                "std::runtime_error(\"retained macro did not execute once\");"),
            "retained macro state never silently drops side effects");
    cpp.reset();
    require(!contains(cpp.complete("make_th"), "make_thing"),
            "reset removes old completion declarations");
    std::cout << "C++ discovery checks passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

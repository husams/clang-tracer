#include "context_lifecycle.hpp"
#include "context_owner.hpp"
#include "context_test_config.hpp"

#include <fstream>
#include <iostream>
#include <iterator>

#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>

int main(int argc, char **argv) {
  if (argc != 3 && argc != 4)
    return 1;
  std::ifstream input(argv[1]);
  if (!input)
    return 1;
  const std::string source{std::istreambuf_iterator<char>(input), {}};
  std::vector<std::string> arguments{"-std=c++20",
                                     "-I" TRACER_TEST_SOURCE_DIR "/include",
                                     "-resource-dir=" TRACER_TEST_RESOURCE_DIR};
  arguments.insert(arguments.end(), std::begin(kContextSystemArguments),
                   std::end(kContextSystemArguments));
  auto ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n" + source, arguments,
      "context-lifecycle-fixture.cc");
  if (!ast || ast->getDiagnostics().hasErrorOccurred())
    return 1;
  std::ofstream output(argv[2]);
  if (argc == 4 && std::string(argv[3]) == "owner") {
    auto generated = tracer::generate_context_owner(*ast);
    if (!generated.error.empty()) {
      std::cerr << generated.error << '\n';
      return 1;
    }
    output << generated.compilation_copy
           << "\n#include <array>\n#include <string_view>\n"
              "inline auto generated_owner_test_slots() {\n"
              "return std::array<::tracer::context_engine::StateRegistration, "
           << generated.states.size() << ">{{\n";
    for (std::size_t i = 0; i < generated.states.size(); ++i) {
      const auto &state = generated.states[i];
      output << '{' << i + 1 << ", ::tracer::context_engine::Policy::"
             << (state.type.shared ? "shared" : "confined") << ", &"
             << state.construct << ", &" << state.destroy << ", &"
             << state.address << "},\n";
    }
    output << "}}; }\n"
              "inline auto generated_owner_test_state_names() {\n"
              "return std::array<std::string_view, "
           << generated.states.size() << ">{{\n";
    for (const auto &state : generated.states)
      output << '"' << state.type.state_type << "\",\n";
    output << "}}; }\n";
    return output ? 0 : 1;
  }
  auto generated = tracer::generate_context_lifecycle(*ast);
  if (!generated.error.empty()) {
    std::cerr << generated.error << '\n';
    return 1;
  }
  output << "#include \"context_lifecycle_runtime.hpp\"\n"
         << generated.compilation_copy;
  return output ? 0 : 1;
}

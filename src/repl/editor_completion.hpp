#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tracer_repl {
struct EditorCompletions {
  std::size_t bytes = 0;
  std::vector<std::string> items;
};

EditorCompletions complete_command(std::string_view input, std::string_view after);
std::optional<EditorCompletions> complete_include(std::string_view input,
                                                   std::string_view after);
} // namespace tracer_repl

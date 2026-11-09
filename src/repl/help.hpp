#pragma once

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace tracer_repl {
class CppEngine;

const std::vector<std::string>& command_completions();
std::string command_usage(std::string_view name);
void print_help(std::ostream& out, const std::vector<std::string>& arguments,
                CppEngine& cpp, bool color);
} // namespace tracer_repl

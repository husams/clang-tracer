#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace tracer_repl {
class CppEngine;

const std::vector<std::string>& command_completions();
void print_help(std::ostream& out, const std::vector<std::string>& arguments,
                CppEngine& cpp, bool color);
} // namespace tracer_repl

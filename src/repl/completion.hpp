#pragma once

#include "interpreter.hpp"

namespace clang {
class CompilerInstance;
class LangOptions;
} // namespace clang

namespace tracer_repl {
CppCompletions complete_cpp(const std::vector<std::string> &arguments,
                            const std::string &history,
                            const std::string &source);
bool needs_void_terminator(const clang::LangOptions &language,
                           const std::vector<std::string> &arguments,
                           const std::string &history,
                           const std::string &source, bool synchronized);
std::string describe_cpp_api(clang::CompilerInstance &parent,
                             const std::string &topic);
} // namespace tracer_repl

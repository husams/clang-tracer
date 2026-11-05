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
// Runs only in the disposable checker. Errors terminate that helper.
bool check_cpp_source(const std::vector<std::string> &arguments,
                      const std::vector<std::string> &history,
                      const std::string &source);
std::string reflect_cpp_header(const std::vector<std::string> &arguments,
                               const std::vector<std::string> &history,
                               const std::string &request);
bool incomplete_cpp(const std::string &source);
std::string describe_cpp_api(clang::CompilerInstance &parent,
                             const std::string &topic);
} // namespace tracer_repl

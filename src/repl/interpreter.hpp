#pragma once
#include <memory>
#include <string>
#include <vector>

#include "api.hpp"

namespace tracer_repl {
struct CppCompletions {
  std::string prefix;
  std::vector<std::string> items;
};

class CppEngine {
  struct Impl;
  std::unique_ptr<Impl> impl_;
  Facade &facade_;
  std::vector<std::string> arguments_;
  void initialize();

public:
  CppEngine(Facade &facade, std::vector<std::string> arguments);
  ~CppEngine();
  bool evaluate(const std::string &source);
  CppCompletions complete(const std::string &source);
  std::string describe_api(const std::string &topic = "api");
  void reset();
};
} // namespace tracer_repl

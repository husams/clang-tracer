#pragma once

#include <optional>
#include <string>
#include <vector>

#include <replxx.hxx>

namespace tracer_repl {
class CppEngine;

class Input {
  using Action = replxx::Replxx::ACTION;
  using Result = replxx::Replxx::ACTION_RESULT;
  using Key = replxx::Replxx::KEY;
  struct Field {
    std::size_t begin;
    std::size_t end;
    const char* hint = nullptr;
  };

  bool terminal_;
  replxx::Replxx editor_;
  CppEngine* cpp_ = nullptr;
  std::string previous_;
  std::vector<std::size_t> closers_;
  std::vector<Field> fields_;
  std::size_t field_ = 0;
  bool selected_ = false;
  bool continue_commands_ = false;

  void changed(std::string& text, int& cursor);
  void set(std::string text, std::size_t pos);
  void clear_selection(std::string& text, std::size_t pos);
  Result tab(char32_t key, bool backwards);
  Result enter(char32_t key);
  Result pair(char32_t key);
  Result erase(char32_t key, bool backwards);
  replxx::Replxx::completions_t complete(const std::string& text, int& length);
  void colorize(const std::string& text, replxx::Replxx::colors_t& colors) const;

public:
  explicit Input(bool color);
  bool terminal() const { return terminal_; }
  void use_cpp(CppEngine& cpp) { cpp_ = &cpp; }
  std::optional<std::string> read(const std::string& prompt, bool continue_commands = false);
};
} // namespace tracer_repl

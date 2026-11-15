#pragma once

#include <string>
#include <variant>

namespace shop {
class Label {
  std::string text_;
  std::variant<int, std::string> code_;
  char symbol_ = '*';

public:
  Label(std::string text, int code);

  void mark(); // overwrites two characters of text_ in place
  void bump(); // increments the int held by code_
  const std::string &text() const { return text_; }
  int code() const { return std::get<int>(code_); }
};
} // namespace shop

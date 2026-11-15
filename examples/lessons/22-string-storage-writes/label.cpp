#include "label.hpp"

#include <utility>

namespace shop {
Label::Label(std::string text, int code) : text_(std::move(text)), code_(code) {}

// Overwrites the first and the fourth character. The string keeps its
// length and its buffer; only two bytes change.
__attribute__((noinline)) void Label::mark() {
  char *chars = text_.data();
  chars[0] = symbol_;
  chars[3] = symbol_;
  symbol_ = symbol_ == '*' ? '#' : '*'; // alternate so every call changes the text
}

__attribute__((noinline)) void Label::bump() {
  if (int *number = std::get_if<int>(&code_))
    *number += 1;
}
} // namespace shop

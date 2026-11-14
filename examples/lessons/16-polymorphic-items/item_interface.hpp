// The interface every item version implements. Item holds a pointer to it.
#pragma once

#include <string>

struct ItemInterface {
  virtual ~ItemInterface() = default;
  virtual const std::string& description() const = 0;
  virtual std::string encode(const std::string& type) const = 0;
};

#pragma once

#include <string>
#include <utility>

namespace prepared_pointee {

struct HfwDetails {
  int revision;
  bool active;
};

struct HfwBase {
  explicit HfwBase(int value) : base_id(value) {}
  virtual ~HfwBase() = default;

protected:
  const int base_id;
};

struct HfwItem : HfwBase {
  HfwItem(int base, int value, std::string text, HfwDetails state)
      : HfwBase(base), id(value), label(std::move(text)), details(state) {}

  int id;
  std::string label;
  HfwDetails details;
};

struct HfwManager {
  int manager_id;
  std::string name;

  HfwItem *createItem(int id);
};

} // namespace prepared_pointee

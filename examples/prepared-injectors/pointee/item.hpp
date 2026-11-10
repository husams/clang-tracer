#pragma once

#include <string>

namespace prepared_pointee {

struct HfwDetails {
  int revision;
  bool active;
};

struct HfwBase {
  virtual ~HfwBase() = default;

protected:
  int base_id = 0;
};

struct HfwItem : HfwBase {
  int id;
  std::string label;
  HfwDetails details;

  void set_base_id(int value) { base_id = value; }
};

struct HfwManager {
  int manager_id;
  std::string name;

  HfwItem *createItem(int id);
};

} // namespace prepared_pointee

#pragma once

#include <string>

namespace prepared_pointee {

struct HfwDetails {
  int revision;
  bool active;
};

struct HfwItem {
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

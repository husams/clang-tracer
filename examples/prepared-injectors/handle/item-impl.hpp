#pragma once

#include "item.hpp"

#include <string>

namespace HFW {
#ifdef HANDLE_DEEP
class HfwItemDetail {
public:
  int depth = 0;
  HfwItemImpl *back = nullptr;
};
#endif

class HfwItemImpl {
public:
#ifdef HANDLE_VIRTUAL_IMPL
  virtual ~HfwItemImpl() = default;
#endif
  int revision = 0;
  std::string label;
#ifdef HANDLE_DEEP
  HfwItemDetail *_detail = nullptr;
#endif
};
}

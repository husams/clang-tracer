#pragma once

namespace my_api {

struct MyType {
  unsigned count = 0;

  void add(unsigned amount) noexcept;
};

extern MyType my_instance;

} // namespace my_api

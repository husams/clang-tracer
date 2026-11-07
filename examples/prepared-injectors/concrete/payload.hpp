#pragma once

#include <ostream>
#include <sstream>

namespace HFW {

struct Payload {
  struct Nested {
    int marker;
  } nested{19};

  virtual void describe(std::ostream &out) const = 0;
  virtual std::ostringstream &buffer() = 0;
};

} // namespace HFW

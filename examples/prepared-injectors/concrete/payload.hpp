#pragma once

#include <ostream>
#include <sstream>
#include <system_error>

#include <time.h>

namespace prepared_history {

int uses_toolchain_headers(const std::error_code *error, const struct tm *time);

} // namespace prepared_history

namespace HFW {

struct Payload {
  struct Nested {
    int marker;
  } nested{19};

  virtual void describe(std::ostream &out) const = 0;
  virtual std::ostringstream &buffer() = 0;
};

} // namespace HFW

#pragma once

namespace HFW {

struct Payload {
  struct Nested {
    int marker;
  } nested{19};
};

} // namespace HFW

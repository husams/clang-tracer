#pragma once

namespace receiver_header {
struct Request {
  long cents;
};

struct Quote {
  long cents;
};

Quote quote(const Request &request);
} // namespace receiver_header

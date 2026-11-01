#pragma once

#include <string>

namespace tracer {
struct CallbackClassPlan {
  std::string type = "Interceptor";
  bool has_pre = false;
  bool pre_returns_void = true;
  bool has_post = false;
  bool post_returns_void = true;
};

// Generated invocation objects live in the target wrapper, spanning PRE,
// original dispatch and POST, with automatic cleanup on synchronous unwinding.
std::string make_class_callback_bridge(const CallbackClassPlan &);
std::string make_legacy_callback_bridge();
} // namespace tracer

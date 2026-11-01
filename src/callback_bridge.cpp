#include "callback_bridge.hpp"

namespace tracer {
std::string make_class_callback_bridge(const CallbackClassPlan &plan) {
  std::string code = R"CPP(
#include <optional>
#include <type_traits>
#include <cerrno>
extern "C" bool tracer_callback_lifecycle_enter(unsigned long long) noexcept;
extern "C" void tracer_callback_cleanup_enter() noexcept;
extern "C" void tracer_callback_lifecycle_leave() noexcept;
extern "C" void tracer_callback_failure(unsigned long long) noexcept;
struct tracer_invocation_instance final {
)CPP";
  code += "  std::optional<" + plan.type + "> interceptor;\n";
  code += R"CPP(
  bool hooks_enabled = false;
  explicit tracer_invocation_instance(const tracer::TraceContext &raw) noexcept {
    const int saved_errno = errno;
    if (tracer_callback_lifecycle_enter(raw.probe_id)) {
      try { interceptor.emplace(); hooks_enabled = true; }
      catch (...) { tracer_callback_failure(raw.probe_id); }
      tracer_callback_lifecycle_leave();
    }
    errno = saved_errno;
  }
  tracer_invocation_instance(const tracer_invocation_instance &) = delete;
  tracer_invocation_instance &operator=(const tracer_invocation_instance &) = delete;
  tracer_invocation_instance(tracer_invocation_instance &&) = delete;
  tracer_invocation_instance &operator=(tracer_invocation_instance &&) = delete;
  ~tracer_invocation_instance() noexcept {
    if (!interceptor) return;
    const int saved_errno = errno;
    tracer_callback_cleanup_enter();
    interceptor.reset();
    tracer_callback_lifecycle_leave();
    errno = saved_errno;
  }
};
)CPP";
  code += "static_assert(std::is_nothrow_destructible_v<" + plan.type +
          ">, \"Interceptor destruction must be noexcept\");\n";
  code += R"CPP(
extern "C" long long tracer_user_dispatch(const tracer::TraceContext *raw,
                                          void *object, bool *ran) {
  *ran = false;
  auto &instance = *static_cast<tracer_invocation_instance *>(object);
  if (!instance.hooks_enabled) return 0;
  tracer::Call call(*raw);
  try {
)CPP";
  const auto hook = [&](const char *phase, const char *method,
                        bool returns_void) {
    code += std::string("    if (raw->phase == tracer::TracePhase::") + phase +
            ") { *ran = true; ";
    if (!returns_void) {
      code += "return ";
    }
    code += std::string("instance.interceptor->") + method + "(call);";
    if (returns_void) {
      code += " return 0;";
    }
    code += " }\n";
  };
  if (plan.has_pre) {
    hook("Pre", "pre", plan.pre_returns_void);
  }
  if (plan.has_post) {
    hook("Post", "post", plan.post_returns_void);
  }
  code += R"CPP(
  } catch (...) { instance.hooks_enabled = false; throw; }
  return 0;
}
)CPP";
  return code;
}

std::string make_legacy_callback_bridge() {
  return R"CPP(
extern "C" long long tracer_user_dispatch(const tracer::TraceContext *raw,
                                          void *, bool *ran) {
  *ran = true;
  return trace_callback(*raw);
}
)CPP";
}
} // namespace tracer

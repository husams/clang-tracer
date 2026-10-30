class Interceptor {
public:
  void pre(const Call &call) {
    const auto &stack = call.capture_stack(24);
    call.field("stack_frames", stack.count);
  }
  void post(const Call &call) { call.capture_stack(24); }
};

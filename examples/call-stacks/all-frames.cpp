class Interceptor {
  static void capture(const Call &call) {
    // Explicit callback override, independent of the probe's default setting.
    call.capture_stack(24, false);
    call.field("stack_frames", call.stack().count);
  }

public:
  void pre(const Call &call) { capture(call); }
  void post(const Call &call) { capture(call); }
};

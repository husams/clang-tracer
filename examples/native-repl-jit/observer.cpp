// Capture the caller at entry for the live and replayed call summaries.
struct Interceptor {
  void pre(const Call &call) { call.capture_stack(8, false); }
  void post(const Call &) {}
};

// Runs inside the traced process on the application's calling thread.
extern "C" long long trace_callback(const TraceContext &context) {
  return context.arguments[0].signed_value * 100 + 17;
}

// Entry-only observer using the older TraceContext interface.
// It runs on the application's calling thread when add is entered.
extern "C" long long trace_callback(const TraceContext& context) {
  context.log("legacy add entry");
  return context.arguments[0].signed_value + context.arguments[1].signed_value;
}

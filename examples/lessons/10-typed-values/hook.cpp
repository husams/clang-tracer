// The decoding happens in the REPL, so the observer can be empty. The probe
// still records every argument and the result because of --print-arguments
// and --print-result.
struct Interceptor {
  void pre(const Call&) {}
  void post(const Call&) {}
};

class Interceptor {
public:
  void pre(const Call &call) {
    call.field(call.self()->bias);
    call.field(call.self()->calls);
    // --stack supplies the entry snapshot to both hooks.
    call.field("stack_frames", call.stack().count);
  }
  void post(const Call &call) {
    call.field(call.self()->bias);
    call.field(call.self()->calls);
  }
};

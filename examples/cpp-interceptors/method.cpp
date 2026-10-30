// Clang constructs the receiver type from binary DWARF; no target source is
// needed.
class Interceptor {
public:
  void pre(const Call &call) {
    call.argument<int>(0);
    call.argument<int>(1);
    call.field(call.self()->bias);
    call.field(call.self()->calls);
  }

  void post(const Call &call) {
    call.argument<int>(0);
    call.argument<int>(1);
    call.field(call.self()->bias);
    call.field(call.self()->calls);
  }
};

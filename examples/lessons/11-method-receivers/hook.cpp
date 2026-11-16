// call.self() is a pointer to the receiver, typed from the binary's DWARF.
class Interceptor {
public:
  void pre(const Call& call) {
    call.argument<int>(0);
    call.field(call.self()->balance);
    call.field(call.self()->operations);
  }

  void post(const Call& call) {
    call.field(call.self()->balance);
    call.field(call.self()->operations);
  }
};

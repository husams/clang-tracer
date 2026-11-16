template <>
class Interceptor<int(int)> {
public:
  void pre(const Call& call) {
    call.field("value", call.argument<0>());
  }

  void post(const Call& call) {
    call.field("result", call.result());
  }
};

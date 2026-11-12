template <>
class Interceptor<int(int, int)> {
public:
  void pre(const Call& call) {
    call.field("left", call.argument<0>());
    call.field("right", call.argument<1>());
  }

  void post(const Call& call) {
    call.field("new_sum", call.result());
  }
};

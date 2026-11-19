// Variant of hook.cpp that also captures up to eight caller frames in PRE,
// so `%events stats` can report the caller edge.
template <>
class Interceptor<int(int, int)> {
public:
  void pre(const Call& call) {
    call.capture_stack(8, false);
    call.field("left", call.argument<0>());
    call.field("right", call.argument<1>());
  }

  void post(const Call& call) {
    call.field("sum", call.result());
  }
};

template <>
class Interceptor<tracer::ResolvedSignature> {
public:
  void pre(const Call& call) {
    call.field("input", call.argument<0>());
  }

  void post(const Call& call) {
    call.field("output", call.result());
  }
};

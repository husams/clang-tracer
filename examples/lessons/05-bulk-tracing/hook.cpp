// Installed on every function in the batch, so it uses the resolved signature
// of each one and records its first argument and the result.
template <>
class Interceptor<tracer::ResolvedSignature> {
public:
  void pre(const Call& call) {
    call.field("first", call.argument<0>());
  }

  void post(const Call& call) {
    call.field("result", call.result());
  }
};

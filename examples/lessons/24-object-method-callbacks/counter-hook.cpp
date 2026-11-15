template <> class Interceptor<int(int)> {
public:
  long long pre(const Call& call) {
    return call.self()->value + call.argument<0>();
  }
  long long post(const Call& call) {
    return call.result();
  }
};

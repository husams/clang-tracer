// Beginner example: one Interceptor object handles both sides of a call.
// The callback compiler provides Call; no tracer headers are needed here.
class Interceptor {
public:
  long long pre(const Call &call) {
    return call.arg<int>(0) * 100 + 17;
  }

  long long post(const Call &call) {
    return call.result<int>();
  }
};

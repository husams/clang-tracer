// User-owned callback logic. Generated adapters are kept in the package.
template <> class Interceptor<int(int)> {
public:
  long long pre(const Call &call) { return call.arg<int>(0) * 100 + 17; }
  long long post(const Call &call) { return call.result<int>(); }
};

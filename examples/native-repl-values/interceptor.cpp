// The REPL supplies Call and the target signature; run.sh supplies types.hpp.
struct Interceptor {
  void pre(const Call &) {}
  void post(const Call &) {}
};

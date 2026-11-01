struct BenchScope {};
struct BenchState : state<BenchScope> {
  unsigned long long calls = 0;
  int value = 0;
};
struct Interceptor {
  Interceptor(State<BenchState>);
  void pre(const Call& call, BenchState& state) {
    ++state.calls;
    state.value = call.arg<int>(0);
  }
  void post(const Call&, BenchState& state) { ++state.calls; }
};

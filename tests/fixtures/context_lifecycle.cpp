#include <stdexcept>

namespace lifecycle_demo {
struct Scope {};
struct FailureScope {};
inline unsigned constructed = 0, destroyed = 0;
struct Data : shared_state<Scope> {
  int value = 10;
  Data() { ++constructed; }
  ~Data() noexcept { ++destroyed; }
};
struct Other : shared_state<Scope> {
  int value = 100;
};
struct Failing : shared_state<FailureScope> {
  Failing() { throw std::runtime_error("factory failed"); }
};
struct A {
  A(State<Data>);
  int tracer_context_frame_0 = 17; // Force collision-free member selection.
  void pre(const Call &) {}
};
struct B {
  B(State<Data>, State<Other>);
  void pre(const Call &, Data &state) { state.value += 32; }
};
struct Failure {
  Failure(State<Data>, State<Failing>);
  void pre(const Call &) {}
};
} // namespace lifecycle_demo

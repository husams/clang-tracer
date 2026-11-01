#include <string>
#include <vector>

namespace owner_demo {
struct Scope {};
struct LocalScope {};
struct FailedScope {};
inline unsigned made = 0, destroyed = 0;

struct Data : shared_state<Scope> {
  std::string text;
  int value;
  Data() : value(-1) { ++made; }
  explicit Data(const ScopeInit &init)
      : text(80, 'x'), value(init.root_call().arg<int>(0)) {
    ++made;
  }
  Data(const Data &) = delete;
  ~Data() noexcept { ++destroyed; }
};
struct Local : state<LocalScope> {
  std::vector<int> values{1, 2, 3};
};
struct Failed : shared_state<FailedScope> {
  Failed() { throw 17; }
};
struct A {
  A(State<Data>);
  void pre(const Call &, Data &) {}
};
struct B {
  B(State<Data>, State<Local>, State<Failed>);
  void post(const Call &, Local &) {}
};
} // namespace owner_demo

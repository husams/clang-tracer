#include "repl_cdtor.hpp"
namespace cdtor {
Plain::Plain(int x) : v(x) {}
Plain::Plain(const Plain &other) : v(other.v) {}
Plain::~Plain() { v = -1; }
int Plain::get() const { return v; }
template <class T> Box<T>::Box(T x) : v(x) {}
template <class T> Box<T>::~Box() {}
template <class T> T Box<T>::get() const { return v; }
template struct Box<int>;
template struct Box<double>;
VBase::VBase(int x) : b(x) {}
VBase::~VBase() {}
int VBase::id() const { return b; }
Mid::Mid(int x) : VBase(x + 1), m(x) {}
Mid::~Mid() {}
int Mid::id() const { return m; }
Leaf::Leaf(int x) : VBase(x + 2), Mid(x + 1), l(x) {}
Leaf::~Leaf() {}
int Leaf::id() const { return l; }

int run_once(int n) {
  int sum = 0;
  {
    Plain p(n);
    Plain q(p);
    sum += q.get();
  }
  {
    Box<int> i(n);
    Box<double> d(n + 0.5);
    sum += i.get() + static_cast<int>(d.get());
  }
  {
    Leaf stack(n);
    sum += stack.id();
  }
  {
    VBase *heap = new Leaf(n + 1);
    sum += heap->id();
    delete heap;  // the deleting destructor (D0), reached through a virtual thunk
  }
  return sum;
}
}  // namespace cdtor

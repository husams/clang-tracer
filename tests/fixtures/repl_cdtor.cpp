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
template <class T> Holder<T>::Holder(int t) : tag(t) {}
template <class T> Holder<T>::~Holder() {}
// Only the lifetime members: instantiating copy() for a unique_ptr would not compile.
template Holder<std::unique_ptr<int>>::Holder(int);
template Holder<std::unique_ptr<int>>::~Holder();
VBase::VBase(int x) : b(x) {}
VBase::~VBase() {}
int VBase::id() const { return b; }
Mid::Mid(int x) : VBase(x + 1), m(x) {}
Mid::~Mid() {}
int Mid::id() const { return m; }
Leaf::Leaf(int x) : VBase(x + 2), Mid(x + 1), l(x) {}
Leaf::~Leaf() {}
int Leaf::id() const { return l; }
template <class T> TMid<T>::TMid(T x) : t(x) {}
template <class T> TMid<T>::~TMid() {}
template <class T> T TMid<T>::get() const { return t; }
template struct TMid<int>;
TLeaf::TLeaf(int x) : TMid<int>(x + 1), l(x) {}
TCtorLeaf::TCtorLeaf(int x) : TCtor(x + 1) {}

int Poly::tag() const { return p; }

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
    Holder<std::unique_ptr<int>> holder(n);
    sum += holder.tag;
  }
  sum += use_poly(n);
  {
    Leaf stack(n);
    sum += stack.id();
  }
  {
    TMid<int> complete(n);  // the complete-object constructor (C1): no VTT argument
    TLeaf derived(n);       // the base-object constructor (C2) of TMid<int>, with a VTT argument
    TCtorLeaf made(n);      // the base-object constructor of TCtor, a template specialization
    sum += complete.get() + derived.get() + derived.l + made.v;
  }
  {
    VBase *heap = new Leaf(n + 1);
    sum += heap->id();
    delete heap;  // the deleting destructor (D0), reached through a virtual thunk
  }
  return sum;
}
}  // namespace cdtor

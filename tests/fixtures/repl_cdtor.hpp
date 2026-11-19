#pragma once
#include <memory>
// Constructors and destructors in every shape the Itanium ABI gives them:
//  - Plain: C1/C2 and D1/D2 are one body (aliases), copy constructor;
//  - Box<T>: class template, constructor and destructor of an explicit instantiation;
//  - VBase/Mid/Leaf: virtual inheritance (base-object entry points take a hidden VTT
//    argument), virtual destructors (deleting D0 through a base pointer).
namespace cdtor {
struct Plain {
  int v;
  explicit Plain(int x);
  Plain(const Plain &other);
  ~Plain();
  int get() const;
};
template <class T> struct Box {
  T v;
  explicit Box(T x);
  ~Box();
  T get() const;
};
extern template struct Box<int>;
extern template struct Box<double>;
// A class template whose unused member needs a copyable T: only a move-only T makes the
// body of copy() ill-formed, and nothing may instantiate it just to observe the lifetime members.
template <class T> struct Holder {
  T value{};
  int tag;
  explicit Holder(int t);
  ~Holder();
  T copy() const { return value; }
};
struct VBase {
  int b;
  explicit VBase(int x);
  virtual ~VBase();
  virtual int id() const;
};
struct Mid : virtual VBase {
  int m;
  explicit Mid(int x);
  ~Mid() override;
  int id() const override;
};
struct Leaf : Mid {
  int l;
  explicit Leaf(int x);
  ~Leaf() override;
  int id() const override;
};
// A polymorphic class with an implicit destructor whose key function is defined in repl_cdtor.cpp.
// repl_cdtor_user.cpp emits its own copy of the destructor from a translation unit that sees the
// class only as a declaration of the debug information, so one address has several descriptions.
struct Poly {
  int p = 0;
  virtual ~Poly() = default;
  virtual int tag() const;
};
int use_poly(int n);
// One pass through every construction and destruction path; returns a checksum.
int run_once(int n);
}  // namespace cdtor

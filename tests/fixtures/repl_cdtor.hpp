#pragma once
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
// One pass through every construction and destruction path; returns a checksum.
int run_once(int n);
}  // namespace cdtor

// Class definitions for the headerless DWARF receiver layout test (issue #146). A header-assisted
// control includes this file; the rest of the test recovers the same classes from DWARF alone.
#pragma once

namespace layout {

// Plain scalar members: the control that always worked.
struct Plain { int a = 1; int b = 2; int get() const; };

// A base class.
struct Base { int base_value = 11; };
struct Derived : Base { int own = 22; int get() const; };
// A base of a base, and two bases (the second at a nonzero offset).
struct Level2 : Derived { int level = 55; int get() const; };
struct Second { int second_value = 33; };
struct Multi : Base, Second { int multi_own = 44; int get() const; };

// Bit-fields.
struct Bits {
  unsigned a : 3;
  unsigned b : 5;
  int plain = 5;
  Bits() : a(1), b(2) {}
  int get() const;
};
// An unnamed bit-field in the source (DWARF does not list it) and a signed one.
struct BitGap {
  unsigned a : 3;
  unsigned : 4;
  unsigned b : 5;
  int s : 6;
  bool flag : 1;
  long wide;
  BitGap() : a(1), b(2), s(-7), flag(true), wide(77) {}
  int get() const;
};

// A record member, a union member, an array member and anonymous members.
struct Inner { int x = 1; };
struct Holder { Inner in; int y = 2; int get() const; };
union U { int i; float f; };
struct UHolder { U u{}; int z = 4; int get() const; };
struct Arr { int a[4] = {1, 2, 3, 4}; int get() const; };
struct AnonMem {
  int a = 1;
  struct { int x = 2; int y = 3; };
  union { int u = 4; float uf; };
  int get() const;
};
// An array of records, a member of an unnamed type, a two-dimensional array.
struct Deep {
  Inner items[2];
  struct { short lo; short hi; } pair;
  int m[2][3];
  Deep() : pair{7, 8} {
    items[1].x = 6;
    for (int i = 0; i < 2; ++i)
      for (int j = 0; j < 3; ++j)
        m[i][j] = i * 3 + j;
  }
  int get() const;
};
// A member type nested in the receiver itself.
struct Outer {
  struct In { int v = 9; };
  In in;
  int tail = 10;
  int get() const;
};

// Declared alignment, of the class and of a member.
struct alignas(16) Aligned { int a = 1; int get() const; };
struct MemberAligned { char c = 1; alignas(16) int wide = 66; int get() const; };

// Shapes that are not rebuilt from debug data. The receiver is still observable.
struct Poly {
  int a = 1;
  virtual int vget() const;
  int get() const;
  virtual ~Poly() {}
};
struct VB { int vb = 1; };
struct VD : virtual VB { int d = 2; int get() const; };
struct __attribute__((packed)) Packed { char c = 1; int i = 2; int get() const; };
enum Color { Red, Green };
struct WithEnum { Color c = Green; int i = 3; int get() const; };
struct WithPtr { Inner* p = nullptr; int i = 3; int get() const; };

}  // namespace layout

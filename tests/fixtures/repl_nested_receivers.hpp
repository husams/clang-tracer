#pragma once
namespace nested_receivers {
struct Outer {
  struct Nested { int v = 3; int f(int x) const; };
  struct Sibling { int w = 5; int g(int x) const; };
  Nested n;
};
struct Outer2 { struct Mid { struct Deep { int v = 1; int f(int x) const; }; }; };
union UnionScope { int storage; struct Nested { int n = 6; int simple(int x) const; }; };
struct OwnerCase { int v = 3; struct Nested { int n = 3; int owner(const OwnerCase *o) const; }; };
struct SiblingCase {
  struct B;
  struct A { int n = 4; int sibling(const B *b) const; };
  struct B { int s = 2; };
};
struct OpaqueOwner {
  int values[2] = {1, 4};
  struct Nested { int n = 5; int owner(const OpaqueOwner *o) const; };
};
struct DeepSibling {
  struct B { struct Deep { int n = 5; }; };
  struct A { int n = 6; int sibling(const B::Deep *d) const; };
};
struct Plain { int a = 1; int b = 2; int get() const; };
}

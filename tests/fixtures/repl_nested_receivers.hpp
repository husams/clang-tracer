#pragma once
namespace nested_receivers {
struct Outer {
  struct Nested { int v = 3; int f(int x) const; };
  struct Sibling { int w = 5; int g(int x) const; };
  Nested n;
};
struct Outer2 { struct Mid { struct Deep { int v = 1; int f(int x) const; }; }; };
struct Plain { int a = 1; int b = 2; int get() const; };
}

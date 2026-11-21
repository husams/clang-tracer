// Fixture for repl_unnamed_receiver.py: ordinary and special members whose artificial receiver
// parameter the test strips of its DW_AT_name, plus a virtual-base class whose base-object
// constructor takes a hidden VTT argument after the receiver.
#pragma once
namespace unnamed {
#define UNNAMED_NOINLINE __attribute__((noinline))
struct Plain {
  int value;
  UNNAMED_NOINLINE Plain(int x);
  UNNAMED_NOINLINE ~Plain();
  UNNAMED_NOINLINE int ordinary(int x);
  UNNAMED_NOINLINE int constant(int x) const;
  UNNAMED_NOINLINE static int statik(int x);
};
struct Root {
  int root;
  UNNAMED_NOINLINE Root(int x);
  UNNAMED_NOINLINE ~Root();
};
struct Mid : virtual Root {
  int value;
  UNNAMED_NOINLINE Mid(int x);
  UNNAMED_NOINLINE ~Mid();
};
struct Leaf : Mid {
  UNNAMED_NOINLINE Leaf(int x);
  UNNAMED_NOINLINE ~Leaf();
};
int run_once(int pass);
}  // namespace unnamed

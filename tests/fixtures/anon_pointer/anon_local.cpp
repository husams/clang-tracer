#include "anon_pointer.hpp"

namespace anonp {
// A function-local class has a named enclosing function, so it was never affected.
ANON_NOINLINE auto make_local() { struct L { int a = 9; }; static L l; return &l; }
ANON_NOINLINE int take_local(decltype(make_local()) p) { return p->a; }
int run_local(int) { return take_local(make_local()); }
}  // namespace anonp

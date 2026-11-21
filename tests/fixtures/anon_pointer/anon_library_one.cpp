#include "anon_pointer.hpp"

namespace anonl {
namespace { struct Anon { int a; int b; }; }
static Anon g_one{11, 12};
ANON_NOINLINE int take(Anon *p) { return p->a + p->b; }
ANON_NOINLINE Anon *make(int pass) { g_one.a = 11 + pass; return &g_one; }
int run_library_one(int pass) { return take(make(pass)); }
}  // namespace anonl

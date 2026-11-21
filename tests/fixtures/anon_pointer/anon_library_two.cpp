#include "anon_pointer.hpp"

namespace anonl {
namespace { struct Anon { long x; long y; long z; }; }
static Anon g_two{21, 22, 23};
ANON_NOINLINE int take(Anon *p) { return static_cast<int>(p->x + p->y + p->z); }
ANON_NOINLINE Anon *make(int pass) { g_two.x = 21 + pass; return &g_two; }
int run_library_two(int pass) { return take(make(pass)); }
}  // namespace anonl

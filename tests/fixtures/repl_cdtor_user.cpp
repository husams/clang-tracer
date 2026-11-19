#include "repl_cdtor.hpp"
namespace cdtor {
int use_poly(int n) {
  Poly poly;
  poly.p = n;
  return poly.p;
}
}  // namespace cdtor

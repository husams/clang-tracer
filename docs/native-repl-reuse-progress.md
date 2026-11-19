# Native REPL reuse and compiler profiles

Base: `origin/c++17` at `8c36a45`. All new client code remains C++17.

- [x] Persist automatically generated mappings in a dependency-checked cache;
  report the saved path and reuse unchanged headers across sessions.
- [x] `%cpp generate HEADER [--output FILE] [--type TYPE ...]`: analyze the whole
  include by default, using only the syntax-only Clang AST helper. Never execute
  or link application declarations, constructors, or initializers to generate.
- [x] `%shell "COMMAND"` and shell backticks in `%cpp options add`: capture
  stdout, keep stderr separate, handle quoting, bound execution, and retain
  compiler options on failure.
- [x] `%cpp options compdb DATABASE --file FILE`: exact file entry first;
  otherwise use a C++ source entry in the same directory. Resolve relative paths
  using the database entry directory; never fall back to another directory.
- [x] Tests, command/API help, documentation, and independent review.

Validation: ten tests pass on macOS/Clang 22 and linux-dev/Clang 21 with
the GCC 8.5-compatible SDK. The installed linux-dev client passes the complete
header-reuse/profile contract and all seven native tracing examples; the first
example saves a mapping header and the following six reuse it. Review findings
for newly created header maps and imports after existing C++ cells are covered
by regression tests. The previous client is backed up at
`linux-dev:/tmp/clang-tracer-repl-before-reuse-20261105.tgz`.

Benchmark work is separate in chat `01a1aa99-261d-7ee2-8b4b-a7426934949a`,
using Sol 6.1 High with at most ten minutes of measured runs and no rebuilds.

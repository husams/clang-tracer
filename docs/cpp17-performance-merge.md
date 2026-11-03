# Attach, injection and stream changes for C++17

Target baseline: origin/c++17 at 7add3a8.
Port branch: perf/cpp17-attach-injection.
Source commits: ba71e27, 90a3919 and fdbfeb9.

- [x] Port unlimited console collect/events and Ctrl+C handling.
- [x] Port sparse ELF reads and per-handle CFI validation reuse.
- [x] Preserve legacy glibc main and separate libpthread startup symbol lookup.
- [x] Port cancellation polling and operation-local debug verification reuse.
- [x] Port common-header PCH reuse into signature and validation paths.
- [x] Preserve signature ABI checks, side-effect-free validation and forced-header order.
- [x] Complete independent modern Linux checks.
- [x] Validate the matching GCC 8.5 / glibc 2.23 SDK profile.
- [ ] Merge the validated port into c++17.

## Compatibility adaptations

The branch retains its existing C++20 implementation language and GCC-8-era
library/runtime compatibility. Callback compilation and the public facade remain
C++17; the build standard and SDK runtime settings are preserved.

Admission reads only the exact startup symbols needed for libc, executable main,
or libpthread. The existing compat span/string operations, syscall wrappers and
legacy rendezvous guards remain in place.

Probe validation performs fresh metadata verification when it has no start-token.
Normal injection reuses its own operation's verified result. Signature template
handling, structured diagnostics, Sret handling, parse/materialize and conditional
initializer execution remain in the C++17 pipeline.

Extracted forced includes bypass both PCH variants because their macros must
precede standard headers. A regression warms the common cache, then validates
and injects with the old libstdc++ string ABI selected by a forced header.
The PCH helper test uses the configured runtime JIT flags and C++17 mode, and
links tracer_compat for SDK filesystem/runtime dependencies.

The SDK's LLVM is built without RTTI, so callback_pch.cpp follows the existing
per-source LLVM RTTI setting. GCC 8 caches only the contiguous cstdint/cstddef
prefix: its string headers have an internal template that is unavailable after
PCH loading, and keeping the remaining suffix textual preserves header order.
Other standard libraries retain the full common prefix.

Interpreter creation runs in LifecycleNoTrace. Loading the initial PCH header
IR can start LLVM workers; these must not count as application pthread tasks
belonging to a shared context. The scope covers creation and its uncached
fallback and ends before callback parsing and user initializer execution.

## Validation and outstanding work

Local Python: 192 passed, 45 native skips. The final helper sources also pass both
normal and unavailable-cache checks with macOS arm64 LLVM 22.1.8 and C++17
callback input. The helper-only scratch build uses the default empty macOS JIT
flags and does not configure or install the Linux agent.

The independent Ubuntu/Clang 21 build completes all 183 actions. Both cache
tests, native/fallback compatibility checks, export audit, single dependency
collision, native gRPC integration and CFI-cache integration pass. Python native
checks pass: all eight proposal/validation cases (including forced-header ABI
selection), eleven signature/reference/object-return cases, and seven
injection/restore/metadata/cache/retention/context/stream cases.

Five modern-host test failures reproduce with the exact 7add3a8 baseline: shared
dependency collision and typed-this fail the same overloaded-callable lookup;
the three admission variants fail the same exact libc/pthread unwind boundary.
They are recorded as baseline coverage gaps, not passing tests.

The user subsequently stopped the registered performance study and authorized
isolated SDK validation on linux-dev. The exact Clang 21.1.8, GCC 8.5 library,
glibc 2.23 sysroot and loader profile builds all 183 actions against the existing
static PIC gRPC/protobuf/SQLite prefix. The SDK and installed runtime are unchanged.
After the compatibility fixes above, the final SDK run passes all 38 CTests in
187.80 seconds at parallelism three, including PCH reuse/fallback, all three
legacy admission variants, CFI/module identity, export isolation and both
shared-context drain cases. The earlier 35/38 run exposed the internal string
template and compiler-worker tracking bugs. The shared-context cases passed
with baseline 7add3a8, and an otherwise unchanged no-PCH candidate passed the
class test; the guarded candidate then passed both cases serially and in the
final full suite. The gRPC current-CPU warning also appears in passing controls.
All 29 cases in the full native proposal and signature Python suites also pass
with the final SDK build.
Seven additional native cases pass, covering context history limits,
injection/restoration, metadata rejection, cached typed callbacks, event name
filters and pagination. The unlimited-symbol stress case fails during server
shutdown because its SDK libdl backing file is not mapped in the target. The
exact 7add3a8 baseline reproduces the same failure with the same SDK and test;
this remains a baseline coverage gap, not a passing check.

The unchanged GCC 8.5 application also passes with glibc 2.23 and libstdc++
6.0.25: add returns 7, Calculator::multiply returns 17, and inspect(const
Packet&) returns 7.5 with paired PRE/POST events. All three removals restore
their bytes, detach succeeds and the target exits zero. The original and
scratch application SHA256 remain
331d285b8290e3713ba36c9e0b823953d424ae2b9c8e3bc2a2e546d95f35c749.
The by-value Packet case remains rejected because its DWARF lacks complete ABI
evidence; no unsupported signature was admitted to make the check pass.

The checksum-verified SDK evidence is retained in the primary checkout at
[gcc85/README.md](/Users/husam/workspace/clang-tracer/benchmark-results/2026-11-03-cpp17/gcc85/README.md).
Its source archive matches all three final agent/cache/test source hashes;
logs retain both the candidate and exact-baseline cleanup failures.

The reports in benchmarks/attach and benchmarks/injection preserve the original
C++20 measurements. Their speedups have not been re-established for this port.
Build/test evidence and snapshots are staged under
/tmp/clang-tracer-cpp17-20261103.

The checksum-verified modern validation archive is also retained at
[benchmark-results/2026-11-03-cpp17/results](../benchmark-results/2026-11-03-cpp17/results/).
Its summary records 8 passing and 5 baseline-reproduced failing CTests, plus
26 passing native Python cases. The disposable namespace has been removed;
the evidence predates the user-requested stop of the registered study.

## Stopped benchmark preservation

The controller was interrupted at the user's request and cleaned up its own
processes. All 8,765 complete blocks, 26,297 complete trial markers and the partial
last block are retained. The run is marked stopped_by_user, not scientifically
complete, and its heartbeat is paused. No new statistical conclusion is drawn
from the unplanned stopping point.

The original remote tree remains intact. Its checksum-verified 1,300,074,588-byte
archive is retained in the shared-context-benchmark worktree under
benchmark-results/2026-11-01-ordinary-036-high-precision/stopped-2026-11-03/;
STOP-PRESERVATION.md records the full archive path, SHA256 and inventory.

## Remaining merge checks

Final source commit: 98e149bb07e21935fbb5597075d7c5f6afcafa1b. Independent source
review approves merging with the reproduced baseline cleanup gap documented.
Retain the final SDK evidence, fetch the current c++17 head again and merge the
validated port. The target branch remains at 7add3a8 until that merge.

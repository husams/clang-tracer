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
- [ ] Validate the matching GCC 8.5 / glibc 2.23 SDK profile.
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

## Validation and outstanding work

Local Python: 192 passed, 45 native skips. Both actual PCH helper modes pass with
macOS LLVM 22 and C++17 callback input.

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

The exact SDK is available only on linux-dev, where the registered performance
study remains active. The user explicitly chose to wait for that study before
copying the SDK. No SDK transfer, SDK build or runtime installation is authorized
on that host while it runs. Independent validation uses a disposable worker01
pod; it must not be described as GCC 8.5 validation.

The reports in benchmarks/attach and benchmarks/injection preserve the original
C++20 measurements. Their speedups have not been re-established for this port.
Build/test evidence and snapshots are staged under
/tmp/clang-tracer-cpp17-20261103.

The checksum-verified modern validation archive is also retained at
[benchmark-results/2026-11-03-cpp17/results](../benchmark-results/2026-11-03-cpp17/results/).
Its summary records 8 passing and 5 baseline-reproduced failing CTests, plus
26 passing native Python cases. The disposable namespace has been removed;
the final completion-marker check still found the registered study running.

## Resume after the benchmark

Verify the registered run's completion marker before copying the SDK. Validate
this port with scripts/gcc85-build.sh and the native proposal/signature/cache
regressions using the matching compiler, sysroot, loader and runtime paths.
Fetch the current c++17 head again, integrate any intervening changes and retain
the validation evidence before merging perf/cpp17-attach-injection into c++17.
The c++17 branch remains at 7add3a8 until that validation is complete.

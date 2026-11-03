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
- [ ] Complete independent modern Linux checks.
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

Local Python before the extra native regression: 192 passed, 44 native skips.
Native checks require Linux; standalone helper checks are also being run with
macOS LLVM 22 and C++17 callback input.

The exact SDK is available only on linux-dev, where the registered performance
study remains active. The user explicitly chose to wait for that study before
copying the SDK. No SDK transfer, SDK build or runtime installation is authorized
on that host while it runs. Independent validation uses a disposable worker01
pod; it must not be described as GCC 8.5 validation.

The reports in benchmarks/attach and benchmarks/injection preserve the original
C++20 measurements. Their speedups have not been re-established for this port.
Build/test evidence and snapshots are staged under
/tmp/clang-tracer-cpp17-20261103.

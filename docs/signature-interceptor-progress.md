# Signature Interceptor implementation progress

Base: `c++17` at `866e35a`. Branch: `codex/signature-interceptor-cpp17`.
Original main and c++17 checkouts were clean and are preserved.
The existing branch builds the engine in C++20 mode against older runtimes;
the new facade and callback examples are verified in actual C++17 mode.

## Stories

- STORY-001: Native signature specializations, inferred access, DWARF and header type recovery, const method binding, object returns. Complete.
- STORY-002: Per-request compiler options, server syntax/full compilation validation, Python/CLI surface. Complete.
- STORY-003: Independent review, regressions, compatibility and full available suites. Complete.

## Validation constraints

Native instrumentation requires Linux x86-64. With user authorization, removed
the generated `/home/husam/.cache/clang-toolkit` cache on linux-dev, bringing free
disk from 6.8 GiB to 17 GiB. Native Clang 21/GCC 8.5 SDK support libraries are built
in a separate source mirror under the matching worktree path on linux-dev.
Python native tests that compile additional fixtures must set `CXX` to that
SDK's `bin/clang++` and pass matching sysroot, loader and runtime search paths
through `CXXFLAGS`, as in `cmake/toolchains/gcc85-sdk.cmake`. Mixing the host's
default compiler/runtime with the SDK controller invalidates loader identity
checks; the matching-profile symbol-listing regression passes.

## Verification

- Portable LLVM 22 suite: 10/10 tests pass serially, including four actual C++17
  compiled accessor checks. The same four pass with the native GCC 8.5 SDK.
- Python suite on macOS: 184 passed, 42 native cases skipped.
- Full Python suite on Linux using the matching SDK runtime: 226/226 passed,
  including all 42 native cases and the 28 focused proposal scenarios.
- Native SDK agent/server/client/fixtures reached a successful full build.
- Focused runtime checks pass for syntax/full validation without installation,
  syntax locations and signature mismatch rejection, no global initializer
  execution during validation, C++17 headers/options and live scalar injection.
- The 28 focused Linux scenarios pass, including real-header `std::string`
  method calls, mismatched libstdc++ dual-ABI rejection, exact original signature
  matching, and read-only pointer/reference observations for arguments/results.
- Exact const method binding, matching separate debug files, borrowed reference
  results, precise unsupported-ABI rejection, and small/mixed/large record returns
  pass. Forced-no-NRVO nontrivial returns preserve zero copies, zero moves, and
  one caller-owned destruction per result.
- The full native CTest suite uses shortened generated test output paths under
  `/tmp/ct-s17-*`, because the required worktree path exceeds UNIX socket limits.
  Only build-generated `--output-dir` arguments are adjusted; source tests and
  assertions are unchanged. Diagnostic builds use a separate linked agent so
  active tests retain their original binaries.
- All 35 native CTest cases are verified: the final aggregate run passed 33/35,
  followed by a passing serial rerun of both affected cases (2/2, 126.92 seconds).
  One negative assertion needed the exact new compiler-option rejection wording;
  one live installation safely rejected a busy entry under parallel load and
  passed serially with the same binaries. No runtime behavior was weakened.
- Independent review approved all three stories and resolved all 11 findings.
  Condensed native logs are retained
  in the ignored `build-native-evidence/` directory; portable results are in
  `build-portable-tests.log`. The implementation lives on the isolated feature branch.
- Partial cidx index covers the callback facade and accessor tests; native code
  coverage/freshness is not established and is not treated as verification.

## User review incorporated

Keep `Call` non-templated, generate per-probe original/observation type traits,
and check `Interceptor<Sig>` with a generated `static_assert`. Never reconstruct
`std::` types from DWARF; require real headers and verify relevant libstdc++
`cxx11` linkage evidence against `_GLIBCXX_USE_CXX11_ABI`.

## Acceptance

See `signature-interceptor-proposal.md` for the complete requested behavior.
Do not claim full compilation validation from syntax-only checks, execute
callbacks/static initialization during dry runs, or install probes during validation.

## Admission boundaries

Native forwarding targets Linux x86-64 SysV. Unsupported calling conventions,
variadic targets, inheritance/virtual receiver adjustments, and multilevel
record-pointer signatures are rejected. Exact accessible nonvirtual methods
need a resolvable implementation. Opaque class references and pointers permit
forwarding; fields and methods require sufficient matching type information.
Standard-library definitions must come from real headers with verified relevant
libstdc++ dual-ABI evidence. These checks do not establish arbitrary compiler or
standard-library ABI compatibility.
### Independent review: pre-existing engine test contention

The portable `shared_context_engine` test intermittently terminated because
`closure_and_admission_race_cannot_resurrect_records` called throwing `end_scope`
while the worker's capture held the admission gate. The engine intentionally
reports `active_access` from its nonblocking try-lock; the exception then unwound
past a joinable test thread. A temporary diagnostic test copy reproduced that
exact error. The test now retries only `active_access` through `try_end_scope`,
with a five-second deadline, and releases/joins the worker before reporting any
unexpected result. The actual capture/closure race remains. After rebuilding,
20 consecutive focused CTest runs passed on macOS; no engine behavior changed.

### Independent review: supported signature forms

All 11 actionable review findings are resolved by inspected fixes and focused
native evidence. The 28 focused native scenarios pass, including original
signature assertions, read-only pointer/reference arguments and results,
by-value const parameters, real standard-library headers and dual-ABI rejection,
method binding, object return ABI, and validation without initializer execution.
Some additional cv-qualified pointer-reference and cv-qualified return spellings
can still be rejected by the early signature matcher before the generated
`ResolvedSignature` assertion. These optional forms fail closed; no ABI bypass
or mutable inferred view results. Aggregate build and test verification is complete.

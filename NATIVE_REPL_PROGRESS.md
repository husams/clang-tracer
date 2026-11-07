# Native REPL delivery

Base: `origin/c++17` at `ef2d2dd` (started at `953d505`, fast-forwarded over five
non-overlapping upstream commits). Worktree: `codex/native-repl-cpp17`.
The existing backend still requests C++20; all new REPL targets use C++17
and build independently with `TRACER_REPL_ONLY=ON`.

| Story | Status | Acceptance |
| --- | --- | --- |
| S1 Editor and generated grammar | Complete | replxx 0.0.4 / ANTLR 4.13.2, C++17 build, malformed-input checks pass |
| S2 Server and tracing controller | Complete | Real Linux owned startup, PID/name attach, resolve, upload, restoration and detach pass |
| S3 Events and C++ | Complete | Continuous cancellation, resident allocation limits, replay, detail inspection, shared-state decoding, C++ facade and full-buffer recording pass |
| S4 Delivery checks | Complete | macOS LLVM 22.1.8 and Linux LLVM 21.1.8: 3/3 CTests each; real native integration and staged installations pass |

## Validation on 4 November 2026

- All ten REPL translation units compile with `-std=c++17`; private RPC bindings
  and dependencies do not inherit the backend's C++20 targets.
- macOS: `ctest --test-dir build-native-repl --output-on-failure`, 3/3 pass.
- Linux: `ctest --test-dir build-repl --output-on-failure`, 3/3 pass.
- Real Linux integration uses the installed `/usr/local/bin/clang-tracer-server`
  and the existing `cpp-interceptors-target` fixture. It starts an owned server,
  attaches by PID and executable name, uploads a client-held interceptor,
  captures and inspects arguments/results/receiver fields/stacks, pairs calls,
  accesses collections from C++, records/replays, restores the probe, detaches,
  stops the server and verifies that the application continues running.
- Real-run output is retained locally in
  `build-native-repl/linux-native-integration.log`.
- Runtime-component installation was staged on both hosts. Each staged console
  evaluates a `std::vector<int>` expression to 42 with its installed facade
  header and bundled matching Clang builtin headers.
- Full backend-plus-REPL CMake configuration succeeds; dependency configuration
  preserves the parent's build type. Existing backend sources were not changed
  by this feature, and its full backend test suite was not rerun.
- Grammar regeneration is reproducible with the pinned ANTLR jar; formatting,
  script syntax and handwritten-source whitespace checks pass. Generated files
  preserve ANTLR's output, including its whitespace. Clang static analysis reports no
  diagnostics in the controller, event storage and server lifecycle code.
- A failing executable regression reproduced disk recording being incorrectly
  blocked by full memory collections; the corrected path now passes.

## Review fixes

- Reject stale and disconnected session handles before issuing tracing RPCs.
- Reconcile failed, lost-reply and late attaches with explicit recovery IDs;
  block silent retry and preserve external session ownership. Resolved
  candidates stay resolved even when the server retains detached history.
- Gracefully stop an owned Linux server when its console dies unexpectedly;
  the native integration check redeploys afterward to verify hook restoration.
- Release a live collection before same-name replacement; require an unused
  name for offline replay to preserve captures if a recording is corrupt.
- Handle unknown event phases safely and sum cumulative loss counters by their
  probe/server scope without double counting repeated samples.
- Add focused regressions for these failure and recovery paths.

Dynamic symbol/C++ semantic completion and remote process enumeration remain
follow-up capabilities. The implementation provides command completion and
local process selection; explicit remote PID attach works through the service.

Delivery uses the isolated `codex/native-repl-cpp17` branch and a pull request
targeting `c++17`; merging and live installation are separate operations.

## Typed scalar accessors follow-up — 4 November 2026

- Complete in isolated `codex/native-repl-typed-values` worktrees, based on the
  merged `c++17` commit `76c91ab`.
- Added `argument_as<T>(index)`, `result_as<T>()`, presence checking and owning
  scalar variants; existing text accessors remain compatible.
- Type/payload checks and integer/floating range checks cover native and
  shared-context events. Bool, numeric, missing-value and malformed-value
  regression tests pass; the initial tests reproduced the missing API.
- macOS LLVM 22.1.8 and Linux LLVM 21.1.8: 4/4 CTests pass on each host. The
  decoder uses oneof case accessors compatible with Linux's Protobuf 3.14.
- Reinstalled the runtime and facade header under `/usr/local` on `linux-dev`;
  their hashes match the new build and source header.
- The installed console passes live integer arithmetic, bool arguments/results,
  typed bool recording replay, attach/upload/collection and restoration checks.
  Linux evidence is in the remote typed-values worktree's
  `build-repl/native-integration.log`; the prior binary/header are retained there
  for rollback. Delivery target: `origin/c++17`.

## Printing and API discovery follow-up — 4 November 2026

- Reproduced the exact `tracer.print(data.at(0))` report without a semicolon on
  both LLVM versions. Clang's void value-capture path discarded the call.
  A disposable syntax-only compiler now identifies bare void expressions;
  the live interpreter terminates and executes them once. Counter, print and
  detach regressions cover the fix.
- Added batch printing (including empty summaries), explicit event flushing,
  `%help api` / `%help Event` / `%help Collection` / `%help tracer`, and Tab
  completion for local C++ declarations, public fields and methods.
- API help reads the actual public AST declarations. Completion uses Clang
  semantic analysis of successful session inputs and never executes that code;
  `%complete "data.at(0)."` provides the same discovery in scripted sessions.
- Six CTests pass on macOS LLVM 22.1.8 and Linux LLVM 21.1.8. Terminal tests
  exercise the exact bare print spelling, real Tab insertion and help display.
  Format/grammar regeneration checks and Clang static analysis pass.
- Reinstalled the runtime/header on `linux-dev`; build and installed hashes
  match. The installed terminal check and real attach/upload/collection,
  bool/numeric analysis, recording/replay and restoration checks pass.
  Evidence: remote `build-repl/discovery-ctest.log` and
  `build-repl/discovery-integration.log`. The preceding installation is retained
  as `previous-discovery-repl` / `previous-discovery-api.hpp` in that build.
- Source is maintained in isolated typed-values worktrees for delivery to `c++17`. Dynamic
  completion of remote tracing symbols and typed target-object reconstruction
  remain follow-up work; scalar typed access and structured inspection work.

### Publication review

- Independent review approved after regressions for comments ending in `;`,
  declarations retained across runtime exceptions, source-line-dependent
  return types, and macros retained after parse errors.
- Raw Clang tokenization distinguishes actual terminators from comments.
  Discovery records accepted declarations before execution, mirrors each cell's
  logical line 1, and rejects ambiguous state after parse errors with explicit
  recovery guidance. Pending values are transferred through the public
  interpreter API without repeating the user cell.

## Local C++ JIT P0

Base: `04e503b` on `c++17`. Isolated branch: `codex/local-cpp-jit-p0`.
Scope: local Linux x86-64 C++ targets.

| Story | Implementation |
| --- | --- |
| J1 Runtime identity | Compare the loaded agent's GNU build ID and bootstrap version before accepting attach. Report the runtime profile and measure resolution, target JIT compilation and patch installation separately. Recheck PID start time on both sides of ptrace admission. |
| J2 Named observer validation | Validate the exact in-memory C++ draft before upload, through the existing Clang interpreter and non-activating validation path. The native fixture distinguishes validation from activation and checks target-side STL, templates and RAII. |
| J3 Observer lifetime | Use the existing probe ID as the observer generation. Keep the established replacement and drain paths, which retain native wrappers for late arrivals. Document the callback-scoped borrow contract. |

The named-observer fixture ran with Clang/LLVM 21.1.8, glibc 2.23 and the GCC
8.5 SDK. One install measured 1.60 ms to resolve, 1,685.74 ms in the target
JIT and 5.13 ms to patch. The callback reported the target PID and its RAII
destructor count. The REPL inspected and replayed the owned events.

The stale-agent check replaced the configured ELF on disk while the original
remained mapped. Attach rejected the build ID mismatch and released ptrace.
Native Python checks covered invalid syntax, signature mismatch, unresolved
symbols, initializer-free validation and failed replacement preserving the
old observer. All 198 portable Python tests passed.

Nine focused CTests and five native Python tests passed on Linux x86-64.
The staged-install interceptor test passed with its target fixtures rebuilt
with DWARF in the isolated validation directory. It covered original exception
propagation, replacement and concurrent detach. The ordinary GCC 8.5 SDK
profile was used throughout.

## Local C++ browsing P1

T1 adds identity-checked local process selection, thread listings, executable
module mappings and bidirectional C++ symbol lookup. Process selections carry
the PID start time and executable path into attach. Module snapshots carry the
session ID, live executable ranges and backing-file identity. Reverse lookup
requires a current token from that session. Loader notifications and import,
export and section views remain follow-up slices.

The browser fixture selects one of two same-name C++ targets, rejects an
incorrect start time for the other, lists both threads of the selected target,
round-trips a C++ symbol through an interior runtime address, and rejects the
old token both after unload and after loading a different ELF identity at the
same path.

The GCC 8.5 Linux x86-64 build passed four focused CTests and the native
Python symbol-pagination regression. All 198 portable Python tests passed.

### T2 Bulk trace dry run

`%trace preview` expands qualified-name and module globs, applies exclusions,
and resolves exact overloads in one pass per mapped image. It reports verified
runtime entries, static ABI status, and the target agent's retained probe
capacity when readable. The versioned manifest stores selection rules for
another run against the current mappings.

The GCC 8.5 Linux x86-64 fixture passed split DWARF, ASLR address translation,
overload exclusion, exact signatures, concrete templates, virtual
implementations, duplicate modules, and manifest round-trip. The fixture
compared the same target entry bytes before and after preview and confirmed
that reattaching after removal still reports the retired agent slot.

Seven focused Linux CTests, the REPL help smoke check and all 198 portable
Python tests passed.

### T3 Bulk trace apply and removal

`%trace apply --manifest FILE` resolves the saved selection in the live
session and installs typed Clang JIT observers. An optional callback file
is compiled for each selected function. Preflight checks selection blockers,
existing hooks and available retained slots. Bulk installs cannot replace
active probes. The server serializes installs and removals within the session.

Each retained probe records its batch ID. After an installation failure, the
server restores attempted probes from that batch in reverse order and reports
the result per entry. `%trace remove BATCH_ID` removes only probes owned by
that batch and can retry incomplete restoration. Bulk apply currently
accepts DWARF-supported bindings. Prepared-only entries block the batch.

The Linux x86-64 regression forced a type mismatch on the second overload
after the first was installed. It verified original bytes after rollback,
successful typed observation on both overloads in a later batch, repeatable
removal, and an unrelated probe emitting events through both operations.

Eight focused CTests passed on Linux x86-64 with the GCC 8.5 SDK and
Clang 21.1.8. All 198 portable Python tests passed.

### T4 Live and replay trace summaries

`%events stats NAME` reports observed call counts per probe, fixed duration
histograms, and direct caller edges from entry stacks. PRE and POST are
paired by server lifetime, session, process, probe, thread and call ID.
Missing phases and ambiguous or invalid timestamps are reported separately
from timed calls, cumulative loss counters and local evictions.

Eight focused CTests passed on Linux x86-64 with the GCC 8.5 SDK and Clang
21.1.8. The native fixture verified the same event window produced matching
live and offline summaries, including the symbolized caller. Unit coverage
checks incomplete pairs, replayed copies, duplicate phases, invalid timing,
histogram boundaries and caller evidence from filtered or partial stacks.

A bulk observer captured 32 timed calls across both fixture overloads and
reported `main` as the caller for both.

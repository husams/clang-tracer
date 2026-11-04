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

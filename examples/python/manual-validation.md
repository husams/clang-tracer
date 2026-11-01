# Manual Python API validation — 2026-11-01

The installed Python client and native server on `linux-dev` were exercised
directly in an interactive Python prompt over an SSH terminal. These checks
used the installed package from the persistent `uv` tool environment, rather
than the editable checkout or a mocked gRPC service. The native example target
was `cpp-interceptors-target`, built with DWARF information.

## Observed API behavior

| Operation | Observed result |
| --- | --- |
| `status`, `start_server` | Initially unavailable; then running and managed with a server PID. |
| `attach`, `list_sessions` | Returned an ATTACHED session for the real target PID with no initial probes. |
| `resolve_probe` | `add(int, int)` resolved to `_Z3addii`, with INT32 result/arguments and DWARF argument names `left`, `right`. |
| `add_probe` | Installed the resolved spec with the interceptor source and entry address. |
| `collect_traces` | Received eight alternating PRE/POST events for four calls. POST results were 16, 18, 20 and 12, matching the sums of the captured arguments. |
| Native stacks | Frames included `add(int, int)` and `main`, with source file and line information. |
| `remove_probe` | Confirmed `original_bytes_restored=True`. |
| `inject` | Installed `Calculator::multiply(int, int) const` without manually declaring its ABI or class layout. |
| Receiver values | A POST event had arguments `[3, 5]`, fields `bias=7`, `calls=228`, and result `250`; `3 * 5 + 7 + 228 == 250`. |
| `watch_events` | Session/PID filters worked; the delivered event advanced the resume cursor to 10. |
| `record_traces` | Wrote six binary events; replay decoded sequences 11 through 16 and retained the method probe identity and phases. |
| Recording exclusivity | Recording over the same path raised `FileExistsError` and preserved the existing recording. |
| Invalid PID | `attach(0)` raised `ValueError` before a native operation. |
| Missing session | Resolution returned gRPC `NOT_FOUND` with `session does not exist`. |
| Idle deadline | Collecting from an unused PID filter returned `DEADLINE_EXCEEDED`. |
| Blocked cancellation | Closing a stream from the controlling thread unblocked a pending read with gRPC `CANCELLED`. |
| Terminal Ctrl-C | The Python prompt received `KeyboardInterrupt`; the managed server stayed ready and the target stayed alive. |
| `detach` | Both probes were inactive with original bytes restored; the target continued running. Both probes reported zero callback errors and zero agent drops. |
| `shutdown_server` | The owned server stopped and its status became unavailable. |
| `ServerProcess` | Standalone launch/status/shutdown succeeded. An observing `Client` reported `managed=False` and refused to shut down a server it had not launched. |

The target and both managed servers were cleaned up after the session. Raw
recordings, target output, and the observed-value JSON are retained on Linux
under `/tmp/clang-tracer-python-manual-20261101/`.

## Runnable examples

All four scripts were then executed against the installed Linux API/server:

- `trace.py`: six free-function events and six method events; probes removed and
  sessions detached. The typed-value display was also exercised for a method
  with an external server: arguments `[3, 5]`, fields `bias=7`, `calls=3`, result
  `25`, followed by `[4, 6]`, `calls=4`, result `35`.
- `stream.py`: six events, cursor 6 and cancellation on context exit. Its stack
  display was also exercised for four events and showed `add(int, int)`, `main`.
- `record.py`: six events recorded and decoded back as PRE/POST sequences 1–6.
- `server_lifecycle.py`: server readiness, an external client's ownership state,
  empty session listing and graceful shutdown.

The external-server invocation detached its session and left that server
running; the manually launched server was stopped afterward. The replay reader
also rejected a truncated length header, a truncated body, and a frame larger
than 32 MiB. Ruff formatting/lint and strict mypy passed for the example sources.

These results cover the scalar function/method fixture above; they do not claim
exhaustive coverage of every supported target ABI or shared-context schema.

## Review follow-up

Review identified unsafe removal of an owned server's endpoint directory when
graceful shutdown times out. The examples now retain that directory while the
child is alive and print its PID and endpoint for recovery. Sixteen regressions
verify retention, ordinary cleanup, external endpoints and constructor failures;
the full local Python suite passes 85 tests, with two native-only tests skipped.
The reviewed scripts were rerun on Linux against a standalone target compiled
from the installed C++ source. Independent re-review found no remaining blockers.

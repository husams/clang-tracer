# Python client and console

Implementation and review fixes are isolated in this worktree. The follow-up
request authorizes committing, merging into main, and rebuilding/installing the
native server, Python client and console on linux-dev.

- [x] Python package, generated messages, and all public gRPC operations implemented.
- [x] C++ selector resolution on the server for remote Python clients implemented.
- [x] Cancellable live trace streams and bounded binary collection implemented.
- [x] Shared server launch, readiness, status, and graceful shutdown runtime implemented.
- [x] Lark command grammar and highlighted prompt-toolkit console implemented.
- [x] API, transport, parser, terminal, process, and packaging validation.
- [x] Usage documentation and recorded validation limits.

## Validation state

User authorized an isolated local Python environment after both IPython tool
connections disconnected, and explicitly authorized copying/testing this source
on linux-dev. The source snapshot and Linux build are isolated under
`/tmp/clang-tracer-python-client-20261101/`; no deployed installation was replaced.

- macOS / Python 3.12.10: **68 passed**, two native-only tests skipped.
- Rocky Linux / Python 3.12.14 / Clang 21.1.8: native build succeeded;
  **70 passed**, including real attach, DWARF name resolution, free/method probes,
  stack capture, binary recording, probe removal, detach, managed server restart,
  and shutdown restoration with subsequent reattachment.
- Ruff lint/format and strict mypy passed on macOS; Linux lint/typing passed.
- Wheel and source distribution built successfully, including grammar, generated
  `.py`/`.pyi` bindings, typing marker and license. Wheel installed in a separate
  clean environment with Protobuf 7.36.2; imports, grammar, message pickling and
  `python -m clang_tracer --help` passed (test environments used Protobuf 5.29.6).
- Installed console exercised in an actual terminal; keywords/options/strings/
  numbers emitted distinct ANSI colors, and Ctrl-C returned to the prompt.
- Existing native CTest suite: **18/18 passed** (193.20 seconds). The final
  formatted server was rebuilt and both native Python scenarios passed again.
- Generated bindings reproduced byte-for-byte from the checked-in schemas.
- Review fixed client channel readiness after managed-server restart and kept a
  resumed stream's supplied cursor until its first delivered event.
- Independent review found two managed-server lifecycle bugs: foreground Ctrl-C
  reached the server, and repeated shutdown attempts could resend SIGTERM during
  restoration. The server now starts in a separate session and receives a single
  termination request; a real process-group regression and timeout-retry test
  cover both. No other actionable review findings remain.
- Final reviewed snapshot includes main's performance changes at `0d4f613`:
  **69 macOS Python tests passed** (two Linux-only tests skipped), **71 Linux
  Python tests passed**, and **18/18 native tests passed** (193.02 seconds).
  The native install also includes the Python usage guide linked by its README.

Compatibility: rebuilding the server is necessary for name-based injection
through the additive ResolveProbe RPC. Existing RPCs remain wire-compatible.
Server shutdown manages only children launched by this API/console; no remote
shutdown RPC was added. The process manager preserves a child on graceful
shutdown timeout instead of force-killing a server during target restoration.

## Manual API walkthrough and runnable examples

The installed Linux API was exercised interactively after installation. Actual
function/method results, DWARF names, receiver fields, stack frames, cursor
updates, recording/replay, cancellation/deadlines, error statuses, terminal
Ctrl-C, restoration and server ownership/shutdown were observed. All four new
scripts in `examples/python/` were run with the installed package and server;
external-server usage and malformed recording input were checked too. See
`examples/python/manual-validation.md` for concrete results. The follow-up
request authorizes review, commit/merge into main, and Linux installation.

Example review found and fixed unconditional deletion of a still-running
server's private socket directory after a graceful-shutdown timeout. All four
scripts preserve the directory and report recovery PID/endpoint until child
exit. Sixteen regressions cover timeout, normal, external-server and construction
failure cleanup; 85 Python tests pass locally (two Linux-only tests skipped).
The four reviewed scripts also pass on Linux with a target compiled directly
from installed example source. Independent re-review has no remaining blockers.

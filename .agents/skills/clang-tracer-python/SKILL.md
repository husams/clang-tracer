---
name: clang-tracer-python
description: Use the clang_tracer Python API or clang-tracer-console when the task explicitly calls for Python. Covers Python integrations, injection, event streams, and recordings.
---

# Clang Tracer Python

For native demos, prepared injection, typed C++ inspection, and runnable
`.repl` scripts, use the [native REPL skill](../clang-tracer-native-repl/SKILL.md).
For Python automation, operate the public `clang_tracer` API directly.
`clang-tracer-console` is the interactive Python client.

## Find the environment and target

- Use the requested PID, selector, callback and endpoint. Determine whether the
  server is already running or the task calls for launching an owned child.
- Python requires 3.12+. On `linux-dev`, the installed API interpreter is
  `$HOME/.local/share/uv/tools/clang-tracer-client/bin/python`; the console is
  `$HOME/.local/bin/clang-tracer-console`. This API environment is separate from
  the managed base Python. Elsewhere use the task's environment containing
  `clang_tracer`, or the setup in the client guide below.
- Native server launch and instrumentation require Linux x86-64. For a remote
  tracing host, run the API there through the existing connection or connect to
  its already reachable gRPC endpoint. `start_server()` launches on the Python
  process's host; it is not remote process management.
- Check `Client(endpoint).status()` in a client context before assuming service
  readiness. Defaults are `CLANG_TRACER_ENDPOINT`, then `127.0.0.1:50051`.
  Independently launched native servers default to a private Unix socket: match
  their actual endpoint explicitly.

Read [the client guide](../../../python/README.md) for transport options,
headers and advanced arguments. Read only the relevant
[runnable example](../../../examples/python/README.md) when a complete program
is needed. Consult the wiki manuals first for project facts when available:
`[[pages/manuals/clang-tracer-python]]` and
`[[pages/manuals/clang-tracer-console]]`.

## Attach and install an interceptor

1. `session = client.attach(pid)`; retain the returned `session.id`. Inspect
   `list_sessions()` when finding an existing session instead of guessing IDs.
2. Prefer `client.inject(session.id, selector, callback_source, ...)` for normal
   tracing. This uses server-side DWARF resolution; do not reconstruct target
   classes, invent ABI signatures or guess mangled symbols from a declaration.
3. For explicit spec editing, call `resolve_probe()`, modify that generated
   `messages.ProbeSpec`, then call `add_probe()`. Keep its inferred ABI/debug
   metadata intact unless the user actually requests low-level ABI work.
4. Preserve returned probe IDs for removal and event filtering. `stack_depth`
   is 0–64; application-only frames are the default.

Use `list_modules(session.id)` and `list_symbols(session.id, query=..., page_size=20)`
to find the mapped image and exact selector. Pass `module=...` to resolution
and injection when the selector is ambiguous across images. When the client
and server support `ValidateProbe`, call `validate_probe(session.id, selector, source)`
to compile-check the callback before installing it. Check `report.success`
and its diagnostics.

Callback source is transmitted **by value**. `include_headers`,
`compilation_database` and optional `callback_package_path` are paths on the
**server host**. A local callback path does not establish that a matching server
path exists. Name-based operations require the server's `ResolveProbe` RPC;
report `UNIMPLEMENTED` rather than silently switching to manual ABI mode.

For an already-ready server and task-supplied `endpoint`, `pid`, `selector` and
`callback_path`, a bounded collection looks like:

```python
from pathlib import Path
from clang_tracer import Client, messages

with Client(endpoint) as client:
    session = client.attach(pid)
    try:
        probe = client.inject(
            session.id, selector, Path(callback_path).read_text(), stack_depth=16
        )
        events = client.collect_traces(6, session_ids=[session.id], timeout=10)
        for event in events:
            print(event.sequence, messages.TracePhase.Name(event.phase), event.call_id)
        removed = client.remove_probe(session.id, probe.id)
        print("restored:", removed.original_bytes_restored)
    finally:
        client.detach(session.id)
```

For the installed demo, the free-function pair is `add(int, int)` with
`/usr/local/share/clang-tracer/examples/cpp-interceptors/free_function.cpp`.
The method pair is `Calculator::multiply(int, int) const` with `method.cpp` in
that directory. These are fixture examples, not selectors to apply to an
unrelated target.

## Receive, collect or record

- `watch_events([session_id], after_sequence=cursor, timeout=...)` returns an
  `EventStream`. Use a context manager or `close()` to cancel; another thread
  can close a blocked read. Without a supplied timeout, a live stream can wait
  indefinitely. The timeout is a total stream deadline. Use `names=[...]`
  to filter by native function selector or custom event name.
- `collect_traces(count, ..., timeout=...)` keeps a bounded batch in memory;
  `record_traces(path, count, ..., timeout=...)` writes directly to disk.
  Recording creates its path exclusively and preserves completed records on
  failure. Use a fresh path rather than overwriting a recording.
- `record_sqlite(path, count, session_ids=[session_id], timeout=...)` saves a
  bounded live stream to SQLite and leaves server history available. In the
  console, use `record capture.db 100 --sqlite --timeout 30`. Use
  `drain_traces()` or console `drain` only when the task calls for consuming
  retained history. It commits the database writes before acknowledging events,
  which removes those events for all readers.
- For recording and replay with standalone prepared C++ type bundles, use the
  [native REPL recording workflow](../clang-tracer-native-repl/SKILL.md#record-and-replay-events).
- Scope session/PID filters to the task. Empty `session_ids` subscribes to all
  sessions. Resume with the last delivered `stream.after_sequence`; report
  `OUT_OF_RANGE` as an expired bounded-history cursor, not a reason to reset it
  silently. Keep gRPC error codes/details; do not retry mutating RPCs blindly.
- Events are generated Protobuf messages. Preserve PRE/POST `phase`, `call_id`,
  arguments/results, receiver fields, stack metadata and optional `context`
  schema/payload data. Check drop counters and per-probe sequence gaps before
  claiming a complete trace.
- Protobuf recording framing is uint32 little-endian length plus serialized `TraceEvent`.
  Reuse the bounded reader in [record.py](../../../examples/python/record.py)
  for replay, including truncated-frame and size checks.

## Restore and close

`remove_probe()` restores a particular entry; `detach()` restores the session's
probes. Put detach in `finally` after a successful attach. Closing a client
connected to an independent server does not detach that server's sessions.

`Client.start_server()`/`shutdown_server()` and `ServerProcess` manage only their
owned local child. Shutdown sends one SIGTERM and waits for native restoration.
On timeout, keep the manager for another wait; do not force-kill it or delete
its socket directory while it is alive. Report its PID, endpoint and retained
directory if cleanup cannot finish. Follow the public `delete=False` temporary
directory pattern in the examples when launching an owned server.

The server starts in a separate process session, so terminal Ctrl-C cancels a
console input/stream without stopping it. An external server has no shutdown
RPC. Do not adopt or stop it merely because a client can query its status.

Report the operations actually completed, event counts, native restoration
status and any incomplete cleanup. The fixture's
[manual results](../../../examples/python/manual-validation.md) are historical
evidence; do not substitute them for a requested live verification.

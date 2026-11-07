# Event retention and SQLite storage

## Configure retained history

```sh
clang-tracer-server --listen 127.0.0.1:50051 \
  --event-history-limit 16384 \
  --context-history-limit 512 \
  --context-history-bytes 67108864
```

In the console, enter the following on one line:

```text
server start --event-history-limit 16384 --context-history-limit 512 --context-history-bytes 67108864
```

The server retains a bounded shared history. This change keeps its defaults and
adds startup flags `--event-history-limit`, `--context-history-limit`, and
`--context-history-bytes` (serialized context bytes, not total process RAM).
All limits are positive integers; lowering a context limit must not block tracing.
An event larger than the context-byte budget is dropped. This can include
PRE/POST events from a context-enabled probe. Limits apply to the shared server
history, not separately to each session, and cannot be changed on a running server.

## Save or drain

```sh
# Save without consuming retained history.
clang-tracer events 12345 --sqlite capture.db --count 100 --timeout 30
# Drain the current retained snapshot for every session.
clang-tracer drain --sqlite capture.db
# Continuously save and consume events for one PID.
clang-tracer drain 12345 --sqlite capture.db --listen --timeout 60
```

Console commands:

```text
record capture.db 100 --sqlite --timeout 30
drain capture.db
drain capture.db --listen --count 1000 --timeout 60
```

The console uses the selected session by default; without a selection it drains
all sessions. `--session ID`, `--pid PID`, `--name NAME`, and `--after CURSOR`
narrow the capture. Ctrl-C returns to the prompt. Native `drain` defaults to all
sessions and supports PID and after filters. `--listen` polls every 100 ms when
the snapshot is empty; it is a foreground operation.

`clang-tracer drain --sqlite capture.db` saves a snapshot of retained events and
acknowledges exactly those events after SQLite commits. `--listen` repeats this
until interrupted; `--count` and `--timeout` bound the operation. An optional PID
narrows the drain. The console supports `drain capture.db [--listen] [--count N]`
with the existing session, PID, name and after filters. `record capture.db N
--sqlite` and native `events PID --sqlite capture.db` save without consuming.

Reads never reserve or delete events. Acknowledgements carry a random server
lifetime identity and exact sequence IDs. Snapshot continuation pins that same
identity. Concurrent drains are idempotent in one database, but different
databases may both save a batch read before either acknowledgement. Storage
failures do not acknowledge events. An acknowledgement failure may leave a
committed batch; retrying into the same database deduplicates it. Draining consumes
history for every client; stale watchers receive OUT_OF_RANGE. Existing overflow
and transport loss remain possible; recording does not provide lossless capture.

## SQLite format and failure handling

Both writers use `trace_events` with full Protobuf payloads and queryable identity,
process, thread, phase, function and drop-counter columns. Unsigned 64-bit values
are decimal TEXT, avoiding SQLite signed-integer overflow. The primary key is
`(server_instance_id, sequence)`; server restarts must not collide. Each bounded
batch commits before acknowledging. SQLite stays on the client host and can grow
until storage is full. WAL uses FULL synchronous commits.
The database path must name a persistent file; empty paths and `:memory:` are
rejected. Both writers create a missing database and append to an existing one.

Open the recording from the native REPL with `%events open capture.db --as saved`.
Use `tracer.replay("capture.db", query)` to select events by function or custom
event name, phase, process, or session before reading them. See
[Query a recording offline](native-repl.md#query-a-recording-offline) for examples.

## Verification

Acceptance: validate CLI limits; exercise ordinary/context eviction under small
limits; finite snapshots terminate during production; filtered acknowledgement
preserves unrelated events; persisted Protobuf replays exactly; database failure
never removes events; retries deduplicate; restart IDs prevent stale acknowledgements;
continuous capture stops cleanly; both CLI and console reach the same behavior.

Verified on 2026-11-01 in an isolated Linux x86-64 build with LLVM 21.1.8:
173 Python/native tests passed, plus all three native server/client CLI
compatibility CTest checks. The macOS client suite passed 160 tests with 13
Linux-only skips. Ruff, strict mypy, and Python wheel/source builds passed.
The native CLI also rejected `:memory:` before connecting to a server.

Reproduce with `.venv/bin/python -m pytest python/tests` (set
`CLANG_TRACER_NATIVE_BUILD` to the Linux build directory for native tests),
and CTest `grpc_native_integration`, `grpc_client_integration`, and
`grpc_friendly_client_integration` after building their fixtures.

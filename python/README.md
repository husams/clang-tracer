# Clang Tracer Python client

The Python API covers every public `Tracer` gRPC operation. The interactive
console uses that same API, a Lark LALR command grammar, and prompt-toolkit for
live command highlighting, completion, history and input validation. All network
traffic and recorded traces use binary Protobuf.

Python requires 3.12 or newer. Clients can run on macOS, Linux or Windows and
connect to a reachable tracing server. Launching the native server and tracing
processes require Linux x86_64 with the existing Clang Tracer installation and
attachment permissions. The Python wheel does not bundle the native server or
agent. See the [native build instructions](../README.md).

## Install and open the console

From the repository root:

```sh
python3.12 -m venv .venv
.venv/bin/python -m pip install ./python
.venv/bin/clang-tracer-console --endpoint 127.0.0.1:50051
# Equivalent:
.venv/bin/python -m clang_tracer --endpoint 127.0.0.1:50051
```

`CLANG_TRACER_ENDPOINT` supplies the endpoint when no explicit endpoint is given.
The default is `127.0.0.1:50051`, matching the existing C++ client. A server
launched by Python is always passed this endpoint explicitly. Native servers
launched separately default to a private Unix socket; use the same explicit
endpoint on both sides. `unix:///absolute/path/control.sock` is supported.

The default channel uses the existing service's local insecure transport.
Programmatic clients can supply a preconfigured gRPC channel, including a secure
channel when connecting through a TLS endpoint. The supplied channel remains
owned by its caller.

## Programmatic tracing

The repository's `examples/python/README.md` describes runnable programs for
attachment, probes, live streams, binary recordings, and server lifecycle,
including commands for the native example target.

```python
from pathlib import Path
from clang_tracer import Client

with Client("127.0.0.1:50051") as tracer:
    session = tracer.attach(12345)
    probe = tracer.inject(
        session.id,
        "Calculator::add(int, int)",
        Path("interceptor.cpp").read_text(),
        stack_depth=24,
        print_arguments=True,
        print_result=True,
    )
    try:
        for event in tracer.collect_traces(20, session_ids=[session.id], timeout=10):
            print(event.sequence, event.phase, event.call_id, event.stack)
    finally:
        tracer.remove_probe(session.id, probe.id)
        tracer.detach(session.id)
```

`inject()` resolves the C++ selector on the server using DWARF, then submits the
resulting `ProbeSpec` with the interceptor source. It requires the additive
`ResolveProbe` RPC introduced with this client. Rebuild/reinstall the native
server from this revision before using it; an older server returns gRPC
`UNIMPLEMENTED`. Existing RPCs remain compatible with older servers.

Callback source is transmitted by value. Optional `include_headers`,
`compilation_database` and `callback_package_path` refer to paths **on the server
host**. Package location is optional; pass it when the source belongs to a
retained shared-context package. Omitting it follows the native service's
isolated source-lineage behavior. The client never assumes a local callback
path exists remotely.

`resolve_probe(session_id, selector, ...)` returns the complete inferred spec
without installing it. `add_probe(session_id, spec)` accepts a generated
`clang_tracer.messages.ProbeSpec`, preserving access to every advanced field
and the explicit low-level ABI mode. `list_sessions()`, `remove_probe()` and
`detach()` return generated messages too. The generated stubs and all context
transport messages are available under `clang_tracer.proto`; `ContextTransport`
is the native producer/collector protocol, rather than an additional public
tracing control API.

Unary RPCs default to a 30-second deadline, configured with `Client(timeout=...)`.
Errors retain their original `grpc.RpcError` status and details. The client
performs no implicit control retries because operations can mutate targets.

## Receiving and recording traces

```python
with Client() as tracer:
    with tracer.watch_events([session_id], after_sequence=last_sequence) as stream:
        for event in stream:
            consume(event)
            last_sequence = stream.after_sequence
```

Live streams have no deadline unless `timeout` is supplied. Call `stream.close()`
from another thread to cancel a blocked read, or leave its context manager.
`Client.close()` cancels streams on its owned channel. Empty session filters
subscribe to every session; `pid` optionally filters by process. Every event
retains stack frames, shared-context schema/payload bytes, drop counters and
PRE/POST correlation fields.

The server's history is bounded. A stale cursor produces `OUT_OF_RANGE`; the
client does not silently reset it or reconnect. Watch the event drop counters
and per-probe sequence gaps when assessing completeness. Stream deadlines also
raise their gRPC error, including during collection.

`collect_traces(count, ..., timeout=30)` retains at most `count` messages in a
list. For larger recordings use:

```python
with Client() as tracer:
    written = tracer.record_traces(
        "capture.pb",
        100_000,
        session_ids=[session_id],
        timeout=120,
    )
```

Recordings contain repeated four-byte unsigned little-endian lengths followed
by serialized `TraceEvent` bytes. Recording creates the file exclusively and
streams directly to disk. On interruption or RPC failure it preserves completed
records in the partial file and cancels the subscription. The exception still
propagates; partial data is not reported as a successful complete recording.

## Launch, status and shutdown

Both the API and console can launch and own a local native server:

```python
with Client("unix:///tmp/my-tracer/control.sock") as tracer:
    tracer.start_server(
        "/opt/clang-tracer/bin/clang-tracer-server",
        agent="/opt/clang-tracer/lib/libclang_tracer_agent.so",
        operation_timeout=10,
        timeout=30,
    )
    print(tracer.status())
    # Attach processes, install probes, and receive events here.
    tracer.shutdown_server(timeout=30)
```

`ServerProcess` is also exported for standalone process management. Startup
checks that the endpoint is unused, launches without a shell, and waits for a
successful `ListSessions` RPC. Startup output is captured for failure diagnostics.
`status()` distinguishes service readiness from ownership of a child process.

Shutdown sends SIGTERM to the owned child and waits for the native server to
detach sessions and restore probes. A nonzero exit is an error. If graceful
shutdown times out, ownership remains available for a later retry; the manager
does not force-kill a server while it may still be restoring targets. Closing
the client or exiting the console also shuts down its managed server.
The child has its own process session, so terminal Ctrl-C only interrupts the
console. Shutdown retries wait for the same cleanup without sending another signal.

An independently running or remote server can be queried with `status()`, but
cannot be terminated through this manager. There is no remote shutdown RPC in
the existing service. A console connection does not adopt an existing server.
Sessions on an external server remain attached until explicitly detached.

## Console commands

```text
server start --executable /opt/clang-tracer/bin/clang-tracer-server
server status
attach 12345
sessions
use SESSION_ID
probe resolve "Calculator::add(int, int)"
probe inject "Calculator::add(int, int)" --callback interceptor.cpp --stack 24
events --count 10 --after 0 --timeout 20
collect 10 --timeout 20
record capture.pb 1000 --timeout 60
probe remove PROBE_ID
detach
server stop
exit
```

`attach` selects the returned session. Probe commands operate on that selected
session. Stream commands default to the selected session, or all sessions if
none is selected; repeated `--session ID` overrides the selection. `help` lists
every option. Quote selectors and paths containing spaces; both quote styles
and escaped quotes are supported. `#` begins a comment outside a quoted value.
Ctrl-C clears an input or cancels a live stream, returning to the prompt. EOF,
`exit` and `quit` leave the console. The grammar accepts no Python evaluation or
shell command execution. Color-capable terminals show keywords, options, quoted
strings and numbers in different colors; the standard `NO_COLOR` setting is respected.

## Development

```sh
.venv/bin/python -m pip install -e './python[dev]'
.venv/bin/python python/scripts/generate_proto.py
.venv/bin/python -m pytest -c python/pyproject.toml python/tests
.venv/bin/python -m ruff check python
.venv/bin/python -m mypy --strict python/src/clang_tracer
.venv/bin/python -m build python
# Also run the native Python integration tests after building the server/fixtures:
CLANG_TRACER_NATIVE_BUILD="$PWD/build" \
  .venv/bin/python -m pytest -c python/pyproject.toml python/tests
```

Generated `.py` and `.pyi` bindings are checked in so installation does not need
`protoc` or a native build. Regenerate with the pinned `grpcio-tools` version
after changing either schema. The grammar and typing marker are wheel data.

The Python tests use a real gRPC transport peer to verify control requests,
binary events, cancellation, deadlines and console behavior. They do not prove
native ptrace injection or target restoration unless `CLANG_TRACER_NATIVE_BUILD`
is set. That opt-in adds actual server launch, free-function and method
injection, stack/event capture, recording, removal, detach, and restoration of a
live probe during server shutdown followed by reattachment. Native server tests
require the Linux toolchain and a rebuilt server. Process error paths also use
controlled child-process doubles.

Implementation references: [Python gRPC](https://grpc.io/docs/languages/python/basics/),
[Lark API](https://lark-parser.readthedocs.io/en/stable/classes.html), and
[prompt-toolkit](https://python-prompt-toolkit.readthedocs.io/en/stable/pages/asking_for_input.html).

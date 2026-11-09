# Optional Python client and console

Use this reference for Python automation or the Python console. For native tracing, use the [native command reference](commands.md) and [C++ developer guide](developer.md).

## Install the Python client

The Python client is installed separately. Use Python 3.12 or newer. Install from the repository or replace `./python` with your distribution's wheel.

```bash
python3.12 -m venv .venv
source .venv/bin/activate
python -m pip install ./python
clang-tracer-console --help
```

## Python client API

The package exports `Client`, `EventStream`, `ServerProcess`, `ServerStatus`, `messages` (Tracer Protobuf types), and `context` (context transport Protobuf types). Operations return generated messages unless the table specifies a different type. RPC failures raise `grpc.RpcError`; argument and local storage failures raise normal Python exceptions. The client applies deadlines without automatically retrying mutations.

### Connection and lifetime

| Method or type | Behavior |
| --- | --- |
| `Client(endpoint=None, *, timeout=30, channel=None)` | Open a gRPC client. Endpoint defaults to `CLANG_TRACER_ENDPOINT` or `127.0.0.1:50051`. The default channel is insecure. A supplied gRPC channel keeps its own policy and ownership. `timeout` is the default unary deadline. |
| `status(*, timeout=1) → ServerStatus` | Probe endpoint readiness. Read `endpoint`, `running`, `owned`, `pid`, and diagnostic `detail`. |
| `start_server(executable="clang-tracer-server", **options) → ServerStatus` | Launch a local child and wait for its endpoint and the client channel. A child that starts but misses the client deadline can still be owned by the client. |
| `shutdown_server(*, timeout=30)` | Request graceful stop of the owned child. On timeout it remains tracked so you can wait again. |
| `close()`, `with Client(...) as client` | Shut down the owned server and close the owned channel. For a shared server, detach the session explicitly before closing. |
| `ServerProcess(endpoint="127.0.0.1:50051")` | Own and manage a local server separately from a tracing client. `pid` and `running` describe the child. |
| `ServerProcess.start(executable="clang-tracer-server", **options) → self` | Launch and wait for readiness on Linux x86-64. Refuses an endpoint that already serves a tracer. |
| `ServerProcess.status(*, timeout=1) → ServerStatus` | Probe endpoint readiness and add child ownership information. |
| `ServerProcess.diagnostics() → str` | Read the latest captured child diagnostics. |
| `ServerProcess.shutdown(*, timeout=30)`, `with ServerProcess(...) as server` | Request cleanup and wait for the owned process. A timeout can be retried. |

Both start methods accept `agent`, readiness `timeout=30`, native `operation_timeout=10`, `durable_directory`, `event_history_limit`, `context_history_limit`, `context_history_bytes`, `context_event_bytes`, and `context_transport_memory_bytes`. Omitted paths and budgets use native server defaults. See the [native server command](commands.md#native-server) for units.

Use a private Unix endpoint to run an owned server without reserving a TCP port.

```python
from pathlib import Path
from tempfile import TemporaryDirectory

from clang_tracer import Client, ServerProcess

with TemporaryDirectory() as directory:
    endpoint = f"unix://{Path(directory) / 'tracer.sock'}"
    with ServerProcess(endpoint) as server:
        server.start(event_history_limit=8192)
        with Client(endpoint) as client:
            print(server.pid, server.running, client.status().running)
            print(client.list_sessions())
        print(server.diagnostics())
```

### Attach, discover, and inspect

| Method | Behavior |
| --- | --- |
| `attach(pid) → Session` | Attach to a positive Linux PID. Use the returned `id` for later calls. |
| `list_sessions() → list[Session]` | List server attachments and their probes. |
| `detach(session_id) → Session` | Restore probes and detach a session. |
| `list_modules(session_id) → list[str]` | Return eligible mapped application paths. |
| `list_threads(session_id) → list[TargetThread]` | Return Linux TIDs, names, and start times. |
| `list_symbols(session_id, *, query="", module="", page_size=None, page_token="") → ListSymbolsResponse` | Match case-sensitive selector or linkage-name substrings and optionally narrow modules. A positive page size returns a continuation token. |
| `list_memory_mappings(session_id) → ListMemoryMappingsResponse` | Return mapped ranges, permissions, and a session-bound mapping token. |
| `read_memory(session_id, address, size, *, mapping_token) → bytes` | Read a complete readable range of 1–65,536 bytes. Refresh the token if the mappings change. |
| `scan_memory(session_id, address, size, *, mapping_token, pattern, mask, max_matches=0) → ScanMemoryResponse` | Search a readable range of up to 1 MiB. Pattern and mask have equal length. Set each mask bit that should match and clear each wildcard bit. Zero requests the server's default result limit. |
| `preview_function_entry(session_id, *, mapping_token, selector=None, elf_address=None, module="", abi="") → PreviewFunctionEntryResponse` | Verify and disassemble an entry. Use a selector, or provide ELF address, module and ABI together. Inspect `patch_size` and `patch_diagnostic` before installing a probe. |

```python
maps = client.list_memory_mappings(session.id)
entry = client.preview_function_entry(
    session.id, selector="add(int, int)", mapping_token=maps.mapping_token,
)
entry_bytes = client.read_memory(
    session.id, entry.runtime_address, len(entry.entry_bytes),
    mapping_token=maps.mapping_token,
)
print(entry_bytes.hex(" "), entry.patch_size, entry.patch_diagnostic)
```

### Resolve, validate, install, and restore

| Method | Behavior |
| --- | --- |
| `resolve_probe(session_id, selector, **resolution) → ProbeSpec` | Resolve a selector using server-side DWARF. Return a spec you can inspect or extend. |
| `validate_probe(session_id, selector, callback_source, *, level="compile", **resolution) → ValidationReport` | Check callback source without activating a probe. Use `syntax` for parsing only or `compile` for code generation and symbol checks. Inspect the returned report and diagnostics. |
| `add_probe(session_id, spec) → Probe` | Activate an explicit resolved or ABI-described spec. |
| `inject(session_id, selector, callback_source, **options) → Probe` | Resolve, compile, and install a callback in one call. Supports the resolution options below. |
| `reload_probe(session_id, probe_id, callback_source) → ReloadProbeResponse` | Switch active observer generations. Calls already in progress finish with their original generation. |
| `remove_probe(session_id, probe_id) → Probe` | Restore original target bytes and report probe status. |
| `preview_bulk_trace(session_id, rules) → PreviewBulkTraceResponse` | List matches and report ABI eligibility and capacity before installation. |
| `apply_bulk_trace(session_id, rules, batch_id, *, callback_source=None) → BulkTraceReport` | Install a batch with a unique caller-chosen ID and an optional observer override. Inspect the returned report for outcomes and restoration status. |
| `remove_bulk_trace(session_id, batch_id) → BulkTraceReport` | Restore the probes belonging to the batch or retry restoration. |

Resolution options are ordered `include_headers=()`, `compilation_database=""`, ordered `compile_options=()`, `module=""`, `debug_max_entries=0`, and `debug_max_depth=0`. Include and database paths refer to the server. Zero DWARF budgets are unlimited. Callback source is a string read or composed on the client.

Injection also accepts `stack_depth=0` (positive integer or `"full"`), `include_internal_stack_frames=False`, `print_arguments=False`, `print_result=False`, and `callback_package_path=""` (a server path for shared-context lineage).

```python
from pathlib import Path
from clang_tracer import Client

with Client("127.0.0.1:50051") as client:
    session = client.attach(12345)
    try:
        source = Path("hook.cpp").read_text()
        report = client.validate_probe(session.id, "add(int, int)", source)
        if not report.success:
            raise RuntimeError(str(report))
        probe = client.inject(session.id, "add(int, int)", source, stack_depth=16)
        print("Press Enter in the target terminal now")
        for event in client.collect_traces(2, session_ids=[session.id]):
            print(event.sequence, event.function_name, event.phase)
        client.remove_probe(session.id, probe.id)
    finally:
        client.detach(session.id)
```

To manage several probes together, preview a versioned rule set before applying it. Here the rules match a single selector.

```python
from uuid import uuid4

from clang_tracer import messages

rules = messages.BulkTraceRules(version=1, exact_selectors=["add(int, int)"])
print(client.preview_bulk_trace(session.id, rules))
batch_id = uuid4().hex
try:
    print(client.apply_bulk_trace(session.id, rules, batch_id))
    # Trigger the target and collect events while the batch is active.
finally:
    print(client.remove_bulk_trace(session.id, batch_id))
```

### Read, record, and acknowledge

The common filters are `session_ids=()`, `pid=0`, `names=()`, and `after_sequence=0`. Empty filters select all eligible events. Names are exact selector, linkage or custom context-event names. Matches within a filter are ORed and distinct filters are ANDed. Resume strictly after the last processed sequence from the same server lifetime.

| Method or type | Behavior |
| --- | --- |
| `watch_events(session_ids=(), *, after_sequence=0, pid=0, names=(), timeout=None) → EventStream` | Return a cancellable live iterator. The optional deadline covers the whole stream. |
| `EventStream(call, after_sequence=0, names=())` | Wrapper normally created by `watch_events`. Iteration yields `TraceEvent` and updates `after_sequence`. `close()` or context exit cancels the call, including blocked reads. |
| `collect_traces(count, **filters) → list[TraceEvent]` | Collect up to a positive count in memory, with default `timeout=30`. Cancels the stream at the limit or on error. |
| `record_traces(path, count, **filters) → int` | Stream a positive count to a new framed Protobuf file. Default `timeout=30`. A partial file remains if capture fails. |
| `record_sqlite(path, count, **filters) → int` | Append a bounded live capture to SQLite without consuming history. Default `timeout=30`. Returns the number received. |
| `read_events(*, limit=256, through_sequence=0, server_instance_id="", timeout=None, **filters) → ReadEventsResponse` | Read retained history without removal. Use the returned server ID and upper sequence to pin subsequent reads to the same lifetime and window. `None` uses the unary deadline. |
| `acknowledge_events(server_instance_id, sequences, *, timeout=None) → int` | Consume exact committed event IDs from that server lifetime and return the acknowledged count. Affects other readers. |
| `drain_traces(path, *, listen=False, count=None, timeout=None, **filters) → int` | Write and commit retained events to SQLite before acknowledging. Default drains the initial snapshot. `listen=True` follows new events. Retry storage or acknowledgement failures against the same database. |

```python
with client.watch_events([session.id], timeout=30) as stream:
    event = next(stream)
    print(event.sequence, event.function_name)
    cursor = stream.after_sequence
# The stream is closed. Supply after_sequence=cursor to resume on this server.
```

## Python console startup and server commands

```bash
clang-tracer-console --endpoint 127.0.0.1:50051 --timeout 45 --color auto
python -m clang_tracer --help
```

`--endpoint` specifies a server and defaults to the environment or loopback. Omit it and use `server start` to create an owned server. `--timeout` sets a unary RPC deadline (default 30 seconds). `--color auto|always|never` selects a color policy. Auto respects terminal detection and `NO_COLOR`. `--help` and `-h` show startup options.

| Command and example | Options and behavior |
| --- | --- |
| `server start --timeout 45 --event-history-limit 8192` | Start a private local server. `--executable PATH` and `--agent PATH` select binaries. `--timeout SEC` sets the readiness deadline (default 30) and `--operation-timeout SEC` sets the native deadline (default 10, range 1–300). `--durable-directory PATH` enables durable context recording. Supports the five history and transport budget flags in the native server reference. |
| `server status` | Show endpoint readiness and ownership. |
| `server stop --timeout 45` | Gracefully stop the owned server. The shutdown deadline defaults to 30 seconds. Retry if the child is still shutting down. |
| `attach 12345` | Attach and select the process's session. |
| `sessions` | List sessions and probes. |
| `use s1` | Switch to an existing session. |
| `modules` | List eligible application ELF paths. |
| `symbols --filter "add(" --limit 20` | List DWARF callables. `--filter TEXT` and `--module TEXT` match case-sensitive substrings. `--limit N` sets the page size (zero is unbounded) and `--page TOKEN` resumes it. |
| `detach s1` | Restore and detach this session. Omit the session to detach the selected one. |
| `probe resolve "add(int, int)"` | Resolve using DWARF. Accepts Python resolution options. |
| `probe validate "add(int, int)" --callback hook.cpp --level compile` | Compile and check a local callback for the target. `--level syntax` selects syntax-only validation. Accepts Python resolution options. |
| `probe inject "add(int, int)" --callback hook.cpp --stack full --print-result` | Resolve and install a callback. `--stack N\|full` captures entry stacks. `--internal-frames` includes runtime and JIT frames. `--print-arguments` and `--print-result` include snapshots. `--package SERVER_PATH` selects a shared-context package lineage. Accepts Python resolution options. |
| `probe add probe.textproto` | Install an explicit `ProbeSpec` from a local Protobuf text file. |
| `probe remove 1` | Restore the original target entry for this probe. |
| `events --count 2 --timeout 30` | Stream events until a count, deadline, or Ctrl+C. Accepts console stream options. |
| `collect 2 --name "add(int, int)"` | Stream and display events. Optional positional count limits the capture. Accepts console stream options. |
| `record capture.db 2 --sqlite` | Save a required positive count to a new framed Protobuf file or append to SQLite. Accepts console stream options. Default deadline 30 seconds. |
| `drain capture.db --listen --count 10` | Commit to SQLite then acknowledge shared history. `--listen` includes future events. Omit it to drain the initial snapshot. `--count N` limits writes and acknowledgements. Accepts console stream options. |
| `help probe inject` | Show help for a topic. Omit topic for the index. `help colors` describes color options. |
| `! pwd` | Run the rest of the line in a local `/bin/sh`. Working-directory changes stay in that shell. |
| `exit` or `quit` | Exit and stop any server this console owns. |

Python resolution options are repeatable `--include HEADER` and `--compile-option ARG`, plus `--compdb PATH`, `--module TEXT`, `--debug-max-entries N`, and `--debug-max-depth N`. The headers and database are on the server. Zero DWARF budgets mean unlimited traversal.

Console stream options are repeatable `--session ID` and `--name NAME`, plus `--pid PID`, `--after SEQUENCE`, and `--timeout SECONDS`. The default session filter is the selected session, or all sessions when none is selected. Names match an exact selector, linkage name or emitted context event name. Ctrl+C interrupts the stream and returns to the prompt. An expired cursor reports `OUT_OF_RANGE`.

## Automate tracing with Python

Start the target from the first lab and a server listening on loopback. In the same directory as `hook.cpp`, run the [Python client example](#resolve-validate-install-and-restore) with the real PID. Trigger one target call when prompted. The script prints two events, removes the probe and detaches. Repeat with `watch_events` for a cursor-based stream or use `record_traces` to write a framed recording that the native REPL can replay.

```bash
clang-tracer-server --listen 127.0.0.1:50051
```

The [Python client walkthrough](../../python/README.md) includes launch, readiness and shutdown, console examples, and recording. Installed examples in `share/clang-tracer/examples/python` exercise the Python API end to end.

## Trace from the Python console

Use the running target and server from the Python automation example. Run `clang-tracer-console --endpoint 127.0.0.1:50051`, then enter these commands. Trigger one call while collecting.

```bash
attach 12345
symbols --filter "add("
probe validate "add(int, int)" --callback hook.cpp
probe inject "add(int, int)" --callback hook.cpp --print-result
collect 2 --timeout 30
detach
exit
```

## Use the generated Python gRPC stub

Use the generated stub when you need request fields beyond the Python convenience methods. This example lists verified runtime addresses. The mapping token binds the result to the target's current image mapping.

```python
from clang_tracer import Client, messages

with Client() as client:
    session = client.attach(12345)
    try:
        reply = client.stub.ListSymbols(
            messages.ListSymbolsRequest(
                session_id=session.id, query="add(", include_addresses=True,
            ),
            timeout=10,
        )
        print(reply)
    finally:
        client.detach(session.id)
```

## Test the Python client

From the repository root, install the development extras and point the native integration tests at a built tracer.

```bash
python -m pip install -e './python[dev]'
CLANG_TRACER_NATIVE_BUILD="$PWD/build-rhel" python -m pytest python/tests
```

For Python module indexing and preparation, see the [Python client walkthrough](../../python/README.md#index-a-module-and-prepare-a-probe-without-application-headers).

## C++17 branch Python extensions

| Python method | RPC | Behavior |
| --- | --- | --- |
| `allocate_memory(session_id, size)` | `AllocateMemory` | Return the allocation ID, address, and size. |
| `free_memory(session_id, allocation_id)` | `FreeMemory` | Free the session allocation and report its state. |
| `load_target_library(session_id, path)` | `LoadTargetLibrary` | Load a trusted library on the target host. |
| `apply_memory_patch(request)` | `ApplyMemoryPatch` | Submit an executable patch, import redirect, or writable patch with a mapping token and expected bytes. |
| `restore_memory_patch(session_id, patch_id)` | `RestoreMemoryPatch` | Restore and verify original bytes. |
| `start_page_monitor(request)`, `stop_page_monitor(session_id)` | `StartPageMonitor`, `StopPageMonitor` | Arm dedicated pages, then restore them and return first touches. |
| `start_instruction_trace(session_id, address, size, *, tid, mapping_token, **options)` | `StartInstructionTrace` | Start bounded instruction capture. Options are `max_records=256`, `timeout_seconds=5`, `registers=False`, `callback_library=""`, `memory_address=0`, `memory_size=0`, and `discover_calls=False`. |
| `stop_instruction_trace(session_id)` | `StopInstructionTrace` | Stop and return original PCs in execution order. |

Python `validate_probe` and `inject` also accept `allow_call_original` and `trace_exceptions`. Both default to false. Other opt-ins can be set in the corresponding Protobuf requests on a compatible server.

```python
allocation = client.allocate_memory(session.id, 4096)
try:
    mapping = client.list_memory_mappings(session.id)
    initial = client.read_memory(
        session.id, allocation.address, 1, mapping_token=mapping.mapping_token,
    )
    assert initial == b"\x00"
finally:
    client.free_memory(session.id, allocation.id)
```

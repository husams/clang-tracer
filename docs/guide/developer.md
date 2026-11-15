# Clang Tracer developer guide

Use the Python client or the native REPL facade to control a session. A callback runs inside the application, while an event is serialized and owned by the receiving client. Keep pointers, references, stack views, and context state within the lifetime of the callback. Use an event snapshot when the data needs to survive the call or travel to another process.

| Layer | Responsibilities |
| --- | --- |
| Client | Select a target, resolve a callable, validate source, install and remove probes, stream or persist events. |
| Server | Verify target identity, read ELF and DWARF, validate the ABI, coordinate injection, retain events and report loss. |
| Agent | Run the JIT-compiled callback in the target and export captured data through a bounded transport. |

See the [user guide](user.md) for installation and the first callback, and the [tutorials](tutorials.md) for complete workflows.

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

## Native REPL C++ client API

`tracer` is the installed REPL's `tracer_repl::Facade`. The built-in `session` is the current `SessionHandle`. Read `id`, `pid`, and `connection` from a handle and acquire a fresh one after reconnecting. `%help api` lists topics and `%type` shows overloads in the running interpreter.

| Facade method | Behavior |
| --- | --- |
| `attach(pid)`, `selected_session()`, `detach(session)` | Attach and return a handle, get the selected handle, or restore and detach it. |
| `prepare(selector, header_or_headers, output, compdb={}, translation_unit={})` | Generate a reusable package. Supply a string, vector, or braced list of headers with the declaring header first. The output directory must be new. |
| `prepare(selector, header_or_headers, compdb, translation_unit, module, output)` | Pin a package to an ELF image using the explicit-module overload. |
| `inject(session, selector, callback_file)` | Install a local callback through live resolution and return the probe ID. |
| `inject(session, selector, callback_file, prepared_directory, prepared_mode="require")` | Install through a package. Modes are `off`, `prefer`, and `require`. |
| `collect(session, count, timeout_seconds=30)` | Wait and return a bounded `Collection`. |
| `events(session, count, timeout_seconds=30)` | Wait and return an iterable `EventRange`. Check `error()` and `snapshot()` after a failed stream. |
| `events_async(session, count=0, timeout_seconds=0)` | Return an `EventRange` immediately. Zero leaves that limit open. |
| `record(path, options={})` | Write SQLite with compatible prepared type metadata. Returns `RecordResult`. |
| `replay(path, query={})` | Read framed Protobuf or SQLite offline into a `Collection`. Loads compatible recorded C++ mappings from SQLite. |
| `batch(name)` | Fetch a named retained collection. |
| `print(event)`, `print(collection)` | Render event details or collection summary. |

`RecordOptions` has `session`, `all_sessions`, `names`, `phase`, `probe_id`, `after_sequence`, `count`, and `timeout_seconds`. Omit the session to use the selected attachment. An open-ended count is zero. A zero timeout uses the console default for counted recordings. `RecordResult` reports `received`, `cursor`, `complete`, `cancelled`, and `timed_out`. `EventQuery` has `names`, `phase`, `pid`, `session_id`, and `count`. A zero count reads all matches subject to retention.

### Event data types

| Type and methods | Behavior |
| --- | --- |
| `Collection::size(), at(index), begin(), end()` | Count retained events, index with bounds checking, or iterate. Fields `events`, `received`, `evicted`, `complete`, `cancelled`, and `timed_out` expose capture state. |
| `EventRange::snapshot(), running(), error(), stop(), begin(), end()` | Copy a stable bounded collection, check the producer or error text, stop it while retaining data, or iterate a point-in-time snapshot. A running range reserves the remaining collection storage; once it finishes or is stopped it keeps only the room its retained events use. Release all range copies to release that too. |
| `Event::sequence(), probe_id(), call_id(), phase(), function_name()` | Read the event cursor, emitting probe, call identity, phase, and name. |
| `Event::has_result(), argument_value(index), result_value()` | Check return presence or decode a scalar into `ScalarValue` (bool, signed or unsigned 64-bit integer, or double). Void is present but has no scalar value. |
| `Event::argument_snapshot(index), result_snapshot(), receiver_snapshot(), state_snapshot(index=0)` | Read owned structured data from an argument, return, method receiver, or context state. A receiver snapshot represents entry fields. |
| `Event::argument_as<T>(index), result_as<T>(), state_as<T>(index=0)` | Decode a snapshot into an owning C++ value using `EventValue::as<T>()`. |
| `Event::argument(index), result(), describe()` | Get display strings for an argument, return, or whole event. |
| `EventValue::cpp_type(), matches_record_type(name), data(), is_null()` | Inspect the captured type, test record identity, inspect the underlying variant, or check for a null value. |
| `EventValue::field(name), fields(), elements(), at(index), size()` | Access an object member, the object map, the element array, a checked element, or the size of an object, array, or string. Incompatible shapes and missing members raise exceptions. |
| `EventValue::as<T>()`, `EventRecord<T>`, `EventCodec<T>` | Convert to an owning scalar, supported standard container, or user type. Define `EventRecord<T>::type_name` and `fields()` with `event_member(name, &T::member)`, or specialize `EventCodec<T>::decode(const EventValue&)` for custom construction. |

Pipe an `EventRange` or `Collection` through `tracer_repl::take(n)`, `drop(n)`, `first()` (optional value), `stride(n)`, `enumerate()` (zero-based index and value), `filter(predicate)`, `take_while(predicate)`, `drop_while(predicate)`, or `transform(function)`. `to_vector()` and `reverse()` materialize a vector. Filtering and projection run on the foreground snapshot.

```cpp
// Install the probe with --print-result before collecting.
using namespace tracer_repl;
auto live = tracer.events_async(session, 4, 30);
// Trigger two calls in the target terminal and wait for the range to finish.
auto results = live.snapshot()
    | filter([](const Event& e) { return e.phase() == "POST"; })
    | transform([](const Event& e) { return e.result_as<int>(); })
    | to_vector();
live.stop();
```

## Agent callback API

For a typed callback, define one top-level explicit specialization of `Interceptor<Signature>` with either or both of `pre(const Call&)` and `post(const Call&)`. Write the original function type as `R(Args...)`, for example `int(int, int)`, or use `tracer::ResolvedSignature` to reuse the type recovered for the selected target. The callback compiler supplies the primary template, checks the specialization against the target, and exposes its argument and result types through `Call`. For a supported non-static method, the selector identifies the class and method qualifiers. The signature lists the return and explicit parameter types, and `call.self()` provides the receiver separately.

Hooks can return `void` or a `long long` callback value carried in the event. The original function still receives its arguments and returns its own result. Existing plain `class Interceptor` callbacks remain supported. Raw-symbol callbacks can use the entry-only `extern "C" long long trace_callback(const TraceContext&)` API.

The tracer compiles callback source with the effective target flags and checks the supported calling convention. The callback runs on the calling application thread. Keep work bounded and avoid retaining borrowed objects or invoking the probed entry recursively.

| Method | Behavior |
| --- | --- |
| `Call::context()` | Get the current typed shared-context facade. |
| `arg<T>(index)`, `argument<T>(index)` | Decode a raw argument using a supported scalar or pointer type. Uses a checked zero-based index. |
| `argument<Index>()` | Read the compiler-generated argument type. References and supported record values are borrowed with read-only access. |
| `argument<T>(name, index)` | Decode and emit an explicitly named argument, then return it. |
| `argument_count()`, `argument_name(index)`, `argument_value(index)` | Read argument metadata and the raw `TraceValue`. Names fall back to `arg0`, `arg1`, and so on. |
| `result<T>()`, `result<>()` | Read the original result in POST. Supply a type explicitly or use the inferred type on a typed probe. For supported objects and references the result is borrowed. `void` checks the return tag and produces no value. |
| `result_value()` | Read the original raw result in POST. |
| `object_address()`, `self()` | Read the receiver address or typed const receiver for a supported non-static method. Free and static functions have no receiver. |
| `field(name, value)`, `field(member_expression)` | Emit a named field from a supported value. The one-argument form uses Clang member-name inference for supported member expressions. Use the explicit form for computed values. |
| `probe_id()`, `id()` | Return the probe ID and invocation ID. PRE and POST for the same call share the invocation ID. |
| `print_options()` | Read the requested automatic argument and result rendering preferences. |
| `capture_stack(depth=16)`, `capture_stack(depth, application_only)` | Capture and symbolize the entry stack from either hook. The one-argument form uses the probe's filter. A positive depth is required. |
| `stack()` | Read the invocation's current stack snapshot, including automatic capture when enabled. |
| `log(message, level=tracer::LogLevel::Info)` | Queue a diagnostic and return whether it was accepted. Levels are `Info`, `Warning`, and `Error`. |
| `TraceContext::log(message, level)` | Same queue from a legacy callback. Legacy code reads `probe_id`, `call_id`, `phase`, `argument_count`, `arguments`, `argument_names`, `object_address`, `result`, and `stack` directly. |

`TraceValue` has a `type` tag, signed, unsigned, and real slots, and an optional `snapshot`. Decode the slot matching the tag. For snapshots, check the status and detail before reading the owned event representation. `StackTrace` has status, requested and captured depth, count, hidden frame count, filter, detail, and a borrowed `frames` array. Each frame can carry PC, stack pointer, kind, function, module, module offset, file, line, and application classification.

```cpp
template <>
class Interceptor<int(int, int)> {
public:
  void pre(const Call& call) {
    call.field("left", call.argument<0>());
    const auto& stack = call.capture_stack(12);
    call.field("visible_frames", stack.count);
    call.log("entered add");
  }

  long long post(const Call& call) {
    const int sum = call.result();
    call.field("sum", sum);
    return sum;
  }
};
```

For a reusable specialization and a concrete C++ template target, see [Trace a function template instantiation](tutorials.md#trace-a-function-template-instantiation).

Set `CLANG_TRACER_AGENT_LOG` to an absolute writable log path in the **target's environment before launching it**. The target owns the private log. Records include PID, probe ID, call ID and phase where applicable. Messages are capped at 192 bytes and delivered on a best-effort basis.

With logging enabled, the agent also writes an `operation=hook` record for each PRE and POST hook it runs, after the messages logged by that hook. Its `outcome` is `ok`, or `failed` when the hook threw or could not run. Hook records and callback messages share one queue per probe. When it overflows, the agent writes `operation=log stage=queue outcome=dropped` with the number of lost records.

### Shared context API

Declare payloads with `state<Scope>` or `shared_state<Scope>`. Declare hook dependencies with `State<T>` in the interceptor constructor. Shared state follows the engine's guarded access rules, while ordinary state requires synchronization by the callback author. Use the [shared context walkthrough](../shared-context.md) for package lineage, propagation and durable recording.

| Method | Behavior |
| --- | --- |
| `Context::get<T>()`, `get<Scope, Payload>()` | Access a bound typed payload in the current invocation or identify it by scope and payload type. |
| `Context::with<T>(visitor)`, `with<Scope, Payload>(visitor)` | Invoke a visitor with the selected state under its scoped access rules. |
| `Context::init_scope<Scope>()`, `init_scope<Scope>(payload)` | Start a scope, optionally with a typed initial payload. |
| `Context::end_scope<Scope>()` | End the named scope. Accepted child work can keep leases until completion. |
| `Context::event(name, max_items=100)` | Publish a named descriptor-backed snapshot. The permitted item cap is 0–4096. Events can be dropped when their transport or descriptor budget is exhausted. |
| `Context::thread_id()` | Return the current Linux thread ID. |
| `ScopeInit::root_call()` | Read the root invocation when constructing a payload that accepts `const ScopeInit&`. |

```cpp
struct RequestScope {};
struct RequestState : shared_state<RequestScope> {
  int total = 0;
};

template <>
class Interceptor<int(int, int)> {
public:
  Interceptor(State<RequestState>);

  void post(const Call& call) {
    call.context().with<RequestState>([&](RequestState& state) {
      state.total = call.result();
    });
    call.context().event("request.completed");
  }
};
```

## gRPC control API

Generate bindings from `tracer.proto` and its imported `context_transport.proto`. The Development install component includes both schema files under `share/clang-tracer/proto`. The `clangtracer.v1.Tracer` service exposes the following methods. Requests that act on a process after attach carry its session ID.

| RPC | Request → response | Behavior |
| --- | --- | --- |
| `Attach` | `AttachRequest → Session` | Verify and attach a PID. Optionally verify its observed Linux start time. |
| `ResolveProbe` | `ResolveProbeRequest → ProbeSpec` | Resolve a callable on the server host. |
| `ValidateProbe` | `ValidateProbeRequest → ValidationReport` | Check syntax or compile a candidate without activation. |
| `AddProbe` | `AddProbeRequest → Probe` | Install the supplied probe spec. |
| `ReloadProbe` | `ReloadProbeRequest → ReloadProbeResponse` | Switch the active callback generation. |
| `RemoveProbe` | `RemoveProbeRequest → Probe` | Restore one probe. |
| `Detach` | `DetachRequest → Session` | Restore and detach a session. |
| `ListSessions` | `ListSessionsRequest → ListSessionsResponse` | List attachments and probes. Also used for readiness. |
| `ListModules` | `ListModulesRequest → ListModulesResponse` | List eligible modules and optionally verified mapping details. |
| `ListMemoryMappings` | `ListMemoryMappingsRequest → ListMemoryMappingsResponse` | Get memory ranges and a snapshot token. |
| `ReadMemory` | `ReadMemoryRequest → ReadMemoryResponse` | Copy a verified readable range. |
| `ScanMemory` | `ScanMemoryRequest → ScanMemoryResponse` | Search a verified range using a byte pattern and mask. |
| `PreviewFunctionEntry` | `PreviewFunctionEntryRequest → PreviewFunctionEntryResponse` | Verify live entry bytes and report disassembly and patch eligibility. |
| `ListThreads` | `ListThreadsRequest → ListThreadsResponse` | List attached target threads and identities. |
| `ListSymbols` | `ListSymbolsRequest → ListSymbolsResponse` | Browse callables, paginate, request verified addresses, or reverse-lookup a PC. |
| `PreviewBulkTrace` | `PreviewBulkTraceRequest → PreviewBulkTraceResponse` | Preview rules against the live target and retained capacity. |
| `ApplyBulkTrace` | `ApplyBulkTraceRequest → BulkTraceReport` | Install a batch using an ID and optional observer source. |
| `RemoveBulkTrace` | `RemoveBulkTraceRequest → BulkTraceReport` | Restore a named batch. |
| `WatchEvents` | `WatchEventsRequest → stream TraceEvent` | Stream events after a cursor with session, PID, and exact-name filters. |
| `ReadEvents` | `ReadEventsRequest → ReadEventsResponse` | Read a bounded retained window without consuming it. |
| `AcknowledgeEvents` | `AcknowledgeEventsRequest → AcknowledgeEventsResponse` | Consume exact sequences from the matching server lifetime after committing storage. |

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

The agent-to-collector `ContextTransport.Export` RPC is a bidirectional stream of `ProducerPacket` and `CollectorControl`. The agent uses it to deliver state descriptors and events, maintain producer identity, and process collector control. Client applications consume these events through `Tracer.WatchEvents` or retained history.

## Build and validate changes

Use a fresh build directory for a different compiler or SDK. Configure native integration tests, then run the C++ and Python checks in the corresponding build environment.

```bash
./build-rhel.sh --toolchain-root /opt/llvm-21 --build-dir build-rhel --with-tests
ctest --test-dir build-rhel --output-on-failure
python -m pip install -e './python[dev]'
CLANG_TRACER_NATIVE_BUILD="$PWD/build-rhel" python -m pytest python/tests
```

See [validation](../validation.md) for detour regressions and benchmark reproduction. To regenerate the REPL grammar, follow the [ANTLR walkthrough](../native-repl.md#regenerate-the-grammar).

# Clang Tracer developer guide

Use the native REPL and its C++ facade to control a session, index modules, and analyze events. A callback runs inside the application, while an event is serialized and owned by the receiving client. Keep pointers, references, stack views, and context state within the lifetime of the callback. Use an event snapshot when the data needs to survive the call or travel to another process.

| Layer | Responsibilities |
| --- | --- |
| Client | Select a target, resolve a callable, validate source, install and remove probes, stream or persist events. |
| Server | Verify target identity, read ELF and DWARF, validate the ABI, coordinate injection, retain events and report loss. |
| Agent | Run the JIT-compiled callback in the target and export captured data through a bounded transport. |

See the [C++17 branch reference](cpp17.md#additional-client-and-callback-apis) for additional APIs. See the [user guide](user.md) for installation and the first callback, and the [tutorials](tutorials.md) for complete workflows.

## Native REPL C++ client API

`tracer` is the installed REPL's `tracer_repl::Facade`. The built-in `session` is the current `SessionHandle`. Read `id`, `pid`, and `connection` from a handle and acquire a fresh one after reconnecting. `%help api` lists topics and `%type` shows overloads in the running interpreter.

| Facade method | Behavior |
| --- | --- |
| `module_index(source, output={}, options={})`, `open_module_index(path={})` | Build or open an index and select it. See the [native module API](#native-c-module-index-api). |
| `attach(pid)`, `selected_session()`, `detach(session)` | Attach and return a handle, get the selected handle, or restore and detach it. |
| `prepare(selector, header_or_headers, output, compdb={}, translation_unit={})` | Generate a reusable package. Supply a string, vector, or braced list of headers with the declaring header first. The output directory must be new. |
| `prepare(selector, header_or_headers, compdb, translation_unit, module, output)` | Pin a package to an ELF image using the explicit-module overload. |
| `inject(session, selector, callback_file)` | Install a local callback through live resolution and return the probe ID. |
| `inject(session, selector, callback_file, prepared_directory, prepared_mode="require")` | Install through a package. Modes are `off`, `prefer`, and `require`. |
| `collect(session, count, timeout_seconds=30)` | Wait and return a bounded `Collection`. |
| `events(session, count, timeout_seconds=30)` | Wait and return an iterable `EventRange`. Check `error()` and `snapshot()` after a failed stream. |
| `events_async(session, count=0, timeout_seconds=0)` | Return an `EventRange` immediately and receive in the background. Iterating it waits for events. Zero leaves that limit open. |
| `record(path, options={})` | Write SQLite with compatible prepared type metadata. Returns `RecordResult`. |
| `replay(path, query={})` | Read framed Protobuf or SQLite offline into a `Collection`. Loads compatible recorded C++ mappings from SQLite. |
| `batch(name)` | Fetch a named retained collection. |
| `print(event)`, `print(collection)` | Render event details or collection summary. |

`RecordOptions` has `session`, `all_sessions`, `names`, `phase`, `probe_id`, `after_sequence`, `count`, and `timeout_seconds`. Omit the session to use the selected attachment. An open-ended count is zero. A zero timeout uses the console default for counted recordings. `RecordResult` reports `received`, `cursor`, `complete`, `cancelled`, and `timed_out`. `EventQuery` has `names`, `phase`, `pid`, `session_id`, and `count`. A zero count reads all matches subject to retention.

### Event data types

| Type and methods | Behavior |
| --- | --- |
| `Collection::size(), at(index), begin(), end()` | Count retained events, index with bounds checking, or iterate. Fields `events`, `received`, `evicted`, `complete`, `cancelled`, and `timed_out` expose capture state. |
| `EventRange::snapshot(), running(), error(), stop(), begin(), end()` | Copy the currently retained events without waiting, check the producer or error text, stop it while retaining data, or iterate from the oldest retained event, waiting for later ones until the range finishes or Ctrl+C. A running range reserves the remaining collection storage; once it finishes or is stopped it keeps only the room its retained events use. Release all range copies to release that too. |
| `Event::sequence(), probe_id(), call_id(), phase(), function_name()` | Read the event cursor, emitting probe, call identity, phase, and name. |
| `Event::has_result(), argument_value(index), result_value()` | Check return presence or decode a scalar into `ScalarValue` (bool, signed or unsigned 64-bit integer, or double). Void is present but has no scalar value. |
| `Event::stack()` | Return an owned `std::vector<tracer_repl::StackFrame>` in capture order. Empty when the event has no stored frames. |
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
// Trigger two calls in the target terminal. Iteration waits until the range finishes.
auto results = live
    | filter([](const Event& e) { return e.phase() == "POST"; })
    | transform([](const Event& e) { return e.result_as<int>(); })
    | to_vector();
live.stop();
```

## Native C++ module index API

Use this API in the native REPL to build an index, inspect ELF and DWARF
metadata, or prepare a probe when you have the binary and debug information.
The index is local to the REPL. No target attachment is needed until
injection. The [user guide](user.md#index-debug-information-and-prepare-a-probe-from-a-binary)
shows the equivalent `%module` commands.

| Method | Behavior |
| --- | --- |
| `ModuleSource::file(image, debug_file={})` | Select a local ELF and, optionally, its matching separate debug file. |
| `tracer.module_index(source, output={}, options={})` | Build and select an index. An empty output path uses `<ELF>.tracer.sqlite`. |
| `tracer.open_module_index(path={})` | Open and select a saved index. With an empty path, use the current index or `module-index.sqlite` in the working directory. |
| `ModuleIndex::build(source, output, options={})`, `ModuleIndex::open(path)` | Build or open an index directly from C++. Direct calls leave the REPL's index selection alone. |
| `path()`, `identity()` | Read the SQLite path or the indexed ELF path, SHA-256, build ID, debug companion, and diagnostics. |
| `types(query={})`, `symbols(query={})` | List records and return a page of `entries` and a `next_page` token. A query has a glob `filter`, an optional `limit` (zero means all), and a continuation `page`. `SymbolQuery::exported_only()` restricts the list to dynamic exports. |
| `find_type(name)`, `find_symbol(name)` | Resolve a type by name or a symbol by selector, name, or linkage name. Exact lookup requires a unique match. Use the full selector for overloaded functions. |
| `get_type(id)`, `get_symbol(id)` | Read a particular entry using an ID from the same index. |
| `emit_headers(selection, output)` | Write declarations, read-only field views, and a manifest to a new directory. Fill `ModuleSelection::symbols` and `types` with IDs. |
| `prepare_probe(symbol_id, module, output, generated={}, metadata_only=false)` | Create a prepared package for the indexed symbol. Use an empty `module` to use the recorded ELF path. The output must be new. An optional generated bundle is checked before reuse. `metadata_only` keeps the package and skips separate adapter files. |

`ModuleIndexOptions` accepts `max_entries` and `max_depth` budgets (zero means
unlimited) and a `cancelled` function that can abort indexing. A
`ModuleSymbol` includes its selector, linkage name, parameter type IDs, and
ELF address. Use `%symbols --addresses` for live PCs. A `ModuleType` includes
its kind, optional size and alignment, and known fields with optional byte or
bit offsets.

### Prepare and trace with the C++ API

Compile the [first-probe target](user.md#create-a-target-and-a-basic-probe)
and use its `hook.cpp`. Run these lines in the native REPL from that directory
before attaching. Use new output names on subsequent runs.

```cpp
auto index = tracer.module_index(tracer_module::ModuleSource::file("./target"), "target-api.sqlite");
tracer_module::SymbolQuery query;
query.filter = "add*";
auto matches = index.symbols(query);
auto add_symbol = index.find_symbol("add(int, int)");
auto int_type = index.find_type("int");
std::cout << add_symbol.selector << ' ' << int_type.size.value_or(0) << '\n';
index.emit_headers({{add_symbol.id}, {}}, "recovered-add-api");
index.prepare_probe(add_symbol.id, {}, "prepared-add-api", "recovered-add-api");
```

Run `./target` in a second terminal and substitute its printed PID.
The fourth argument to `tracer.inject` selects the prepared package. Press
Enter in the target terminal while `tracer.collect` waits.

```cpp
auto indexed_target = tracer.attach(12345);
auto indexed_probe = tracer.inject(indexed_target, add_symbol.selector, "hook.cpp", "prepared-add-api");
auto indexed_events = tracer.collect(indexed_target, 2, 30);
tracer.print(indexed_events.at(1));
tracer.detach(indexed_target);
```

The POST event shows `sum=42`. For a later REPL, use
`tracer.open_module_index("target-api.sqlite")`. A standalone C++ application
can include `<tracer/module_index.hpp>` and link `tracer_module_index` to
build, query, and prepare from the same index.

## Agent callback API

For a typed callback, define one top-level explicit specialization of `Interceptor<Signature>` with either or both of `pre(const Call&)` and `post(const Call&)`. Write the original function type as `R(Args...)`, for example `int(int, int)`, or use `tracer::ResolvedSignature` to reuse the type recovered from DWARF or a prepared package for the selected target. The callback compiler supplies the primary template, checks the specialization against the target, and exposes its argument and result types through `Call`. For a supported non-static method, the selector identifies the class and method qualifiers. The signature lists the return and explicit parameter types, and `call.self()` provides the receiver separately.

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

The agent-to-collector `ContextTransport.Export` RPC is a bidirectional stream of `ProducerPacket` and `CollectorControl`. The agent uses it to deliver state descriptors and events, maintain producer identity, and process collector control. Client applications consume these events through `Tracer.WatchEvents` or retained history.

## Build and validate changes

Use a fresh build directory for a different compiler or SDK. Configure and run the native integration tests.

```bash
./build-rhel.sh --toolchain-root /opt/llvm-21 --build-dir build-rhel --with-tests
ctest --test-dir build-rhel --output-on-failure
```

For Python integration and its tests, see the [optional Python reference](python-client.md). See [validation](../validation.md) for detour regressions and benchmark reproduction. To regenerate the REPL grammar, follow the [ANTLR walkthrough](../native-repl.md#regenerate-the-grammar).

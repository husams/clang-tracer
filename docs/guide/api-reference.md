# C++ API appendix

Choose a group, then a type, then a method. The client examples run at the native REPL; agent examples are callback source compiled against the target. This appendix covers the user-facing native C++ APIs. For wire messages, generated support internals, and Python, see the [developer guide](developer.md).

- **Client API:** [Facade](#facade) · [Event](#event) · [EventValue](#eventvalue) · [Collection](#collection) · [EventRange](#eventrange) · [Range adaptors](#range-adaptors) · [EventRecord and EventCodec](#eventrecord-and-eventcodec) · [Reconstructed](#reconstructed) · [Session and event metadata](#session-and-event-metadata) · [Live module metadata](#live-module-metadata)
- **Agent API:** [Call](#call) · [Context and ScopeInit](#context-and-scopeinit) · [TraceContext and callback values](#tracecontext-and-callback-values)
- **Probe and preparation API:** [Interceptor and resolved signature](#interceptor-and-resolved-signature) · [Snapshot extension points](#snapshot-extension-points) · [ModuleIndex](#moduleindex) · [Module index inputs and results](#module-index-inputs-and-results) · [Launch checkpoint](#launch-checkpoint)

## Client API

### Facade

The built-in REPL object `tracer` controls the selected server and exposes live and offline capture. The `session` variable is the current attachment.

[Source declarations](../../src/repl/api.hpp).

**Methods:** [`selected_session`](#facade-selected_session) · [`attach`](#facade-attach) · [`detach`](#facade-detach) · [`modules`](#facade-modules) · [`module_info`](#facade-module_info) · [`prepare`](#facade-prepare) · [`module_index`](#facade-module_index) · [`open_module_index`](#facade-open_module_index) · [`inject`](#facade-inject) · [`collect`](#facade-collect) · [`events`](#facade-events) · [`events_async`](#facade-events_async) · [`record`](#facade-record) · [`replay`](#facade-replay) · [`batch`](#facade-batch) · [`print`](#facade-print)

#### Facade selected_session

`selected_session()`

Read the selected handle. Copy it if you need it after a selection change.

```cpp
auto target = tracer.selected_session();
```

#### Facade attach

`attach(pid)`

Attach by Linux PID and return its handle.

```cpp
auto target = tracer.attach(12345);
```

#### Facade detach

`detach(handle)`

Restore that session's probes and detach it.

```cpp
tracer.detach(session);
```

#### Facade modules

`modules(filter={})`

List modules in the selected target; the filter is a case-sensitive path substring.

```cpp
auto libraries = tracer.modules("lib");
```

#### Facade module_info

`module_info(name)`

Resolve an exact path or unique basename and return its mappings and token.

```cpp
auto image = tracer.module_info(tracer.modules().at(0));
```

#### Facade prepare

`prepare(selector, headers, output, compdb={}, translation_unit={}); prepare(selector, headers, compdb, translation_unit, module, output)`

Create a reusable package in a new directory. Pass one header, a vector, or a braced list with the declaration header first. The six-argument form pins an ELF.

```cpp
tracer.prepare("add(int, int)", "add.hpp", "prepared");
```

#### Facade module_index

`module_index(source, output={}, options={})`

Build and select a local index; the default path derives from the ELF name.

```cpp
auto idx = tracer.module_index(tracer_module::ModuleSource::file("./target"));
```

#### Facade open_module_index

`open_module_index(path={})`

Open and select a saved index. With an empty path use the current index or the working directory default.

```cpp
auto idx = tracer.open_module_index("target.sqlite");
```

#### Facade inject

`inject(handle, selector, callback); inject(handle, selector, callback, package, mode="require")`

Install a local callback and return its probe ID. The second form supplies a package and mode (off, prefer, or require). Use the command when you need additional deployment options.

```cpp
auto id = tracer.inject(session, "add(int, int)", "hook.cpp");
```

#### Facade collect

`collect(handle, count, timeout_seconds=30)`

Wait for a bounded collection and return its counters and retained events.

```cpp
auto data = tracer.collect(session, 2, 30);
```

#### Facade events

`events(handle, count, timeout_seconds=30)`

Capture synchronously and return an iterable range with capture status.

```cpp
auto range = tracer.events(session, 2);
```

#### Facade events_async

`events_async(handle, count=0, timeout_seconds=0)`

Start capture immediately and let the range receive in the background. Zero leaves the corresponding limit open.

```cpp
auto live = tracer.events_async(session, 4, 30);
```

#### Facade record

`record(path, options={})`

Write SQLite and compatible prepared type metadata. The result includes progress and the last cursor.

```cpp
tracer_repl::RecordOptions opts; opts.count = 2; auto saved = tracer.record("capture.db", opts);
```

#### Facade replay

`replay(path, query={})`

Read framed Protobuf or SQLite without a target; SQLite can load compatible recorded mappings.

```cpp
auto data = tracer.replay("capture.db");
```

#### Facade batch

`batch(name)`

Get a named retained collection, for example one created by %collect or %events open.

```cpp
auto data = tracer.batch("results");
```

#### Facade print

`print(event); print(collection)`

Render an event or collection summary in the REPL.

```cpp
tracer.print(tracer.batch("results"));
```


### Event

One serialized observation. In the examples, `e` is a suitable event from `tracer.batch("results").at(0)`.

[Source declarations](../../src/repl/api.hpp).

**Methods:** [`sequence`](#event-sequence) · [`probe_id`](#event-probe_id) · [`call_id`](#event-call_id) · [`phase`](#event-phase) · [`function_name`](#event-function_name) · [`stack`](#event-stack) · [`has_result`](#event-has_result) · [`argument_value`](#event-argument_value) · [`result_value`](#event-result_value) · [`argument_snapshot`](#event-argument_snapshot) · [`result_snapshot`](#event-result_snapshot) · [`receiver_snapshot`](#event-receiver_snapshot) · [`state_snapshot`](#event-state_snapshot) · [`argument_as`](#event-argument_as) · [`result_as`](#event-result_as) · [`state_as`](#event-state_as) · [`argument`](#event-argument) · [`result`](#event-result) · [`describe`](#event-describe)

#### Event sequence

`sequence()`

Read the sequence cursor used by subsequent captures.

```cpp
auto cursor = e.sequence();
```

#### Event probe_id

`probe_id()`

Identify the probe or watch that emitted the event.

```cpp
auto probe = e.probe_id();
```

#### Event call_id

`call_id()`

Correlate events for the same invocation within the probe.

```cpp
auto invocation = e.call_id();
```

#### Event phase

`phase()`

Read the phase label, for example PRE, POST, or EXCEPTION.

```cpp
if (e.phase() == "POST") tracer.print(e);
```

#### Event function_name

`function_name()`

Read the event name or callable selector carried by the event.

```cpp
std::cout << e.function_name();
```

#### Event stack

`stack()`

Copy stored native frames in capture order; empty when no stack was stored.

```cpp
for (const auto& f : e.stack()) std::cout << f.function;
```

#### Event has_result

`has_result()`

Check whether a return is present; void is present but cannot be read as a scalar.

```cpp
if (e.has_result()) std::cout << e.result();
```

#### Event argument_value

`argument_value(index)`

Decode a numeric or boolean argument as ScalarValue, a variant.

```cpp
auto scalar = e.argument_value(0);
```

#### Event result_value

`result_value()`

Decode a numeric or boolean return as ScalarValue.

```cpp
auto scalar = e.result_value();
```

#### Event argument_snapshot

`argument_snapshot(index)`

Copy the captured argument as a structured value. Indexes start at zero.

```cpp
auto input = e.argument_snapshot(0);
```

#### Event result_snapshot

`result_snapshot()`

Copy the captured return, including a pointee if the probe captured it.

```cpp
auto item = e.result_snapshot();
```

#### Event receiver_snapshot

`receiver_snapshot()`

Copy the receiver snapshot stored on the event; ordinary prepared probes expose entry fields unless return sampling is enabled.

```cpp
auto before = e.receiver_snapshot();
```

#### Event state_snapshot

`state_snapshot(index=0)`

Copy the indexed state from a named Context event.

```cpp
auto state = e.state_snapshot();
```

#### Event argument_as

`argument_as<T>(index)`

Decode one argument using EventValue::as<T>().

```cpp
auto left = e.argument_as<int>(0);
```

#### Event result_as

`result_as<T>()`

Decode a return; generated mappings may produce a Reconstructed<T> wrapper.

```cpp
auto answer = e.result_as<int>();
```

#### Event state_as

`state_as<T>(index=0)`

Decode a named event's state to the requested local type.

```cpp
auto state = e.state_as<tracer_repl::EventValue>();
```

#### Event argument

`argument(index)`

Render an argument as a display string.

```cpp
std::cout << e.argument(0);
```

#### Event result

`result()`

Render the return as text; a pointer return can render as its address while still carrying a snapshot.

```cpp
std::cout << e.result();
```

#### Event describe

`describe()`

Render the event as a human-readable string.

```cpp
std::cout << e.describe();
```


### EventValue

An owned scalar, string, record, or array. In the examples, `value` is an existing snapshot with the appropriate shape. References to child fields and elements are valid while that snapshot is alive.

[Source declarations](../../src/repl/event_value.hpp).

`Object` is a string-to-value map; `Array` is a vector. `Data` holds monostate, bool, signed or unsigned 64-bit integer, double, string, Object, or Array. `ScalarValue` is a separate variant for numeric and boolean values.

**Methods:** [`cpp_type`](#eventvalue-cpp_type) · [`descriptor_type`](#eventvalue-descriptor_type) · [`container_role`](#eventvalue-container_role) · [`matches_record_type`](#eventvalue-matches_record_type) · [`data`](#eventvalue-data) · [`is_null`](#eventvalue-is_null) · [`field`](#eventvalue-field) · [`fields`](#eventvalue-fields) · [`elements`](#eventvalue-elements) · [`at`](#eventvalue-at) · [`size`](#eventvalue-size) · [`as`](#eventvalue-as)

#### EventValue cpp_type

`cpp_type()`

Return the captured C++ type name.

```cpp
std::cout << value.cpp_type();
```

#### EventValue descriptor_type

`descriptor_type()`

Return the descriptor type name when a schema supplies it.

```cpp
std::cout << value.descriptor_type();
```

#### EventValue container_role

`container_role()`

Read the role metadata for descriptor-backed containers, if present.

```cpp
std::cout << value.container_role();
```

#### EventValue matches_record_type

`matches_record_type(name)`

Check the recorded type against the expected record name, with the runtime's normalization.

```cpp
bool expected = value.matches_record_type("Item");
```

#### EventValue data

`data()`

Access the underlying Data variant by const reference.

```cpp
bool object = std::holds_alternative<tracer_repl::EventValue::Object>(value.data());
```

#### EventValue is_null

`is_null()`

Check whether the variant contains no value.

```cpp
if (value.is_null()) std::cout << "unavailable";
```

#### EventValue field

`field(name)`

Get one named member. Missing names or a nonrecord value throw.

```cpp
auto id = value.field("id").as<int>();
```

#### EventValue fields

`fields()`

Get the record's map of fields without copying it.

```cpp
if (value.fields().count("impl_snapshot")) std::cout << "captured";
```

#### EventValue elements

`elements()`

Get the array of elements; throws for a nonarray value.

```cpp
for (const auto& entry : value.elements()) std::cout << entry.cpp_type();
```

#### EventValue at

`at(index)`

Access one captured array element with bounds checks.

```cpp
auto first_value = value.at(0);
```

#### EventValue size

`size()`

Count fields, elements, or string bytes; other scalars have no size.

```cpp
auto members = value.size();
```

#### EventValue as

`as<T>()`

Convert an owned snapshot. Supports scalars and provided container codecs, plus custom EventCodec or generated mappings. Target pointers and references are not conversion targets.

```cpp
auto id = value.field("id").as<int>();
```


### Collection

A retained collection with counters. In the examples, `data = tracer.batch("results")`.

[Source declarations](../../src/repl/api.hpp).

`events` holds the retained vector. `received`, `evicted`, `complete`, `cancelled`, and `timed_out` distinguish retention from how capture ended.

**Methods:** [`size`](#collection-size) · [`at`](#collection-at) · [`begin`](#collection-begin) · [`end`](#collection-end)

#### Collection size

`size()`

Count the retained events.

```cpp
auto retained = data.size();
```

#### Collection at

`at(index)`

Return the event at a checked zero-based position.

```cpp
if (data.size()) tracer.print(data.at(0));
```

#### Collection begin

`begin()`

Return the const iterator to the first retained event.

```cpp
if (data.begin() != data.end()) tracer.print(*data.begin());
```

#### Collection end

`end()`

Return the const end iterator.

```cpp
for (auto it = data.begin(); it != data.end(); ++it) tracer.print(*it);
```


### EventRange

An iterable capture returned by `tracer.events` or `tracer.events_async`. Use `live` from the [streaming lesson](tutorials.md#lesson-18-stream-and-release-a-counted-range).

[Source declarations](../../src/repl/api.hpp).

The single-pass iterator supports dereference, increment and comparison with the sentinel. Release all range copies to free retained storage.

**Methods:** [`snapshot`](#eventrange-snapshot) · [`running`](#eventrange-running) · [`error`](#eventrange-error) · [`stop`](#eventrange-stop) · [`begin`](#eventrange-begin) · [`end`](#eventrange-end)

#### EventRange snapshot

`snapshot()`

Copy currently retained events without waiting.

```cpp
auto partial = live.snapshot();
```

#### EventRange running

`running()`

Check whether the producer is active.

```cpp
if (live.running()) std::cout << "receiving";
```

#### EventRange error

`error()`

Read the stream error text; empty when no error is recorded.

```cpp
if (!live.error().empty()) std::cerr << live.error();
```

#### EventRange stop

`stop()`

Stop receiving and preserve the retained data.

```cpp
live.stop();
```

#### EventRange begin

`begin()`

Start at the oldest retained event; iteration waits for additional events while capture is running.

```cpp
for (const auto& e : live) tracer.print(e);
```

#### EventRange end

`end()`

Return the sentinel. Iteration ends when no next event can be read, including cancellation.

```cpp
for (auto it = live.begin(); it != live.end(); ++it) tracer.print(*it);
```


### Range adaptors

Use these C++17 helpers in namespace `tracer_repl`. Examples assume a retained collection `data` and `using namespace tracer_repl;`. Iterating a live range can wait for new data.

[Source declarations](../../src/repl/event_ranges.hpp).

**Methods:** [`take`](#range-adaptors-take) · [`drop`](#range-adaptors-drop) · [`first`](#range-adaptors-first) · [`stride`](#range-adaptors-stride) · [`enumerate`](#range-adaptors-enumerate) · [`filter`](#range-adaptors-filter) · [`take_while`](#range-adaptors-take_while) · [`drop_while`](#range-adaptors-drop_while) · [`transform`](#range-adaptors-transform) · [`to_vector`](#range-adaptors-to_vector) · [`reverse`](#range-adaptors-reverse)

#### Range adaptors take

`take(count)`

Limit iteration to the first count items.

```cpp
auto top = data | take(3) | to_vector();
```

#### Range adaptors drop

`drop(count)`

Skip the first count items.

```cpp
auto rest = data | drop(1) | to_vector();
```

#### Range adaptors first

`first()`

Return an optional copy of the first item.

```cpp
auto earliest = data | first();
```

#### Range adaptors stride

`stride(step)`

Read one item per positive step.

```cpp
auto every_other = data | stride(2) | to_vector();
```

#### Range adaptors enumerate

`enumerate()`

Pair each item with a zero-based index.

```cpp
for (const auto& [index, e] : data | enumerate()) std::cout << index << ' ' << e.phase();
```

#### Range adaptors filter

`filter(predicate)`

Yield only items matching the predicate.

```cpp
auto posts = data | filter([](const Event& e) { return e.phase() == "POST"; });
```

#### Range adaptors take_while

`take_while(predicate)`

Yield the initial prefix that matches.

```cpp
auto prefix = data | take_while([](const Event& e) { return e.phase() == "PRE"; });
```

#### Range adaptors drop_while

`drop_while(predicate)`

Skip the initial matching prefix, then keep the rest.

```cpp
auto rest = data | drop_while([](const Event& e) { return e.phase() == "PRE"; });
```

#### Range adaptors transform

`transform(function)`

Project each item through a function during iteration.

```cpp
auto names = data | transform([](const Event& e) { return e.function_name(); }) | to_vector();
```

#### Range adaptors to_vector

`to_vector()`

Materialize the current sequence into a vector.

```cpp
auto copy = data | to_vector();
```

#### Range adaptors reverse

`reverse()`

Materialize the sequence in reverse order.

```cpp
auto newest_first = data | reverse();
```


### EventRecord and EventCodec

Map a snapshot to a local type before instantiating a typed conversion. See [custom decoding](advanced.md#write-a-client-side-decoder) for a complete specialization.

[Source declarations](../../src/repl/event_value.hpp).

**Methods:** [`fields`](#eventrecord-and-eventcodec-fields) · [`event_member`](#eventrecord-and-eventcodec-event_member) · [`decode`](#eventrecord-and-eventcodec-decode)

#### EventRecord and EventCodec fields

`EventRecord<T>::fields()`

Return a tuple of member descriptors along with a matching static type_name on the specialization. Requires supported assignable members on a default-constructible type.

```cpp
static auto fields() { return std::make_tuple(tracer_repl::event_member("id", &LocalItem::id)); }
```

#### EventRecord and EventCodec event_member

`event_member(name, pointer_to_member)`

Describe a C++ destination field and its source key.

```cpp
auto member = tracer_repl::event_member("id", &LocalItem::id);
```

#### EventRecord and EventCodec decode

`EventCodec<T>::decode(const EventValue& value)`

Customize construction or select a different schema. Specialize EventCodec<T> before use.

```cpp
static LocalItem decode(const tracer_repl::EventValue& value) { return LocalItem{value.field("id").as<int>()}; }
```


### Reconstructed

A wrapper for local inspection of a generated field-by-field reconstruction. The application constructor and destructor do not run; virtual calls are unavailable. Do not move, copy, mutate, or destroy the underlying application record. Child views must not outlive the wrapper.

[Source declarations](../../src/repl/event_value.hpp).

`ReconstructedRecord<T>` and its builder methods support generated mappings; use `EventCodec<T>` for hand-written local conversions.

**Methods:** [`get`](#reconstructed-get) · [`dereference`](#reconstructed-dereference) · [`member access`](#reconstructed-member-access)

#### Reconstructed get

`get()`

Get a borrowed pointer to local reconstructed storage.

```cpp
auto* local = reconstructed.get();
```

#### Reconstructed dereference

`operator*()`

Borrow the rebuilt record for inspection.

```cpp
auto id = (*reconstructed).id;
```

#### Reconstructed member access

`operator->()`

Inspect a field without taking ownership of the record.

```cpp
auto id = reconstructed->id;
```


### Session and event metadata

Read these records without taking ownership of an agent resource.

[Source declarations](../../src/repl/api.hpp).

- `SessionHandle`: `id`, `pid`, `connection`. Get a fresh handle after reconnecting.
- `StackFrame`: `pc`, `stack_pointer`, `kind`, `module`, `module_offset`, `function`, `file`, `line`, `application_frame`. Example: `for (const auto& f : e.stack()) std::cout << f.file << ':' << f.line;`
- `RecordOptions`: `session`, `all_sessions`, `names`, `phase`, `probe_id`, `after_sequence`, `count`, `timeout_seconds`. Example: `opts.names = {"object.trace"};`
- `RecordResult`: `received`, `cursor`, `complete`, `cancelled`, `timed_out`. Use `opts.after_sequence = saved.cursor;` to resume a recording.
- `EventQuery`: `names`, `phase`, `pid`, `session_id`, `count`. Example: `query.phase = "POST";`


### Live module metadata

Use `auto paths = tracer.modules();` in an attached session.

[Source declarations](../../src/repl/api.hpp).

`ModuleList::paths` is the path vector. `ModuleInfo` contains `path`, `mapping_token`, and `mappings`. Each `ModuleMapping` carries `begin_address`, exclusive `end_address`, `file_offset`, `device`, `inode`, and `build_id`.

**Methods:** [`empty`](#live-module-metadata-empty) · [`size`](#live-module-metadata-size) · [`at`](#live-module-metadata-at) · [`begin`](#live-module-metadata-begin) · [`end`](#live-module-metadata-end)

#### Live module metadata empty

`ModuleList::empty()`

Check whether the module query returned paths.

```cpp
if (paths.empty()) std::cout << "no matches";
```

#### Live module metadata size

`ModuleList::size()`

Count paths.

```cpp
auto count = paths.size();
```

#### Live module metadata at

`ModuleList::at(index)`

Get a module path with bounds checking.

```cpp
auto image = tracer.module_info(paths.at(0));
```

#### Live module metadata begin

`ModuleList::begin()`

Return an iterator to the first path.

```cpp
if (!paths.empty()) std::cout << *paths.begin();
```

#### Live module metadata end

`ModuleList::end()`

Return the end iterator.

```cpp
for (const auto& path : paths) std::cout << path << '\n';
```


## Agent API

### Call

The typed agent hook parameter (`const Call& call`). Examples assume the first-probe integer signature unless specified. Keep borrowed values inside the callback.

[Source declarations](../../include/tracer/callback.hpp).

See [scalar override prerequisites](../native-repl.md#opt-in-to-scalar-overrides) before using mutating APIs.

**Methods:** [`context`](#call-context) · [`arg`](#call-arg) · [`argument`](#call-argument) · [`argument_count`](#call-argument_count) · [`argument_name`](#call-argument_name) · [`argument_value`](#call-argument_value) · [`result`](#call-result) · [`result_value`](#call-result_value) · [`object_address`](#call-object_address) · [`self`](#call-self) · [`field`](#call-field) · [`probe_id`](#call-probe_id) · [`id`](#call-id) · [`print_options`](#call-print_options) · [`capture_stack`](#call-capture_stack) · [`stack`](#call-stack) · [`log`](#call-log) · [`override_argument`](#call-override_argument) · [`override_result`](#call-override_result) · [`replace_with`](#call-replace_with) · [`call_original`](#call-call_original)

#### Call context

`context()`

Access the shared-context facade for this hook.

```cpp
auto tid = call.context().thread_id();
```

#### Call arg

`arg<T>(index)`

Read a checked scalar or pointer argument using an explicit matching type.

```cpp
auto left = call.arg<int>(0);
```

#### Call argument

`argument<Index>(); argument<T>(index); argument<T>(name, index)`

Read by compiler-resolved zero-based position, decode with a supplied type, or decode and emit a named argument. Inferred record or reference values are borrowed.

```cpp
call.field("left", call.argument<0>());
```

#### Call argument_count

`argument_count()`

Count explicit arguments; the receiver is separate.

```cpp
auto n = call.argument_count();
```

#### Call argument_name

`argument_name(index)`

Read a borrowed name, or argN when none is available.

```cpp
call.log(call.argument_name(0));
```

#### Call argument_value

`argument_value(index)`

Read a borrowed raw TraceValue. Inspect its tag before using the corresponding value slot.

```cpp
auto tag = call.argument_value(0).type;
```

#### Call result

`result<T>(); result()`

Read the original result in POST. Omit T for compiler inference. Inferred object or reference results can be borrowed; void returns no value.

```cpp
call.field("sum", call.result());
```

#### Call result_value

`result_value()`

Read the raw return value in POST.

```cpp
auto tag = call.result_value().type;
```

#### Call object_address

`object_address()`

Read the receiver address; zero if absent.

```cpp
auto address = call.object_address();
```

#### Call self

`self()`

Read the const receiver of a supported typed non-static method.

```cpp
auto const* receiver = call.self();
```

#### Call field

`field(name, value); field(member_expression)`

Emit a named scalar or address. The single argument form needs compiler member-name inference.

```cpp
call.field("double_left", 2 * call.argument<0>());
```

#### Call probe_id

`probe_id()`

Read the active probe ID.

```cpp
auto id = call.probe_id();
```

#### Call id

`id()`

Read the invocation ID shared by its corresponding PRE and POST.

```cpp
call.field("invocation", call.id());
```

#### Call print_options

`print_options()`

Read the automatic rendering preferences for arguments and result.

```cpp
auto options = call.print_options();
```

#### Call capture_stack

`capture_stack(depth=16); capture_stack(depth, application_only)`

Capture the invocation's entry stack. Depth must be positive; the one-argument form uses the probe filter.

```cpp
auto count = call.capture_stack(12, true).count;
```

#### Call stack

`stack()`

Read the stored or explicitly captured stack view.

```cpp
auto count = call.stack().count;
```

#### Call log

`log(message, level=LogLevel::Info)`

Queue a bounded diagnostic; returns whether it was accepted.

```cpp
call.log("entered add", tracer::LogLevel::Info);
```

#### Call override_argument

`override_argument(index, value)`

Stage an exact-type scalar argument override in PRE with --allow-arg-overrides.

```cpp
call.override_argument(0, 21);
```

#### Call override_result

`override_result(value)`

Stage an exact-type scalar return override in POST with --allow-result-override.

```cpp
call.override_result(42);
```

#### Call replace_with

`replace_with(value); replace_with()`

Supply a scalar or void replacement in PRE with --allow-replacement, skipping automatic original execution.

```cpp
call.replace_with(42);
```

#### Call call_original

`call_original<T=void>()`

In PRE with --allow-call-original, invoke the original with entry arguments. Repeated calls reuse the first result; exceptions propagate to the caller.

```cpp
int original = call.call_original<int>();
```


### Context and ScopeInit

Typed shared state is declared by the callback and admitted by the generated facade. Examples assume a configured `RequestScope` and `RequestState`; see the [request lesson](tutorials.md#collect-every-call-of-one-request).

[Source declarations](../../include/tracer/context.hpp).

Inherit from `state<Scope>` or `shared_state<Scope>` to associate a payload with a scope; declare constructor dependencies as `State<Payload>`. Ordinary state requires application-side synchronization.

**Methods:** [`get`](#context-and-scopeinit-get) · [`with`](#context-and-scopeinit-with) · [`init_scope`](#context-and-scopeinit-init_scope) · [`end_scope`](#context-and-scopeinit-end_scope) · [`event`](#context-and-scopeinit-event) · [`thread_id`](#context-and-scopeinit-thread_id) · [`root_call`](#context-and-scopeinit-root_call)

#### Context and ScopeInit get

`get<T>(); get<Scope, Payload>()`

Borrow the currently bound payload. Use the guarded visitor for shared access; keep any permitted borrow inside the hook.

```cpp
auto& request = call.context().get<RequestState>();
```

#### Context and ScopeInit with

`with<T>(visitor); with<Scope, Payload>(visitor)`

Run a callback with the selected payload under its access rules.

```cpp
call.context().with<RequestState>([](RequestState& s) { ++s.total; });
```

#### Context and ScopeInit init_scope

`init_scope<Scope>(); init_scope<Scope>(payload)`

Create a scope with the configured initializer or supplied initial payload.

```cpp
call.context().init_scope<RequestScope>();
```

#### Context and ScopeInit end_scope

`end_scope<Scope>()`

End that scope. Accepted child work can keep leases until completion.

```cpp
call.context().end_scope<RequestScope>();
```

#### Context and ScopeInit event

`event(name, max_items=100)`

Publish a bounded descriptor-backed state snapshot under a name; cap is 0 to 4096.

```cpp
call.context().event("request.completed", 32);
```

#### Context and ScopeInit thread_id

`thread_id()`

Read the Linux thread ID of the hook.

```cpp
auto tid = call.context().thread_id();
```

#### Context and ScopeInit root_call

`ScopeInit::root_call()`

Read the root invocation during payload initialization.

```cpp
explicit RequestState(const tracer::ScopeInit& init) : total(init.root_call().arg<int>(0)) {}
```


### TraceContext and callback values

Legacy callbacks and the low-level protocol expose borrowed agent views. Prefer Call for typed hooks.

[Source declarations](../../include/tracer/protocol.hpp).

`TracePhase` identifies the hook phase; `TraceValue::type` selects the relevant scalar or address slot and may be accompanied by a `ValueSnapshot`. Check snapshot status and detail before inspecting its payload. `StackTrace` describes depth, filtering, status, and a borrowed frame array. `CallbackPrintOptions` carries argument and result rendering preferences. See [the callback walkthrough](developer.md#agent-callback-api) for the lifetime rules.

**Methods:** [`log`](#tracecontext-and-callback-values-log)

#### TraceContext and callback values log

`TraceContext::log(message, level=LogLevel::Info)`

Queue a diagnostic from a legacy raw callback.

```cpp
extern "C" long long trace_callback(const TraceContext& ctx) { ctx.log("entered"); return 0; }
```


## Probe and preparation API

### Interceptor and resolved signature

Define one top-level specialization of the compiler-supplied `Interceptor<R(Args...)>` in the callback file. Use `tracer::ResolvedSignature` when the prepared or DWARF-resolved signature is authoritative. Methods list only their explicit parameters in the signature.

[Source declarations](../../include/tracer/callback.hpp).

See [the first probe](user.md#create-a-target-and-a-basic-probe) for a complete callback, [probe validation](command-reference.md#command-probe-validate) before installing, and [legacy callbacks](../../examples/legacy-callbacks/README.md) for the raw C entry hook.

**Methods:** [`pre`](#interceptor-and-resolved-signature-pre) · [`post`](#interceptor-and-resolved-signature-post)

#### Interceptor and resolved signature pre

`pre(const Call&)`

Observe entry before the original runs. Returning an optional long long publishes a callback value without changing the original result.

```cpp
void pre(const Call& call) { call.field("left", call.argument<0>()); }
```

#### Interceptor and resolved signature post

`post(const Call&)`

Observe a normal return. The inferred result follows the original signature.

```cpp
void post(const Call& call) { call.field("sum", call.result()); }
```


### Snapshot extension points

Extensions run at different stages. Use the [advanced chapter](advanced.md) for setup, ownership, and before/after checks.

[Source declarations](../../src/prepare_injectors_main.cpp).

**Methods:** [`followpointers`](#snapshot-extension-points-followpointers) · [`encode`](#snapshot-extension-points-encode) · [`anypayloads`](#snapshot-extension-points-anypayloads)

#### Snapshot extension points followpointers

`FollowPointers<Record>::fields`

Select direct pointer or reference members to copy in a prepared probe.

```cpp
static constexpr const char* fields[] = {"impl"};
```

#### Snapshot extension points encode

`tracer_snapshot::encode(const Implementation&) noexcept`

Return a custom record by value in place of the automatic implementation snapshot.

```cpp
V2Projection encode(const HFW::ItemV2& item) noexcept { return {static_cast<int>(item.generation) + 1000}; }
```

#### Snapshot extension points anypayloads

`AnyPayloads<Record>`

Register the allowed concrete record types for a direct std::any method-snapshot field.

```cpp
using extension = std::tuple<demo::RiskState>;
```


### ModuleIndex

Offline ELF and DWARF API in namespace `tracer_module`. Substituting the first-probe ELF and using new output paths, build an index and set `auto hit = idx.find_symbol("add(int, int)");` for examples that require a symbol ID.

[Source declarations](../../include/tracer/module_index.hpp).

**Methods:** [`build`](#moduleindex-build) · [`open`](#moduleindex-open) · [`path`](#moduleindex-path) · [`identity`](#moduleindex-identity) · [`types`](#moduleindex-types) · [`symbols`](#moduleindex-symbols) · [`find_type`](#moduleindex-find_type) · [`find_symbol`](#moduleindex-find_symbol) · [`get_type`](#moduleindex-get_type) · [`get_symbol`](#moduleindex-get_symbol) · [`emit_headers`](#moduleindex-emit_headers) · [`prepare_probe`](#moduleindex-prepare_probe)

#### ModuleIndex build

`build(source, output, options={})`

Create a new SQLite module index; optional budgets and cancellation apply.

```cpp
auto idx = tracer_module::ModuleIndex::build(tracer_module::ModuleSource::file("./target"), "target.sqlite");
```

#### ModuleIndex open

`open(path)`

Open an existing local index without changing the REPL selection.

```cpp
auto idx = tracer_module::ModuleIndex::open("target.sqlite");
```

#### ModuleIndex path

`path()`

Return the index file path.

```cpp
std::cout << idx.path();
```

#### ModuleIndex identity

`identity()`

Read ELF and debug identity and indexing diagnostics.

```cpp
std::cout << idx.identity().build_id;
```

#### ModuleIndex types

`types(query={})`

Return indexed types and an optional next-page token.

```cpp
tracer_module::ModuleQuery q; q.filter = "int"; auto page = idx.types(q);
```

#### ModuleIndex symbols

`symbols(query={})`

Return indexed symbols, optionally only dynamic exports.

```cpp
auto page = idx.symbols(tracer_module::SymbolQuery{}.exported_only());
```

#### ModuleIndex find_type

`find_type(name)`

Find a uniquely named type.

```cpp
auto type = idx.find_type("int");
```

#### ModuleIndex find_symbol

`find_symbol(name)`

Resolve an unambiguous selector, linkage name, or name.

```cpp
auto hit = idx.find_symbol("add(int, int)");
```

#### ModuleIndex get_type

`get_type(id)`

Read a type by stable ID from this index.

```cpp
auto type = idx.get_type(idx.find_type("int").id);
```

#### ModuleIndex get_symbol

`get_symbol(id)`

Read a symbol by stable ID from this index.

```cpp
auto symbol = idx.get_symbol(hit.id);
```

#### ModuleIndex emit_headers

`emit_headers(selection, output)`

Write recovered declarations, views and manifest to a new directory.

```cpp
idx.emit_headers({{hit.id}, {}}, "recovered");
```

#### ModuleIndex prepare_probe

`prepare_probe(symbol_id, module, output, generated={}, metadata_only=false)`

Make an indexed package for that symbol. Empty module uses the recorded ELF. A supplied generated bundle is verified; metadata_only skips separate adapter files.

```cpp
idx.prepare_probe(hit.id, {}, "prepared", "recovered");
```


### Module index inputs and results

These options and results belong to the offline module index; ELF addresses are relative to the image.

[Source declarations](../../include/tracer/module_index.hpp).

- `ModuleSource`: `image`, `debug_file`, `expected_build_id`.
- `ModuleIndexOptions`: `max_entries`, `max_depth` (zero leaves the bound open), `cancelled` predicate.
- `ModuleQuery`: `filter` glob, `page` continuation, `limit` (zero reads all); `SymbolQuery` adds `exports`.
- `ModulePage<Entry>`: `entries` and `next_page`. Keep the query filters unchanged when using the next token.
- `ModuleSelection`: `symbols` and `types` are ID lists.
- `ModuleIdentity` provides image and debug paths, digests, build ID, and diagnostics. `ModuleType` and `ModuleField` describe recovered types, sizes, fields and offsets. `ModuleSymbol` and `ModuleParameter` describe callable identity, source, return type and parameter type IDs. See the declarations linked above for every field.

**Methods:** [`file`](#module-index-inputs-and-results-file) · [`exported_only`](#module-index-inputs-and-results-exported_only)

#### Module index inputs and results file

`ModuleSource::file(image, debug_file={})`

Describe an ELF with an optional matching separate debug file.

```cpp
auto source = tracer_module::ModuleSource::file("./target", "./target.debug");
```

#### Module index inputs and results exported_only

`SymbolQuery::exported_only()`

Set the dynamic-export filter and return the query by reference.

```cpp
tracer_module::SymbolQuery q; q.exported_only();
```


### Launch checkpoint

An opt-in checkpoint in target application code. Use it with [owned launch and registered calls](../native-repl.md#launch-a-local-application).

[Source declarations](../../include/tracer/launch.hpp).

**Methods:** [`wait_for_resume`](#launch-checkpoint-wait_for_resume) · [`named_call`](#launch-checkpoint-named_call)

#### Launch checkpoint wait_for_resume

`wait_for_resume(); wait_for_resume(calls...)`

Pause at the launch gate and service registered calls before resume. Check the boolean result before continuing startup.

```cpp
if (!tracer::launch::wait_for_resume()) return 1;
```

#### Launch checkpoint named_call

`named_call(name, function); named_call(name, receiver, method)`

Register an exact name and a compiler-checked function or bound method. Keep the receiver alive until resume.

```cpp
auto call = tracer::launch::named_call("add", &add); if (!tracer::launch::wait_for_resume(call)) return 1;
```

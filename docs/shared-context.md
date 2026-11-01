# Shared context

Shared context lets independently compiled class interceptors keep typed C++
state and publish a snapshot with `Call::context().event()`. The stream carries
descriptor-backed values, so the CLI can decode fields across schema revisions
without a user-maintained JSON schema.

## Declare and use state

Include the public `tracer/context.hpp` header in callback sources that declare
context types. Mark each state with its scope and inject it through the
interceptor constructor:

```cpp
#include <string>
#include <vector>
#include <tracer/callback.hpp>
#include <tracer/context.hpp>

struct RequestScope {};

struct RequestState : tracer::shared_state<RequestScope> {
  std::string request_id;
  std::vector<std::string> items;
};

struct Interceptor {
  Interceptor(tracer::State<RequestState>);

  void pre(const tracer::Call&, RequestState& state) {
    state.request_id = "request-42";
    state.items.push_back("book");
  }

  void post(const tracer::Call& call) {
    auto& state = call.context().get<RequestState>();
    call.context().with<RequestState>([](RequestState& value) {
      value.items.push_back("shipped");
    });
    call.context().event("request.completed");
  }
};
```

`State<T>` declares a constructor dependency; the engine constructs and binds
the state before the hook and can inject `T&` into a hook parameter. Use
`get<T>()` for direct typed access or `with<T>(visitor)` for a scoped visitor.
`shared_state<Scope>` identifies state shared by compatible calls and probes;
`state<Scope>` identifies ordinary scoped state. Synchronization for ordinary
state remains the callback author's responsibility. Shared state access follows
the engine's guarded access rules; do not retain references beyond the hook or
visitor call.

Generated owner construction can initialize a payload from the source hook's
root call with `T(const tracer::ScopeInit&)`; `init.root_call()` exposes that
call only during initialization. For generic declarations, use
`call.context().init_scope<Scope>()`, the typed-payload overload
`init_scope<Scope>(payload)`, and `end_scope<Scope>()`. Scope initialization and
teardown are runtime-managed; they do not require a user token or global map.

Call `event(name, max_items)` to publish a complete snapshot of the visible
state. The default item cap is 100; the accepted range is 0 through 4096.
The serialized event payload and its metadata have a 256 KiB capture limit.
Descriptor sets are admitted separately, with a 1 MiB limit per descriptor
set; they do not consume that event-payload limit. Admission is atomic: if the
event or any required descriptor cannot fit its respective bounded budget,
the event is dropped rather than partially published.

## Thread propagation

When a supported `pthread_create` call happens in an active application hook,
the native adapter captures the full eligible context-task chain and attaches
it to the new thread. This also covers libraries that create threads through
`pthread_create`; callbacks do not need to wrap the call or pass a token.
Accepted child work retains its state and probe leases until it completes, so
removing the last related probe may wait for that work to drain. Propagation is
limited to application execution. The adapter fails open to the original
`pthread_create` call when it cannot safely admit propagation, and records a
bounded diagnostic. Native `pthread_create` detouring is supported only when
the target's CET status is verified inactive for all threads; active or
unverified CET configurations are rejected for this propagation mode.

## Events and field queries

Context events are included in the regular `events` and `WatchEvents` output.
The viewer discovers the union of field paths present in retained descriptors
for a state family by default. Select one or several dotted paths with
`--context-field`; a path missing from a particular revision is shown as
`<missing>` rather than converted to a different field:

```sh
./build/clang-tracer events "$PID" --count 20
./build/clang-tracer events "$PID" --count 20 \
  --context-field request_id,items.sku
```

The output labels each value with its state family and schema revision. Event
arguments and results remain hidden unless the callback's print options request
them. A requested native entry stack is retained and displayed with the same
Watch event.

## Package lineage

The engine assigns each recognized callback owner package a random 16-byte
lineage UUID and persists it in engine-managed package metadata. Callback files
in the same recognized package reuse that lineage; edits and file renames do
not derive a new identity from source text, path, type spelling or build ID.
The package root is located from existing retained metadata or the nearest
`.git` / `compile_commands.json` root, with the callback directory as the
standalone fallback. Imported definitions are associated with their defining
package. The UUID is internal; there is no public source-origin UUID option.

If a package cannot be verified or its metadata cannot be retained, the engine
isolates that source with a distinct lineage and reports a diagnostic. This
prevents unrelated or unverifiable packages with identical C++ names from
sharing state. The package metadata is separate from callback contents and is
created only during probe setup.

## Build and recording

Shared context is supported by the Linux x86-64 build profile. It requires the
matching static `libstdc++.a` and `libgcc_eh.a` selected by the build compiler.
CMake packages that exact standard-library runtime in the sealed
`libclang_tracer_context_stdlib.so` support image. Runtime admission checks its
ABI, object layouts and archive identity before activating state callbacks;
unsupported or mismatched profiles are rejected. `TRACER_JIT_FLAGS` still
selects callback compilation options, but it does not replace the sealed
runtime profile.

Context streaming uses bounded in-memory, best-effort delivery by default. To
enable durable, resumable producer and collector journals, give the server a
private directory that can retain its contents across restarts:

```sh
./build/clang-tracer-server \
  --agent "$PWD/build/libclang_tracer_agent.so" \
  --listen "unix:///tmp/clang-tracer/control.sock" \
  --context-durable-dir /var/lib/clang-tracer/context
```

The server creates per-recording collector journals in stable hashed
subdirectories and per-process producer journals under this directory. Each
recording/sink pair has an independent durable checkpoint. Keep the directory
across restarts to resume those recordings; deleting it starts new recordings.
The ordinary trace API remains available when durable recording is not enabled.

## Acceptance source

The unchanged [nested-state fixture](../tests/fixtures/shared_context_nested.cpp)
exercises constructor injection, nested containers, mutation in PRE/POST and
typed event publication. The live CLI integration is in
[`class_interceptor_integration.py`](../tests/class_interceptor_integration.py).

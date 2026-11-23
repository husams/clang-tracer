# Shared context

For a commented, runnable introduction, start with the
[two-probe worker example](../examples/shared-context/README.md).

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
visitor call. A shared state must own its string data. Declare a
`std::string` member for text that another hook or a later event will read.
Preparation rejects a `std::string_view` member, including one nested in a
record or container.

A hook, and any helper it calls, can read the target through pointers: cast an
argument, walk an object, return a pointer from a helper. Copy what you need
into the state as owned values. A pointer that can refer to context state is
different: one that comes from `get`, `with`, an injected state parameter or the
`state` shorthand, or from anything computed from them, cannot be cast or
returned from a helper. The callback is rejected with
`unsupported_context_effect` and the line and column of the cast or the
`return`.

### Call functions of the target

A hook can call a function or a method of the target process, for example a
`serialize()` that the application already has:

```cpp
void post(const tracer::Call& call) {
  Payload payload;
  for (const demo::Item* item : call.argument<0>())
    payload.items.push_back(item->serialize());   // runs in the target
  call.context().init_scope<Scope>(std::move(payload));
  call.context().event("demo.populated");
}
```

A context hook runs in the engine's own C++ runtime, with its own allocator and
exception handling, and the target function runs in the target's runtime. The
engine therefore puts a bridge between them. The function is a target function
when it has no body in the callback and is declared by the prepared adapter, by
the callback, or by a project header. A prepared adapter declares the virtual
methods of a class; declare any other function in the callback. The bridge
finds a function by its symbol, the same way an ordinary callback does, so the
target has to export it. A function that cannot be found rejects the callback
when it is validated or installed.

The bridge carries:

- arguments that are arithmetic or enumeration values, pointers and lvalue
  references, and the receiver of a method;
- a result that is `void`, an arithmetic or enumeration value, a pointer, an
  lvalue reference, or a `std::string` by value. The text of a `std::string` is
  copied into the hook's runtime, so it can be stored in the state. A pointer
  or reference result still points into the target: read it, do not keep it.

Rules:

- The target function never runs under a context lock. A hook that takes an
  injected state, calls `get`, or uses the `state` shorthand cannot call a
  target function, and neither can a `with` visitor. Call the function first
  and then store the value through `with`. The callback is rejected with
  `invalid_context_access_plan`.
- The call has to be written in the callback source itself, in a function that
  is not a template. A helper that calls a target function has to be `inline`
  or `static`. Pass every argument explicitly; default arguments are not
  filled in.
- A class passed or returned by value (other than a `std::string` result), an
  overloaded operator, a constructor, a destructor and a function with a
  variable argument list cannot be called. The callback is rejected with
  `unsupported_context_effect`, the name of the function, the reason and the
  position of the call.
- Functions of the C library and the C++ standard library are not target
  functions. The existing rules for them do not change.
- An object that the hook built cannot be given to the target to modify: a
  hook-local `std::string` passed through a non-const reference or pointer, or
  a non-const method called on a hook-local copy. The target would allocate
  with its own runtime into an object that the hook's runtime frees. Passing
  it through a reference or pointer to `const` is allowed.

At run time, when the target function throws, the exception is caught on the
target side and the call throws an exception of the hook's own runtime
instead. A hook that does not catch it ends as a failed hook: its event is not
published and the original call continues. A hook can catch it with
`catch (...)`; the exception of the target is not available. A probe on the
target function, or on anything it calls, is not traced while the hook runs,
and a thread that the function creates does not inherit the context.

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

# Generate and inject from the native C++ REPL

Build and launch the example on Linux x86-64 from the repository root:

```bash
cmake -S . -B build -G Ninja -DTRACER_BUILD_REPL=ON
cmake --build build --target clang-tracer-repl clang-tracer-server clang_tracer_agent prepared-injectors-target
./examples/prepared-injectors/run-native.sh
```

The launcher prints the example directory and target PID, attaches automatically,
and presents the native `tracer[PID]>` prompt. Paste the commands below into that
prompt. Keep each `%` command on one line.

## 1. Generate the package from the header

```text
%probe prepare "prepared_demo::compute(int)" --include demo/calculator.hpp --output prepared
```

The launcher links the build's compilation database into the current directory.
`%probe prepare` uses `./compile_commands.json` by default. Pass `--compdb FILE`
to select another database.

Preparation uses the flags from the first database entry whose source file is
in the first header's directory. Pass `--translation-unit FILE` to choose a
different entry or when the database has no source in that directory.

The selector chooses the integer overload declared in the header. Clang parses
the headers with the selected flags and records the symbol and ABI. This can
run before the target is built. Pass `--module ELF` during preparation if you
want to pin the package to a particular build.

The newly created package contains:

```text
prepared/
  prepared.pb
  generated/
    adapter.hpp
    adapter.cpp
```

The native runtime consumes `prepared.pb`. The generated adapter files expose a
typed function pointer separately from the user-owned interceptor. They are not
inserted into `interceptor.cpp`, and you do not need to compile them manually.
Use `--metadata-only` to save just `prepared.pb`. The output directory must
be new each time.

## 2. Validate against the running application

```text
%probe validate "prepared_demo::compute(int)" --callback demo/interceptor.cpp
```

Expect `success: true` and `prepared_used: true`. Validation checks the prepared
contract against the mapped executable and compiles the callback without
installing a hook.

The entire example interceptor is:

```cpp
template <> class Interceptor<int(int)> {
public:
  long long pre(const Call &call) { return call.arg<int>(0) * 100 + 17; }
  long long post(const Call &call) { return call.result<int>(); }
};
```

The callback observes arguments and results. The target still computes
`value * 3 + 7`; for input `1`, its result is `10`.

## 3. Inject and collect

```text
%probe inject "prepared_demo::compute(int)" --callback demo/interceptor.cpp --print-arguments --print-result
%collect --count 4 --timeout 15 --phase POST --as captured
```

Injection reports `installed probe=...`; retain that number for removal. Expect
a collection containing four POST events.
Then enter ordinary C++ at the same native prompt:

```cpp
auto captured = tracer.batch("captured");
captured.size()
tracer.print(captured.at(0))
for (const auto& e : captured) { std::cout << e.argument_as<int>(0) << " -> " << e.result_as<int>() << '\n'; }
```

For background capture, type `tracer.events_a` and press Tab to complete
`tracer.events_async`. After injecting, start capture with
`auto live = tracer.events_async(session);`. Run `live.snapshot().size()`
to see how many events are retained and `live.stop();` to stop capture.

## 4. Restore and exit

```text
%probe remove PROBE_ID
%quit
```

Replace `PROBE_ID` with the number reported during injection. Removal reports
`restored=true`. Quitting detaches and stops the server owned by
this console; the launcher stops its example application. Generated files and
`target.log` remain in the directory printed at startup. Use `%quit` to leave the
native prompt.

## Equivalent native C++ API

In a fresh example session, these calls perform preparation and injection without
the `%probe` commands:

```cpp
tracer.prepare("prepared_demo::compute(int)", "demo/calculator.hpp", "prepared_cpp");
auto hook = tracer.inject(session, "prepared_demo::compute(int)", "demo/interceptor.cpp");
auto events = tracer.collect(session, 4, 15);
tracer.print(events);
```

Use `%quit` to restore and detach after this variant, or `%probe remove` with the
probe ID reported during injection.

## Modes and scope of this draft

- `require`: use the prepared package or report why it could not be bound.
- `prefer`: attempt prepared resolution, then fall back to DWARF if it fails.
- `off`: use the ordinary DWARF resolution path.

The REPL automatically tries the latest package it generated for the selector
and falls back to DWARF when it cannot use the package. Pass
`--prepared DIRECTORY` to select a package in another directory or from another
REPL session. Fallback happens during resolution, before the hook is installed.

Prepared packages support Linux x86-64 free functions and non-static,
non-virtual methods. Typed packages retain the declared C++ signature and
header layout. The declarations and build flags must describe the running
function's ABI. Injection binds the package to the symbol in the mapped ELF
image and checks the header types before compiling the callback. If multiple
modules define the symbol, pass `--module PATH` to select one.

To run the same workflow automatically through the native REPL:

```bash
./examples/prepared-injectors/run-native.sh --smoke
```

Expect `NATIVE_EVENT_COUNT=4`, `NATIVE_RESULTS=1`, and `detached session=...`.
This variant restores its hook by detaching through `tracer.detach(session)`.

Set `CLANG_TRACER_PREPARED_BUILD` to use a different CMake build directory.

## Typed method example

Build `prepared-methods-target` and launch with `--methods`. The method demo
target has no DWARF. The launcher prints the commands and full selector from
`methods/try.repl` and starts the native C++ REPL.

The method declaration and fields are in `methods/account.hpp`. The callback
in `methods/interceptor.cpp` specializes `Interceptor<Signature>` with the
method’s concrete argument and result types. Preparation and injection use the
same selector. The REPL finds the package registered by `%probe prepare`.

The event accessors return owned snapshots of supported arguments, results,
and receiver fields. `e.receiver_snapshot()` captures the declaring class
and its bases at method entry. PRE and POST events for the same call share that
entry snapshot. Fields at every access level use their short names when unique.
For hidden fields, use the qualified name, such as
`e.receiver_snapshot().field("prepared_demo::Audit::account_id").as<int>()`.

The method example exercises strings and string views, a vector by value,
optional values, a nested user record, a pair, a tuple, an array, a signed
bit-field, and fields inherited through private and protected bases. It also
passes owning and weak smart pointers and a variant as typed method arguments.
The inherited fields exercise a non-null and a null unique pointer, a shared
pointer, live and expired weak pointers, and variants holding duplicate types
and `std::monostate`.

An owning smart pointer snapshot exposes `address` and an owned copy under
`value` when it points to a supported object. A null pointer has address zero.
Array, opaque, and non-owning aliasing pointers are captured by address.
A weak pointer records `expired` without acquiring ownership. A variant records
its `index` and the matching `alternative_N`. For decodable alternatives,
`argument_as<std::variant<...>>()` reconstructs an owned variant. A valueless
variant retains its `std::variant_npos` index in the snapshot. Pointer values and
variants use the same bounded capture budget as the other fields.

Variadic and virtual methods, constructors and destructors, ambiguous repeated
non-virtual base subobjects, unions, and private reference members are outside
this example's automatic capture support. Unique pointers with custom deleters
and recursive pointee graphs also report unavailable snapshots.

Run `./examples/prepared-injectors/run-native.sh --methods --smoke` to check
the prepared binding and the field values through the native REPL.

## Typed custom event

Launch the example with `--state`. The launcher runs
`methods/setup-state.repl` in the native REPL to prepare the package, install
the probe, collect an event, and bind `reading`. Use the inspection commands
in `methods/try-state.repl` at the prompt. The example prepares the method
with `--metadata-only` and requires the package during injection.
Omit `--metadata-only` to keep the separate generated C++ adapter files
alongside the metadata.

The interceptor saves a copy of three typed arguments in a scoped
`prepared_demo::Reading` and publishes `account.adjusted` on return.
The public payload definition is in `methods/reading.hpp`. Preparation
registers both application headers for the REPL's automatic mapping import.

```text
%collect --name account.adjusted --count 1 --timeout 15 --as adjusted
auto event = tracer.batch("adjusted").at(0);
auto reading = event.state_as<prepared_demo::Reading>();
reading.parts.at(1)
reading.adjustment.delta
```

The label is `deposit`, the vector is `{2, 5}`, and the nested adjustment
has delta `4` and limit `30`. Use `event.state_snapshot()` for field access
without a C++ mapping. Both forms read owned capture-time data. A truncated
state reports an error.

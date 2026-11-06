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
The output directory must be new; use another name if you repeat generation.

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

This native draft supports Linux x86-64 free functions with supported primitive
value arguments/results: signed 32/64-bit integers, unsigned 64-bit integers,
float, double, and void results. It excludes methods, object values, pointers,
references, bool, and variadic functions. Native inferred type names are
normalized to the scalar ABI. The source declarations and build flags must
describe the running function's ABI. A source-only package can be reused across
builds with the same symbol and ABI. Injection locates the symbol in the target's
loaded modules and compiles the callback. If more than one module defines the
symbol, pass `--module PATH` when injecting to select the intended module.

To run the same workflow automatically through the native REPL:

```bash
./examples/prepared-injectors/run-native.sh --smoke
```

Expect `NATIVE_EVENT_COUNT=4`, `NATIVE_RESULTS=1`, and `detached session=...`.
This variant restores its hook by detaching through `tracer.detach(session)`.

Set `CLANG_TRACER_PREPARED_BUILD` to use a different CMake build directory.

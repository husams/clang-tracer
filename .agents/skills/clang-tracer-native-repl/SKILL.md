---
name: clang-tracer-native-repl
description: Run Clang Tracer through clang-tracer-repl for native C++ tracing, prepared injection, typed event inspection, replay, and reusable .repl scripts. Use for interactive demos and tracing automation unless the user requests the Python client.
---

# Clang Tracer native REPL

Use `clang-tracer-repl` for both interactive tracing and automation. The prompt
is `tracer[PID]>`. Use the [Python client skill](../clang-tracer-python/SKILL.md)
when the task explicitly calls for Python. For supported syntax and examples,
use the repository revision that matches the running binaries. If the current
checkout lacks the linked demos or manual, open them from the matching branch
or release.

## Run a native script

Start the target on the tracing host and use its actual PID. On Linux x86-64
with the server and agent installed, the REPL can start its own private server.
Use `--endpoint ADDRESS` for an existing server and use its host's PID. A set
`CLANG_TRACER_ENDPOINT` also selects an existing server.

Save the same input you would enter at the prompt in a `.repl` file, with one
command or C++ input per line. For a repeatable run with an attached target:

```bash
clang-tracer-repl --attach "$pid" --fail-fast < capture.repl
```

This example `capture.repl` uses the prepared free-function demo. For batch
use, start the demo target and set up a fresh working directory with `demo`
pointing to `examples/prepared-injectors` and `compile_commands.json` from the
build of that same source tree. For a manual run, paste the lines at the
prompt from the demo launcher, which sets up those paths and attaches.

```text
%probe prepare "prepared_demo::compute(int)" --include demo/calculator.hpp --output prepared
%probe inject "prepared_demo::compute(int)" --callback demo/interceptor.cpp
%collect --count 4 --timeout 15 --phase POST --as captured
auto captured = tracer.batch("captured");
if (captured.size() != 4) throw std::runtime_error("expected four POST events");
for (const auto& e : captured) { std::cout << e.argument_as<int>(0) << " -> " << e.result_as<int>() << '\n'; }
%quit
```

For an interactive session with scripted setup, pass `--startup setup.repl`
and omit `%quit` from the file. Startup runs in the same interpreter and stops
on a failed input. Shell variables belong in the launch command. Use actual
paths or paths relative to the REPL working directory inside the file.
`%cpp load` reads C++ source, while stdin and `--startup` accept both C++ and
percent commands.

For a manual walkthrough, give shell setup and each native command separately.
When verifying a run, check that injection reports an installed probe and
that collection received the expected events before indexing it. Report the
actual working directory and generated paths printed for that run. `%quit` or
EOF detaches sessions owned by this REPL and stops its owned server.

## Choose the capture path

- **Source-prepared injection.** Use `%probe prepare` with the selector, headers,
  and a new output directory. It can run before the target is built.
  `./compile_commands.json` is auto-detected and the first entry whose source
  is beside the first header supplies its flags. Override the database or
  translation unit when needed. Inject by the same selector in that session
  to reuse the package automatically with DWARF fallback. Pass
  `--prepared DIRECTORY` when selecting a package from a different session.
  Use `--prepared-mode require` when a test must prove the prepared path.
  `prepared.pb` is the runtime package. Generated adapter files are separate
  from the interceptor. Use `--metadata-only` to omit those files.
- **Typed methods.** Use the declared signature in `Interceptor<Signature>`.
  Read `argument_snapshot()` and `result_snapshot()` for field inspection,
  or `argument_as<T>()` and `result_as<T>()` for supported C++ values.
  `receiver_snapshot()` captures entry-time fields across access levels and
  base classes. Qualify hidden field names. Owning pointer snapshots can expose
  an owned pointee under `value`, weak pointers expose `expired`, and variants
  expose `index` and the active `alternative_N`.
- **Typed custom events.** Capture the payload with
  `call.context().init_scope<Scope>(payload)` and publish it with
  `call.context().event("name")`. Collect the event with
  `%collect --name name --count 1 --timeout 15 --as captured`. Then read
  `state_snapshot()` for fields or `state_as<Payload>()` for a supported
  C++ value. Supply the payload declaration as an additional header during
  preparation. Injection attempts to import mappings from the standalone
  adapter generated in that session. If a mapping is unavailable, inspect the
  import diagnostic before using an explicit `%cpp import`. See the demo for supported types and
  capture limits.
- **Live capture and replay.** Use `tracer.events(session, count, seconds)` for
  a synchronous range or `tracer.events_async(session, count, seconds)` for a
  background range. Inspect `snapshot()` and `error()` and call `stop()` when
  done. Use `%record` for a file and `%events open` to replay it into a named
  collection for native C++ inspection.

## Reuse the runnable examples

From the repository root on Linux x86-64 with the native demo targets built,
`./examples/prepared-injectors/run-native.sh` opens the free-function demo.
Add `--methods` for typed arguments and inherited fields, or `--state` for
typed custom events. Add `--smoke` to run a bounded native check. The state
launcher runs its setup before handing over the prompt.

- [Prepared demo and manual commands](../../../examples/prepared-injectors/README.md)
- [Typed method commands](../../../examples/prepared-injectors/methods/try.repl)
- [Typed state setup](../../../examples/prepared-injectors/methods/setup-state.repl)
  and [inspection commands](../../../examples/prepared-injectors/methods/try-state.repl)
- [Native REPL build, commands, and C++ APIs](../../../docs/native-repl.md)
- [Shared-context scope and event APIs](../../../docs/shared-context.md)

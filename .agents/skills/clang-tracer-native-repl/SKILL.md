---
name: clang-tracer-native-repl
description: Use clang-tracer-repl for native C++ tracing, prepared probes, typed event inspection, SQLite recording and offline replay, and reusable .repl scripts. Use for native tracing and replay unless the user requests the Python client.
---

# Clang Tracer native REPL

Use `clang-tracer-repl` for both interactive tracing and automation. The prompt
is `tracer[PID]>`. Use the [Python client skill](../clang-tracer-python/SKILL.md)
when the task explicitly calls for Python. For supported syntax and examples,
use the repository revision that matches the running binaries. If the current
checkout lacks the linked demos or manual, read them from the matching branch
or release. For a current `c++17` build, start with
`git show origin/c++17:docs/native-repl.md`.

At the prompt, use `%help` to discover the installed commands and `%help probe inject`
or `%help events open` for options. `%help api` lists the C++ API, and topics such
as `%help api tracer.record`, `%help api tracer.replay`, and `%help api Event`
show examples and signatures. Use `%type tracer.events_async` to inspect a C++
declaration or its overloads without evaluation, and `%complete "tracer."` for
member completion. Use `NO_COLOR=1` for plain terminal output.

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
  is beside the first header supplies its flags. Use `--compdb FILE` or
  `--translation-unit FILE` to override that selection. Inject by the same
  selector in that session to reuse the package with DWARF fallback. Pass
  `--prepared DIRECTORY` when selecting a package from a different session.
  Use `--prepared-mode require` when a test must prove the prepared path.
  Use `%probe validate` with the same selector, callback and prepared options
  to check the callback before installing it. `prepared.pb` embeds the standalone
  adapter for a typed package. `--metadata-only` omits the separate adapter files
  and keeps the embedded adapter. Injection can use the package without the
  original application headers.
- **Virtual methods and templates.** Select the concrete virtual implementation
  reached by dispatch, or use explicit template arguments and a full parameter
  list, such as `prepared_concrete::Handler::convert<int>(int) const`. Injection
  requires a callable symbol with a patchable entry in the running target. For
  sample selectors and supported layouts, read the prepared demo below.
- **Typed methods.** Use the declared signature in `Interceptor<Signature>`.
  Read `argument_snapshot()` and `result_snapshot()` for field inspection,
  or `argument_as<T>()` and `result_as<T>()` for supported C++ values.
  Use snapshots for records with application-defined copy, move, or destruction.
  For a raw pointer, inspect its address or capture an owned field payload in
  the callback while the pointee is valid.
  `receiver_snapshot()` captures entry-time fields across access levels and
  base classes. Qualify hidden field names. Owning pointer snapshots can expose
  an owned pointee under `value`, weak pointers expose `expired`, and variants
  expose `index` and the active `alternative_N`.
- **Typed custom events.** Capture the payload with
  `call.context().init_scope<Scope>(payload)` and publish it with
  `call.context().event("name")`. Collect the event with
  `%collect --name name --count 1 --timeout 15 --as captured`. Then read
  `state_snapshot()` for fields or `state_as<Payload>()` for a supported
  C++ value. Supply the payload declaration through a repeated `--include`
  during preparation. Injection in the same REPL attempts to import mappings
  from its generated adapter. If a mapping is unavailable, inspect the import
  diagnostic and use `state_snapshot()` or `%cpp import` as appropriate.
- **Live capture.** Use `tracer.events(session, count, seconds)` for
  a synchronous range or `tracer.events_async(session, count, seconds)` for a
  background range. Inspect `snapshot()` and `error()` and call `stop()` when
  done. For named collections, use `%events stats NAME` to check for loss and
  incomplete captures.

## Record and replay events

To record the prepared `--state` demo's custom event while attached:

```text
%record capture.db --sqlite --name account.adjusted --count 12 --timeout 15
```

Use `--sqlite` explicitly. Plain `%record` writes framed Protobuf to a new
file. SQLite creates or appends to the database, deduplicates by server
instance and sequence, and saves available standalone C++ type bundles for
prepared custom events. Use a fresh path for an independent capture. Bound
scripted recordings with a count and timeout and check the reported outcome.

For native C++ scripts, use `tracer.record` while attached. It uses the selected
session unless `options.session` or `options.all_sessions` changes the scope.

```cpp
tracer_repl::RecordOptions options;
options.names = {"account.adjusted"};
options.count = 12;
options.timeout_seconds = 15;
auto saved = tracer.record("capture.db", options);
if (!saved.complete) throw std::runtime_error("incomplete SQLite recording");
```

Inspect `saved.received`, `saved.cursor`, `saved.cancelled`, and
`saved.timed_out` when capture stops early. Committed events remain available.

In a fresh REPL without an attachment, use `%events open capture.db --as saved`
to open SQLite or pass a framed Protobuf file. SQLite databases written by
`clang-tracer events PID --sqlite FILE` or `clang-tracer drain PID --sqlite FILE`
also work. Use an unused collection name, then inspect it with
`%events list saved`, `%events show saved --index 0`, or
`tracer.batch("saved")`. Use `tracer.replay` to filter before loading, especially
when the recording exceeds the collection's memory budget:

```cpp
tracer_repl::EventQuery query;
query.names = {"account.adjusted"};
query.count = 1;
auto matches = tracer.replay("capture.db", query);
if (!matches.size()) throw std::runtime_error("missing account.adjusted");
auto reading = matches.at(0).state_as<prepared_demo::Reading>();
```

Enter each statement as a separate REPL input. Compatible recorded types and
their mappings are loaded before the next input. Use `state_snapshot()` for
older recordings or when a recorded declaration cannot be loaded. Replay reads
SQLite without changing it and works after the target and server have exited.

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

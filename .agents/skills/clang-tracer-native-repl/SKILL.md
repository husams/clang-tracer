---
name: clang-tracer-native-repl
description: Use clang-tracer-repl for native C++ tracing, prebuilt object lifetimes and field writes, method snapshots, prepared probes, bulk tracing, observer reload, read-only inspection, typed SQLite replay, and reusable .repl scripts. Use unless the user requests the Python client.
---

# Clang Tracer native REPL

Use `clang-tracer-repl` for both interactive tracing and automation. The prompt
is `tracer[PID]>`. Use the [Python client skill](../clang-tracer-python/SKILL.md)
when the task explicitly calls for Python. For supported syntax and examples,
use the repository revision that matches the running binaries. If the current
checkout lacks the linked demos or manual, read them from the matching branch
or release. For the current checkout, start with `docs/native-repl.md` and the linked examples.

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

## Browse, inspect, and trace a local target

Use a matching REPL, server, and agent on Linux x86-64. Use `%processes --filter TEXT`
to identify the target, `%attach PID --start-time TICKS` to guard against PID
reuse, and `%sessions` to check the target's JIT runtime profile. Browse the
selected target with:

```text
%threads
%modules --details
%symbols --filter Calculator --addresses
%symbols --address 0xADDRESS --mapping TOKEN
```

Use the runtime address and mapping token from the same session. Refresh the
lookup after the target's mappings change. To compile and link a named draft
before installing a probe, use `%interceptor load observer.cpp --as observer`
then `%interceptor validate observer --target "Calculator::multiply(int, int) const"`.

Preview bulk selections and check the reported match status and retained slots
before applying the manifest:

```text
%trace preview --module "libapp*" --include "app::*" --exclude "app::detail::*" --save trace.textproto
%trace apply --manifest trace.textproto
%trace remove BATCH_ID
```

Use `--exact "full C++ signature"` to select a specific overload. Apply resolves
the manifest again and installs typed PRE/POST observers. Every match must be
DWARF-supported and available for a new probe. Use `--callback FILE` to compile
a custom observer for each match. Keep the reported batch ID to remove its probes
or retry any incomplete restoration.

Use `%probe reload PROBE_ID --callback observer-v2.cpp` to update an active
observer while keeping its binding and deployment options. A rejected callback
leaves the existing observer active. Calls already in flight finish under the
old ID. Use the new ID for subsequent filtering or removal, and check `%probes`
if the reload reply was interrupted. Retired generations still count toward
the target's retained-probe limit.

Use `%events stats NAME` on live or replayed collections for per-probe counts,
duration histograms, and caller edges. Caller edges require captured entry stacks.

For read-only inspection, get a memory snapshot token from `%memory maps`:

```text
%memory maps
%memory read 0xADDRESS --mapping TOKEN --count 32
%memory scan 0xADDRESS --mapping TOKEN --count 4096 --pattern "48 8? ?? ff" --limit 16
%probe entry "Calculator::multiply(int, int) const" --mapping TOKEN
```

Use addresses from readable ranges for reads and scans. Reads support up to
64 KiB or one little-endian scalar via `--type`. Scans support up to 1 MiB and
wildcard nibbles. Refresh the snapshot after a mapping or permission change.
An entry preview verifies live bytes against the mapped ELF, disassembles up
to 64 bytes, and reports the copy-only patch window. For an explicit entry,
pass the exact module path, a link-time ELF virtual address with `--elf-address`,
and `--abi linux-x86-64-sysv`. See `%help memory scan` or `%help probe entry`
for the installed syntax.

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
  C++ value. Supply the payload declaration as an additional header during
  preparation. Injection in the same REPL attempts to import mappings
  from its generated adapter. If a mapping is unavailable, inspect the import
  diagnostic and use `state_snapshot()` or `%cpp import` as appropriate.
- **Live capture.** Use `tracer.events(session, count, seconds)` for
  a synchronous range or `tracer.events_async(session, count, seconds)` for a
  background range. Inspect `snapshot()` and `error()` and call `stop()` when
  done. For named collections, use `%events stats NAME` to check for loss and
  incomplete captures.

### Prepare with multiple headers in C++

On builds with the multi-header overload, pass a braced list or a
`std::vector<std::string>` as the second argument to `tracer.prepare()`.
Put the function's declaration header first, followed by headers for callback
types. Use a new output directory.

```cpp
tracer.prepare("demo::compute(int)", {"demo.hpp", "state.hpp"}, "prepared");
```

For the equivalent `%probe prepare` command, repeat `--include` for each header
in the same order. To override the compile flags in C++, pass the compilation
database and translation unit after the output directory. Use
`%help api tracer.prepare` for an example and `%type tracer.prepare` for the
installed overloads.

## Trace object lifetimes and state

Use the matching Linux x86-64 or AArch64 REPL, server, and agent with the existing ELF and matching DWARF. For individual writes, use `%object prepare TYPE --module ELF --field FIELD --code SELECTOR --backend code-cache --output DIRECTORY`. Choose `ptrace` for diagnostic single-stepping. The backend is saved in the plan. For net method changes, prepare with `--on METHOD`, the original `--include` and compile command, and a verified `--synchronization` policy.

Attach, run `%object preview --plan DIRECTORY`, and check the binding and admitted sites. Use `%object start --plan DIRECTORY` to arm the watch. Writes capture values by default. Add `--no-values` to a write watch to record completed stores, including same-value stores, without samples. Record while the workload runs with `%record capture.db --sqlite --name object.trace --probe WATCH_ID --count N --timeout SECONDS`. Check `%object status WATCH_ID`, then stop with `%object stop WATCH_ID` and confirm restoration before `%detach`. Open the recording with `%events open capture.db --as saved` and inspect it with `%events list saved` or `%events show saved --index 0`.

For an exact writer line, select a typed leaf of a nested record with `--field`. Follow keyed vector entries with `--vector` and `--element-key`, ordered or unordered map entries with `--map` and `--map-key`, or verified pointer paths with `--pointee`. Supply each storage-changing boundary with `--mutation` and verify rebinding and writer coverage in replay. For structural changes, strings, variants and registered payloads, use method snapshots with `--field` and a verified `--key PATH=MEMBER` where applicable. Added and removed entries carry the captured value tree in `snapshotAfter` or `snapshotBefore`.

Report gaps and drops as partial evidence. Lifecycle and ownership require verified boundaries, and unadmitted writers, unsafe reads, generated helper windows, and external writers can limit what the trace establishes. Follow the [runnable example and coverage limits in the native REPL guide](../../../docs/native-repl.md#trace-object-lifetimes-and-state-in-a-prebuilt-binary) for detailed commands and collection, union, and shared-library cases.

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

### Run the deployed JIT demo on linux-dev

Run the automated capture and offline replay, or open an attached prompt
from a terminal for the interactive walkthrough.

```sh
ssh linux-dev clang-tracer-jit-demo
ssh -t linux-dev clang-tracer-jit-demo --interactive
```

The launcher uses its bundled target, REPL, server, and agent. The automated
run prints `JIT_EXAMPLE_PASSED=3` and the path to `capture.db`. In interactive
mode, use the walkthrough to copy the current mapping token, entry address,
probe ID, and batch ID from the prompt. Use `%quit` to end the demo.

- [JIT walkthrough for bulk tracing, memory inspection, and reload](../../../examples/native-repl-jit/README.md)
- [Capture script](../../../examples/native-repl-jit/capture.repl) and [offline replay script](../../../examples/native-repl-jit/replay.repl)
- Run the typed-value demos with `ssh linux-dev clang-tracer-values-demo all`.
  [Typed-value walkthrough](../../../examples/native-repl-values/README.md)

### Run the prepared injector demos

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

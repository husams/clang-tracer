# Clang Tracer tutorials

Start with [the first probe](user.md#create-a-target-and-a-basic-probe). The labs below use that target and callback unless stated otherwise. Start a fresh REPL for each live lab or remove the previous probe before installing another on the same function. Replace example IDs, addresses, and tokens with the values shown by your session. Create recordings and manifests in a scratch directory.

## Inspect a process and a function entry

Attach to the running target, list its threads and modules, and find a verified function address. Paginated symbol listings use the next-page token with the same filter and page size.

```bash
%attach 12345
%threads
%modules --details
%symbols --filter "add(" --addresses --limit 20
%memory maps
%probe entry "add(int, int)" --mapping MEMORY_TOKEN
%memory read ENTRY_ADDRESS --mapping MEMORY_TOKEN --count 16
```

Use the runtime address from the entry preview. Copy the first two bytes from the hex dump into the scan pattern, replacing the second byte with `??` to match any byte.

```bash
%memory scan ENTRY_ADDRESS --mapping MEMORY_TOKEN --count 16 --pattern "55 ??" --limit 4
```

The scan matches `55 ??` only when the first byte is `55`. Change it to the first byte you observed. If a token expires after a mapping change, take a new snapshot. Use the module mapping token from `%modules --details` to reverse-lookup the entry PC with `%symbols --address ENTRY_ADDRESS --mapping MODULE_TOKEN`.

## Edit, validate, reload, and restore

Load the first lab's observer as a draft. Edit it in the REPL if desired, entering a complete replacement and ending with `.end`. Upload the validated draft.

```bash
%interceptor load hook.cpp --as demo
%interceptor show demo
%interceptor validate demo --target "add(int, int)"
%interceptor upload demo --target "add(int, int)" --print-result
%probes
```

Save a separate `updated.cpp` with the field name changed from `sum` to `new_sum`. Replace `PROBE_ID` with the ID shown by `%probes`, then reload it and capture another call.

```bash
%probe reload PROBE_ID --callback updated.cpp
%collect --count 2 --timeout 30 --as updated
%events show updated --index 1
%probes
```

The POST event now includes `new_sum`; the target still returns 42. Remove the active probe ID shown by `%probes` and detach.

## Trace a function template instantiation

Extend the first lab's `target.cpp` with this function above `main`. Inside the existing newline branch, add `std::printf("echo<int>(42) = %d\n", echo<int>(42));` before the flush. Rebuild and restart the target using the same commands as the first lab.

```cpp
template <class T>
__attribute__((noinline)) T echo(T value) {
  volatile T saved = value;
  return saved;
}
```

Save this observer as `echo-hook.cpp`. `tracer::ResolvedSignature` lets the same callback source specialize against the type of the selected instantiation. For `echo<int>`, writing `Interceptor<int(int)>` explicitly would select the same signature.

```cpp
template <>
class Interceptor<tracer::ResolvedSignature> {
public:
  void pre(const Call& call) {
    call.field("input", call.argument<0>());
  }

  void post(const Call& call) {
    call.field("output", call.result());
  }
};
```

Start a fresh REPL and attach using the new PID. Quote the complete selector, including the concrete template arguments and function parameters. With `--prepared-mode off`, the tracer recovers the signature from DWARF.

```bash
%attach 12345
%symbols --filter "echo<"
%probe validate "echo<int>(int)" --callback echo-hook.cpp --prepared-mode off
%probe inject "echo<int>(int)" --callback echo-hook.cpp --prepared-mode off --print-result
%collect --name "echo<int>(int)" --count 2 --timeout 30 --as echo_call
%events list echo_call
```

Press Enter in the target terminal while capture waits. The events show `input=42` in PRE and `output=42` in POST. Detach before exiting. For a member template, use the complete selector for the concrete method, including its class, template arguments, parameter list, and qualifiers.

## Preview and apply bulk tracing

Preview an exact selector, save the selection as a reusable manifest, then install it as a batch. Use repeated include or exclude patterns to broaden or narrow the selection on a larger target.

```bash
%trace preview --exact "add(int, int)" --save trace.textproto
%trace apply --manifest trace.textproto --callback hook.cpp
%collect --count 2 --timeout 30 --as bulk
%events stats bulk
%trace remove BATCH_ID
```

Trigger a call during collection. The summary should show one PRE/POST pair. To see a caller edge, have the callback call `capture_stack(8, false)` in PRE before applying the batch. The manifest can be previewed against another session using `%trace preview --manifest trace.textproto`.

## Capture and inspect complete native stacks

```bash
%probe inject "add(int, int)" --callback hook.cpp --stack-depth full --stack-application-only false --print-result
%collect --count 2 --timeout 30 --as fullstack
%events stack fullstack --index 0
%events stats fullstack
```

Trigger one call. The stack shows the entry and available caller frames, with native PC, function/module, and source location where recovered. Both phases describe the entry stack. `full` follows the supported unwind chain and can report a partial unwind. If a deep stack exceeds shared transport capacity, raise the managed server's `--context-event-bytes` and, if needed, `--context-transport-memory-bytes` before starting a new session. See the [stack lab sources](https://github.com/husams/clang-tracer/tree/main/examples/call-stacks) for application-only filtering and callback-side symbolization.

## Capture in the background and query in C++

Use the probe with `--print-result` from the stack lab. Start a background range, trigger two target calls, and examine a stable snapshot. The predicate and transformation run in the REPL.

```cpp
using namespace tracer_repl;
auto range = tracer.events_async(session, 4, 60);
```

```cpp
auto snapshot = range.snapshot();
auto sums = snapshot
    | filter([](const Event& event) { return event.phase() == "POST"; })
    | transform([](const Event& event) { return event.result_as<int>(); })
    | to_vector();
for (auto value : sums) std::cout << value << '\n';
range.stop();
```

Expect two values of 42 when both calls have finished. Try `first()` to get the first optional match, `take(1)` to limit iteration, or `enumerate()` to pair each retained event with an index. `%cpp load analysis.cpp` can run an analysis script and `%cpp reset` resets only the interpreter and its capture handles. See [event ranges and adaptors](../native-repl.md#event-ranges) for further examples.

## Record and replay offline

Keep the probe with `--print-result` active. For each recording command, trigger one new call while it waits, or read available history starting from the requested cursor. Use `--after` with the previous recording's last sequence to start a fresh window.

```bash
%record calls.pb --count 2 --timeout 30
%record calls.db --sqlite --count 2 --timeout 30
%detach
%quit
```

Start a fresh REPL and load the recordings. Offline startup can still launch a private server, but replay requires no target attachment.

```bash
%events open calls.pb --as framed
%events open calls.db --as saved
%events calls saved
%events export saved --format json --output calls.json
```

Use a typed query for POST events and read the recorded result.

```cpp
tracer_repl::EventQuery query;
query.names = {"add(int, int)"};
query.phase = "POST";
auto recorded = tracer.replay("calls.db", query);
recorded.at(0).result_as<int>()
```

Expect 42. In a live CLI session, use `clang-tracer events PID --sqlite copy.db --count 2` to copy events without consuming server history, or `clang-tracer drain PID --sqlite consumed.db` to persist and acknowledge the retained snapshot. See [storage and recovery](../event-storage.md) for loss counters, deduplication and failure behavior.

## Prepare a reusable probe

Move the declaration of the first lab's `add` to `add.hpp`, include it from `target.cpp`, and keep the function definition in `target.cpp`. If your build does not produce a database, create this example `compile_commands.json` in the scratch directory. Replace the directory and compiler path with those of your target.

```json
[
  {
    "directory": "/absolute/path/to/demo",
    "file": "target.cpp",
    "arguments": ["/usr/bin/c++", "-g", "-O0", "-fno-pie", "-c", "target.cpp"]
  }
]
```

Rebuild and restart the target. Generate a package with a new output directory and explicit source entry.

```bash
%probe prepare "add(int, int)" --include add.hpp --translation-unit target.cpp --module ./target --output prepared
%attach 12345
%probe validate "add(int, int)" --callback hook.cpp --prepared prepared --prepared-mode require
%probe inject "add(int, int)" --callback hook.cpp --prepared prepared --prepared-mode require --print-result
%collect --count 2 --timeout 30 --as prepared_call
%events show prepared_call --index 1
```

Trigger one call and check for 42. `require` verifies that injection uses the package. Multiple `--include` options can bring in callback payload declarations. `--cwd` runs the probe command relative to another project directory, and `--extra-arg` passes supplemental compiler flags. Use the offline preparer from the [command reference](commands.md#offline-probe-preparation) for the equivalent shell workflow. Run the [prepared method lab](https://github.com/husams/clang-tracer/tree/main/examples/prepared-injectors) to capture a receiver and replay a custom typed event.

## Decode objects, containers, and method receivers

Run the installed typed-values example. It starts a debug target and checks record, scalar, string, sequence, and associative-container decoding. The native runtime and Development example sources must be present. Substitute the install prefix and data directory if customized.

```bash
cd /usr/local/share/clang-tracer/examples/native-repl-values
./run.sh all
```

To map your own public record, set the local interpreter's compilation options before its first C++ input. Load the mapping and open the collection with the matching argument snapshot.

```bash
%cpp options compdb build/compile_commands.json --file src/target.cpp
%cpp generate model.hpp --type app::Order --output mappings.hpp
%cpp load mappings.hpp
```

```cpp
auto sample = tracer.batch("orders").at(0);
auto order = sample.argument_as<app::Order>(0);
```

Alternatively `%cpp import model.hpp --type app::Order` generates, caches, and loads a mapping in one step. Automatic mapping requires supported public members and an accessible default constructor. Use `EventCodec<T>` for construction through a factory, or `argument_snapshot(0).field("id")` to inspect fields without a mapping. For a method receiver, inspect `receiver_snapshot()` in the client and use `call.self()` in the agent. The [typed C++ argument example](https://github.com/husams/clang-tracer/tree/main/examples/order-arguments) covers references, pointers, headerless DWARF recovery, and opt-in application headers.

## Observe shared context and thread propagation

Run the [shared-context example](https://github.com/husams/clang-tracer/tree/main/examples/shared-context) from a configured source checkout with examples enabled. Start the server and target, then install the same observer on the outer and worker functions.

```bash
cmake --build build-rhel --target shared-context-target
export CLANG_TRACER_ENDPOINT=127.0.0.1:50051
clang-tracer-server --listen "$CLANG_TRACER_ENDPOINT" > server.log 2>&1 &
server_pid=$!
./build-rhel/shared-context-target > target.log 2>&1 &
target_pid=$!
# Wait for clang-tracer list to succeed before attaching.
clang-tracer attach "$target_pid"
callback="$PWD/examples/shared-context/interceptor.cpp"
clang-tracer trace "$target_pid" 'process_request(int)' --callback "$callback" --callback-print-arguments true
clang-tracer trace "$target_pid" 'calculate_total(int)' --callback "$callback" --callback-print-arguments true
clang-tracer events "$target_pid" --context-field request_id,calls,last_result --count 24 --timeout 20
clang-tracer detach "$target_pid"
kill "$target_pid" "$server_pid"
unset CLANG_TRACER_ENDPOINT
```

Look for `request.step` events sharing the same request ID. The worker inherits context through supported `pthread_create` propagation and reports twice the request ID. The outer call reports the worker result plus one. For an explicit scope or a durable producer/collector journal, follow [Shared context](../shared-context.md).

## Automate tracing with Python

Start the target from the first lab and a server listening on loopback. In the same directory as `hook.cpp`, run the [Python client example](developer.md#resolve-validate-install-and-restore) with the real PID. Trigger one target call when prompted. The script prints two events, removes the probe and detaches. Repeat with `watch_events` for a cursor-based stream or use `record_traces` to write a framed recording that the native REPL can replay.

```bash
clang-tracer-server --listen 127.0.0.1:50051
```

The [Python client walkthrough](../../python/README.md) includes launch, readiness and shutdown, console examples, and recording. Installed examples in `share/clang-tracer/examples/python` exercise the Python API end to end.

## Trace from the Python console or the native CLI

Use the running target and server from the previous lab. Run `clang-tracer-console --endpoint 127.0.0.1:50051`, then enter these commands. Trigger one call while collecting.

```bash
attach 12345
symbols --filter "add("
probe validate "add(int, int)" --callback hook.cpp
probe inject "add(int, int)" --callback hook.cpp --print-result
collect 2 --timeout 30
detach
exit
```

For explicit raw ABI mode, save this as `legacy.cpp` and attach the target again. `nm ./target` can confirm the linkage name for the example build.

```cpp
extern "C" long long trace_callback(const TraceContext& context) {
  context.log("legacy add entry");
  return context.arguments[0].signed_value + context.arguments[1].signed_value;
}
```

```bash
clang-tracer attach "$PID"
clang-tracer trace "$PID" --function _Z3addii --return int32 \
  --arg int32 --arg int32 --callback legacy.cpp
clang-tracer events "$PID" --count 1 --timeout 30
clang-tracer detach "$PID"
```

Expect one entry event with callback value 42. To see the diagnostic, start the target with `CLANG_TRACER_AGENT_LOG="$PWD/agent.log" ./target` before attaching. Stop the server and target when finished.

## Run the installed end-to-end JIT demo

The runtime ships the JIT example target when the REPL is enabled. Install the Development component for the runner and source files. Run the demo from the installed example directory.

```bash
cd /usr/local/share/clang-tracer/examples/native-repl-jit
./run.sh
./run.sh --interactive
```

The first run performs bulk trace setup, capture, SQLite recording, and offline C++ analysis. It reports `JIT_EXAMPLE_PASSED=3`. The interactive run lets you explore mapping inspection, function entry previews and observer reload. See the [example walkthrough](https://github.com/husams/clang-tracer/tree/main/examples/native-repl-jit).

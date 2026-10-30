# Capture native C++ call stacks

The target calls `worker_loop -> run_iteration -> library_dispatch ->
StackCalculator::multiply -> stack_add` on three threads. The last three
functions live in `libcall-stacks-calculator.so`. The executable uses optimized
callers with omitted frame pointers; the library also omits frame pointers.
There are no target instrumentation attributes or agent dependencies.

Stacks show application frames by default: four method frames and five
nested-function frames in this example. Standard templates in the executable,
runtime libraries and the tracer's JIT bridge are hidden from both
`Call::stack()` and CLI events. To see the complete native stack, pass
`--stack-application-only false` to `trace`, or call
`call.capture_stack(24, false)` inside a hook. An ordinary
`call.capture_stack(24)` inherits the probe's default-true option.
Use `stack.application_only`, `stack.hidden_frames` and `stack.captured_depth`
to inspect the view and raw unwind depth. The depth limit applies before
filtering, so hidden frames still count toward it; unwind completeness is
unchanged. Names in application-owned frames may remain unavailable.

To compare using the helper:

```sh
# Default application-only output from both native hooks and CLI:
STACK_METHOD_CALLBACK=examples/call-stacks/symbolized.cpp \
STACK_FUNCTION_CALLBACK=examples/call-stacks/symbolized.cpp \
STACK_FUNCTION_CAPTURE=callback \
bash examples/call-stacks/run.sh /tmp/stacks-application-only complete

# Same callbacks, full native/JIT/runtime stacks:
STACK_APPLICATION_ONLY=false \
STACK_METHOD_CALLBACK=examples/call-stacks/symbolized.cpp \
STACK_FUNCTION_CALLBACK=examples/call-stacks/symbolized.cpp \
STACK_FUNCTION_CAPTURE=callback \
bash examples/call-stacks/run.sh /tmp/stacks-full complete
```

See [actual filtered PRE/POST and CLI output](application-only-output.txt) and
`../../stack-capture-results/2026-10-30-application-only/report.md` (local recording).
`call-stacks-filter-target` is a separate demo with an application caller taking
`std::vector` arguments; that caller stays visible. Its standard thread-invoker
frames are still hidden.

From the project root on Linux x86_64, using the existing configured build:

```sh
cmake --build build -j4 --target clang-tracer clang-tracer-server \
  clang_tracer_agent call-stacks-target
bash examples/call-stacks/run.sh
```

The script uses ordinary `attach`, `trace`, `events` and `detach` commands. It
starts an independent target before attaching, arms both probes, then releases
its optional demonstration start gate with SIGUSR1. The normal target runs
immediately when started without `--wait`. It captures 24 events, detaches both
probes, verifies continued target calls and checks unchanged executable/library
hashes. Output and exact commands remain in the printed log directory.

To type the commands yourself:

```sh
run_dir=$(mktemp -d /tmp/clang-tracer-call-stacks.XXXXXX)
export CLANG_TRACER_ENDPOINT="unix://$run_dir/control.sock"
./build/call-stacks-target --wait > "$run_dir/target.log" &
PID=$!
./build/clang-tracer-server --agent "$PWD/build/libclang_tracer_agent.so" \
  --listen "$CLANG_TRACER_ENDPOINT" --timeout 30 > "$run_dir/server.log" 2>&1 &
SERVER=$!
# Wait until this succeeds:
./build/clang-tracer list
./build/clang-tracer attach "$PID"
./build/clang-tracer trace "$PID" 'StackCalculator::multiply(int, int)' \
  --callback examples/call-stacks/method.cpp --stack 24
./build/clang-tracer trace "$PID" 'stack_add(int, int)' \
  --callback examples/call-stacks/function.cpp
kill -USR1 "$PID"
./build/clang-tracer events "$PID" --count 24 --timeout 15
./build/clang-tracer detach "$PID"
tail -3 "$run_dir/target.log"
kill -TERM "$PID" "$SERVER"
wait "$PID" "$SERVER"
```

`method.cpp` reads `call.self()->bias` and `calls` in PRE and POST, and reads the
automatic snapshot through `call.stack()`. `function.cpp` independently requests
capture in both hooks with `call.capture_stack(24)` and records its frame count.
The callback sees bounded structured native addresses and frame kinds; Protobuf
events carry those frames plus LLVM symbolization. Each callback frame also
exposes `function`, `module`, `file` (immutable C strings) and `line`, resolved
before the hook reads `call.stack()`. Empty strings and line 0 mean unavailable.
POST uses the entry caller
stack, while member fields and the original result reflect the completed call.

## Measured overhead

The runnable `benchmark.cpp` / `benchmark.sh` compare unattached execution,
normal tracing without capture, and application-only stacks using the same
existing calculator library. Both traced modes use the no-output
`automatic.cpp` callback; `symbolized.cpp` would request capture even without
`--stack`. See the `../../stack-capture-results/2026-10-30-overhead/report.md` (local recording).

The `../../stack-capture-results/2026-10-30-optimized/report.md` (local recording)
used the identical benchmark: median no-stack tracing fell from 12.317 to 7.415 us
per call, and application-stack tracing from 339.749 to 125.678 us. No-stack
event drops fell from 7.66% to 5.31%; stack event drops remained zero. These
figures exclude text printing and describe the tiny nested-call workload.

The `../../stack-capture-results/2026-10-30-buffered/report.md` (local recording)
received all 57,600 benchmark events with zero transport loss. No-stack median
was 7.992 us/call and application-stack median 127.269 us/call. A bounded queue
lets the caller return while a background sender handles congestion; full queues
still count drops. `transport-pressure.sh` demonstrates completed calls while
the server is stopped, followed by resume, immediate detach and event validation.

The `../../stack-capture-results/2026-10-30-stack-fast/report.md` (local recording)
lowered the stack-enabled median from 127.269 to 95.210 us/call (25.2% less time),
with all 57,600 benchmark events received. Use `PRESSURE_STACK_DEPTH=24` with
`transport-pressure.sh` to exercise stack capture while its receiver is stopped.

The `../../stack-capture-results/2026-10-30-stack-profiled/report.md` (local recording)
measured 93.279 -> 89.162 us/call with application stacks, zero losses and
identical frame metadata. It retains fresh mapping reads while reusing bounded
procfs streams and decoded sparse CFI plans. A stopped receiver still allows
the 1,500-call batch to complete, followed by delivery of all 3,200 events.

The `../../stack-capture-results/2026-10-30-mapread/report.md` (local recording)
reduced read syscall entries from 5 to 2 while retaining complete fresh streams.
Fresh production medians were 92.094 -> 85.582 us/call, with all 57,600 events
received per run. Short/interrupted reads, long records and fallbacks passed.

The `../../stack-capture-results/2026-10-30-positioned-maps/report.md` (local recording)
found no additional end-to-end speedup and restored the original source/agent
exactly. Its interleaved current/candidate medians were 88.179 / 89.403 us/call,
with zero event losses; the experiment and all raw recordings remain available.

## Optional PRE arguments and POST return value

The same `symbolized.cpp` callback optionally prints the traced function's named
arguments before its PRE stack and its original result before its POST stack.
Both are **off by default**. Enable either independently:

```sh
./build/clang-tracer trace "$PID" 'stack_add(int, int)' \
  --callback examples/call-stacks/symbolized.cpp \
  --callback-print-arguments true --callback-print-result true
```

For a complete runnable demonstration from the configured Linux checkout:

```sh
STACK_PRINT_ARGUMENTS=true STACK_PRINT_RESULT=true \
STACK_METHOD_CALLBACK=examples/call-stacks/symbolized.cpp \
STACK_FUNCTION_CALLBACK=examples/call-stacks/symbolized.cpp \
STACK_FUNCTION_CAPTURE=callback \
bash examples/call-stacks/run.sh /tmp/stacks-with-values complete
```

Use a fresh directory. Omit `STACK_PRINT_ARGUMENTS`/`STACK_PRINT_RESULT` for
stack-only callback output, or set either to `false`. The helper translates
them to the corresponding `--callback-print-*` options for both probes.
The preferences do not change captured values in Protobuf or the existing CLI
event display. `call.print_options()` is the read-only preference view;
`argument_count()`, `argument_name(i)`, `argument_value(i)` and POST-only
`result_value()` provide generic typed snapshots. Existing compile-time typed
accessors remain available. Pointer and object values are rendered as addresses;
class contents require explicit supported typed field access.

See [complete actual PRE/POST output](optional-values-output.txt) and
`../../stack-capture-results/2026-10-30-optional-values/report.md` (local recording).

## Read names, modules and source lines inside both hooks

[symbolized.cpp](symbolized.cpp) prints every frame from PRE and POST. Run it
against the same three-thread nested example with both automatic and
callback-requested capture:

```sh
STACK_HIDE_SOURCES=1 \
STACK_METHOD_CALLBACK=examples/call-stacks/symbolized.cpp \
STACK_FUNCTION_CALLBACK=examples/call-stacks/symbolized.cpp \
STACK_FUNCTION_CAPTURE=callback \
bash examples/call-stacks/run.sh /tmp/stacks-symbolized complete
```

The method's `--stack 24` prepares metadata before PRE and retains it for POST;
the function callback requests capture in each hook without that flag. Both
then read `call.stack().frames[i].function`, `.module`, `.file`, and `.line`.
Callback-printed text is in the resulting `target.log`; normal Protobuf/CLI
events are in `events.log`. `../../stack-capture-results/2026-10-30-callback-symbols/report.md` (local recording)
compares every printed frame with the corresponding event. Strings belong to
the agent and should be used within the invocation. Metadata is resolved from
ELF/DWARF; source text does not need to be present. Missing debug names remain
empty. Full-stack mode marks JIT bridges explicitly with no invented source line.

For additional runnable cases:

```sh
STACK_HIDE_SOURCES=1 bash examples/call-stacks/run.sh /tmp/stacks-complete complete
bash examples/call-stacks/run.sh /tmp/stacks-two-frames truncated
bash examples/call-stacks/run.sh /tmp/stacks-missing-cfi partial
bash examples/call-stacks/run.sh /tmp/stacks-depth-64 maximum
bash examples/call-stacks/run.sh /tmp/stacks-disabled disabled
```

Use fresh directories. `partial` selects a real caller compiled without unwind
tables; the event retains that caller and reports exactly where CFI ends.
`truncated` requests two frames. `maximum` requests 64; these particular stacks
reach the thread root sooner. `disabled` omits capture. Every case checks the
CLI's rejection of depth 65 and detaches with live workers. The first command
temporarily hides only this example's target sources/header and the generated
compilation database, restoring them on exit; it supplies no includes.

The implementation needs supported `.eh_frame` rules and readable mapped ELF
images to unwind beyond a caller. Missing debug names do not make a recovered
stack partial. Source text is unnecessary. Signal frames, unavailable CFI
registers/expressions and changed images stop with an explicit reason. No inline
or removed functions are synthesized. In full-stack mode JIT crossings appear as
`JIT_BRIDGE`; `TARGET_ENTRY` is the selected logical entry, and `NATIVE` frames
are recovered return PCs. Depth bounds the native unwind before filtering.

See the recorded output and verification report under
`stack-capture-results/2026-10-30/` in the project root.

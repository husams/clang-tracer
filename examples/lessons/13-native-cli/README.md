# Lesson 13: trace from the native CLI

Guide section: [Trace from the native CLI](../../../docs/guide/tutorials.md#trace-from-the-native-cli).

| File | Purpose |
| --- | --- |
| `target.cpp` | The first lesson's target. |
| `legacy.cpp` | Entry-only `TraceContext` observer that returns the sum of both arguments. |
| `Makefile` | Builds `target` with `-rdynamic` so `add` is exported. |

Two things differ from the guide text, because the commands fail as written
with the installed build:

- The target is linked with `-rdynamic`. Without it, `--function _Z3addii`
  reports "target symbol was not found in the process".
- `legacy.cpp` omits the `context.log(...)` line. `TraceContext` has no `log`
  member, so the probe is rejected when it is present.

Terminal one:

```bash
clang-tracer-server --listen 127.0.0.1:50051
```

Terminal two:

```bash
make
nm ./target | grep addii
./target
```

Terminal three, in this directory, with `PID` set to the printed PID:

```bash
export CLANG_TRACER_ENDPOINT=127.0.0.1:50051
clang-tracer attach "$PID"
clang-tracer trace "$PID" --function _Z3addii --return int32 \
  --arg int32 --arg int32 --callback legacy.cpp
clang-tracer events "$PID" --count 1 --timeout 30
clang-tracer detach "$PID"
```

Press Enter in the target terminal while `events` waits.

Expected: one entry event with `args=[int32:20, int32:22] callback=42`. Stop
the server and the target with Ctrl+C when finished.

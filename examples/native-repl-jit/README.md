# Local JIT tracing in the native REPL

Run this on Linux x86-64 with a matching Clang Tracer REPL, server, and
agent that support bulk tracing and observer reload.

```sh
./run.sh
```

The runner starts the calculator example with DWARF. It previews
a manifest for `add(int, int)`, installs a typed bulk observer, prints live
call and caller statistics, and records 12 events to SQLite. It then stops
the target and opens the recording in a fresh REPL to check the original
results. The run prints `JIT_EXAMPLE_PASSED=3` and the recording path.

The installed runner uses the bundled target, REPL, and server. For a
source checkout, set `CXX` to a compiler that targets the server's runtime
and use `CLANG_TRACER_REPL` or `CLANG_TRACER_SERVER` to select the build.
The manifest and recording are stored in the printed demo directory.

## Inspect and reload interactively

Start with `./run.sh --interactive`. The runner attaches to its calculator.
Browse the selected process and locate the runtime entry for `add`.

```text
%threads
%modules --details
%symbols --filter "add(int, int)" --addresses
%memory maps
%probe entry "add(int, int)" --mapping MEMORY_TOKEN
%memory read ENTRY_ADDRESS --mapping MEMORY_TOKEN --count 16
%memory scan ENTRY_ADDRESS --mapping MEMORY_TOKEN --count 16 --pattern "55 48 89 ??" --limit 4
```

Replace `MEMORY_TOKEN` with the token printed by `%memory maps` and
`ENTRY_ADDRESS` with the verified runtime address from the preview. The scan
pattern illustrates wildcard nibbles. Use bytes from the preview if your
compiler emits a different prologue.

Validate the callback, preview and apply the batch, then reload its active
probe. Copy the probe and batch IDs from the output.

```text
%interceptor load observer.cpp --as observer
%interceptor validate observer --target "add(int, int)"
%trace preview --exact "add(int, int)" --save trace.textproto
%trace apply --manifest trace.textproto --callback observer.cpp
%probes
%probe reload PROBE_ID --callback observer.cpp
%collect --name "add(int, int)" --count 8 --timeout 15 --as after_reload
%events stats after_reload
%trace remove BATCH_ID
%quit
```

The reload reports a new probe ID. Both generations belong to the batch.
The stack captured by the observer establishes the caller edges in the stats.

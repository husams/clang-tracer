# Lesson 25: Discover object lifetimes and compare changes

Each Enter creates a `Counter(10)`, adds 2, and destroys it. Trace two
iterations to see why two objects can have the same address. Then run a
method snapshot to compare the evidence from a store and a call boundary.

## Build and start the target

Use Linux x86-64, `make`, a C++17 compiler, Python 3, and
`clang-tracer-repl` with its matching server and agent. Run these commands
from this lesson directory in the target terminal. `make` keeps the
constructor, destructor and method out of line and writes
`build/compile_commands.json` for the source exercises.

```sh
make
sha256sum build/target > build/target.sha256
./build/target
```

Copy the printed PID. Start `clang-tracer-repl --fail-fast` from this lesson
directory in a second terminal. Replace `PID` in the commands below.

## Capture a lifetime and its field writes

```text
%attach PID
%object start --type lesson::Counter --field value --code "lesson::Counter::add(int)" --backend code-cache
```

The REPL uses the attached process's main executable and matching debug
information to bind the field, writer, and lifecycle sites. Copy the watch
ID from `%object start`. If it reports partial coverage through verified
direct writer call sites, the exercise uses that direct call to `Counter::add`.

```text
%record ./build/writes.db --sqlite --name object.trace --probe WATCH_ID --count 8 --timeout 30
```

While `%record` waits, press Enter twice in the target terminal. It should
print `value=12` twice with the same address. The recording should report
`received=8` and `complete=true`. Stop the watch before switching to snapshots.

```text
%object status WATCH_ID
%object stop WATCH_ID
%detach
%events open ./build/writes.db --as writes
%events list writes
%events show writes --index 1
```

Stop should report `active=no` and `restoration_pending=no`. Replay should
show the following sequence. Addresses and instruction PCs depend on the
build and run.

```text
CREATED        Counter#1 generation=1 value 10
FIELD_CHANGED  Counter#1 generation=1 value 10 -> 12
DESTROY_BEGIN  Counter#1 generation=1
TEARDOWN_END   Counter#1 generation=1
CREATED        Counter#2 generation=2 value 10
FIELD_CHANGED  Counter#2 generation=2 value 10 -> 12
DESTROY_BEGIN  Counter#2 generation=2
TEARDOWN_END   Counter#2 generation=2
```

The tracer discovered both lifetimes from constructor and destructor
boundaries. Check `location_kind: STORE` on the write event. It identifies
the instruction in `counter.cpp` that changed the field.

## Find the writer from source

When the application source and its compilation database are available,
prepare a reusable write plan. The preparer can find `Counter::add(int)` by
parsing `counter.cpp` with its original compile command. Use the same idle
target after stopping the prior watch.

```text
%attach PID
%object prepare lesson::Counter --module ./build/target --field value --discover-writers --compdb ./build/compile_commands.json --translation-unit ./counter.cpp --backend code-cache --output ./build/source-write-plan
%object preview --plan ./build/source-write-plan
%object start --plan ./build/source-write-plan
```

Look for `source_writer=` identifying `Counter::add` in the plan report.
Source discovery selects direct writer candidates. The preparer takes the
built ELF explicitly so it can verify their compiled code and layout, and
the saved plan supplies that binding to preview and start. Use the same
recording steps with a fresh database, then stop and detach.

## Prepare a net method change from source

Use the header and the original compile command to prepare a typed snapshot
plan for `Counter::add`. The `thread-confined` policy describes this fixture
because the receiver is accessed only by the thread handling input.

```text
%attach PID
%object prepare lesson::Counter --module ./build/target --field value --on "lesson::Counter::add(int)" --include ./counter.hpp --compdb ./build/compile_commands.json --translation-unit ./counter.cpp --synchronization thread-confined --output ./build/snapshot-plan
%object preview --plan ./build/snapshot-plan
%object start --plan ./build/snapshot-plan
```

Use the new `WATCH_ID` below. Press Enter twice while recording.

```text
%record ./build/snapshots.db --sqlite --name object.trace --probe WATCH_ID --count 2 --timeout 30
%object status WATCH_ID
%object stop WATCH_ID
%detach
%events open ./build/snapshots.db --as snapshots
%events list snapshots
%events show snapshots --index 0
```

Expect two `SNAPSHOT_DELTA` events from 10 to 12. Inspect the call ID and
`location_kind: CALL_BOUNDARY`. These samples identify the method call and
receiver address. Constructor-based watches provide the lifetime generations
shown in the previous exercise.

## Try a same-value store

Attach again and add `--no-values` to the live write watch.

```text
%attach PID
%object start --type lesson::Counter --field value --code "lesson::Counter::add(int)" --backend code-cache --no-values
```

Record four events into a fresh database and enter `0` once in the target
terminal. It prints `value=10`. Replay should show `FIELD_WRITTEN` for the
completed store even though the value stayed the same. If you repeat the
snapshot exercise with `0`, it yields no delta. A short `%record` timeout
lets you observe that.

Stop the watch and detach, enter `q` in the target terminal, and run
`sha256sum -c build/target.sha256` from this directory. The executable's
checksum should still match. Replay works after the target exits. Choose
fresh database and collection names for new captures.

These exercises use `%object` observations. User-supplied C++ callbacks for
lifetime and change events are part of the custom object probe proposal.
See the [native REPL guide](../../../docs/native-repl.md#trace-object-lifetimes-and-state-in-a-prebuilt-binary)
for more watch options.

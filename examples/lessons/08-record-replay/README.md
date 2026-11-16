# Lesson 8: record and replay offline

Guide section: [Record and replay offline](../../../docs/guide/tutorials.md#record-and-replay-offline).

You will write events to disk while the target runs, then analyze them later
with no target and no attachment.

```bash
make
./target
```

```bash
%attach 12345
%probe inject "add(int, int)" --callback hook.cpp --print-result
%record calls.pb --count 2 --timeout 30
%record calls.db --sqlite --count 2 --timeout 30
%detach
%quit
```

Press Enter in the target terminal each time a `%record` waits. `calls.pb` is
a framed Protobuf file; `calls.db` is SQLite and also keeps prepared C++ types
so a replay can decode typed values. Recording refuses to overwrite an existing file.

Start a fresh `clang-tracer-repl`. It needs no target.

```bash
%events open calls.pb --as framed
%events open calls.db --as saved
%events calls saved
%events export saved --format json --output calls.json
```

Then query the recording with a typed query (`%cpp load query.cpp`). Expected: `42`.

For a live process, `clang-tracer events PID --sqlite copy.db --count 2` copies
events without consuming the server's history, and `clang-tracer drain PID
--sqlite consumed.db` persists and acknowledges the retained snapshot. See
[storage and recovery](../../../docs/event-storage.md) for loss counters,
deduplication and failure behavior.

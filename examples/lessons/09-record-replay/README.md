# Lesson 9: record and replay offline

Guide section: [Record and replay offline](../../../docs/guide/tutorials.md#record-and-replay-offline).

Files: the first lesson's `target.cpp` and `hook.cpp`. Run `make`, start
`./target`, then start `clang-tracer-repl` in this directory.

```bash
%attach 12345
%probe inject "add(int, int)" --callback hook.cpp --print-result
%record calls.pb --count 2 --timeout 30
%record calls.db --sqlite --count 2 --timeout 30
%detach
%quit
```

Press Enter in the target terminal once for each `%record` while it waits.

Start a fresh `clang-tracer-repl`. The target is no longer needed.

```bash
%events open calls.pb --as framed
%events open calls.db --as saved
%events calls saved
%events export saved --format json --output calls.json
```

```cpp
tracer_repl::EventQuery query;
query.names = {"add(int, int)"};
query.phase = "POST";
auto recorded = tracer.replay("calls.db", query);
recorded.at(0).result_as<int>()
```

Expected: both recordings report `received=2 complete=true`, and the replay
query prints `(int) 42`.

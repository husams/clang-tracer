# Lesson 8: capture in the background and query in C++

Guide section: [Capture in the background and query in C++](../../../docs/guide/tutorials.md#capture-in-the-background-and-query-in-c).

| File | Purpose |
| --- | --- |
| `target.cpp`, `hook.cpp` | The first lesson's target and observer. |
| `analysis.cpp` | The snapshot query from the guide, for `%cpp load analysis.cpp`. |

Run `make`, start `./target`, then start `clang-tracer-repl` in this directory.

```bash
%attach 12345
%probe inject "add(int, int)" --callback hook.cpp --print-result
```

```cpp
using namespace tracer_repl;
auto range = tracer.events_async(session, 4, 60);
```

Press Enter twice in the target terminal, then query the snapshot. Either
type the block below or run `%cpp load analysis.cpp`, which contains it.

```cpp
auto snapshot = range.snapshot();
auto sums = snapshot
    | filter([](const Event& event) { return event.phase() == "POST"; })
    | transform([](const Event& event) { return event.result_as<int>(); })
    | to_vector();
for (auto value : sums) std::cout << value << '\n';
range.stop();
```

```bash
%detach
```

Expected: two lines of `42`. Do not run `%collect` while the background range
is active; it reports that collection storage is full until the range is
stopped.

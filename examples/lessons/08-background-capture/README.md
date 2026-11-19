# Lesson 8: capture in the background and query in C++

Guide section: [Capture in the background and query in C++](../../../docs/guide/tutorials.md#capture-in-the-background-and-query-in-c).

| File | Purpose |
| --- | --- |
| `target.cpp`, `hook.cpp` | The first lesson's target and observer. |
| `analysis.cpp` | The streaming query from the guide, for `%cpp load analysis.cpp`. |

Run `make`, start `./target`, then start `clang-tracer-repl` in this directory.

```bash
%attach 12345
%probe inject "add(int, int)" --callback hook.cpp --print-result
```

```cpp
using namespace tracer_repl;
auto range = tracer.events_async(session, 4, 60);
```

The range returns immediately and collects in the background. Either type
the block below or run `%cpp load analysis.cpp`, which contains it. The loop
waits for events, so press Enter twice in the target terminal while it runs.

```cpp
auto sums = range
    | filter([](const Event& event) { return event.phase() == "POST"; })
    | transform([](const Event& event) { return event.result_as<int>(); });
for (auto value : sums) std::cout << value << '\n';
range.stop();
```

```bash
%detach
```

Expected: two lines of `42`, each printed as its call returns. The loop ends
after four events or 60 seconds; Ctrl+C ends it early. Do not run `%collect` while the background range
is active; it reports that collection storage is full until the range is
stopped.

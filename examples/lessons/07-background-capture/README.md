# Lesson 7: capture in the background and query in C++

Guide section: [Capture in the background and query in C++](../../../docs/guide/tutorials.md#capture-in-the-background-and-query-in-c).

`%collect` blocks until it has its events. A background range keeps capturing
while you keep working, and you query it with ordinary C++ in the REPL.

```bash
make
./target
```

```bash
%attach 12345
%probe inject "add(int, int)" --callback hook.cpp --print-result
```

Start a background range for up to 4 events or 60 seconds:

```cpp
using namespace tracer_repl;
auto range = tracer.events_async(session, 4, 60);
```

Press Enter twice in the target terminal. Then take a stable snapshot and
transform it. The predicate and the transformation run in the REPL, not in the target.

```cpp
auto snapshot = range.snapshot();
auto sums = snapshot
    | filter([](const Event& event) { return event.phase() == "POST"; })
    | transform([](const Event& event) { return event.result_as<int>(); })
    | to_vector();
for (auto value : sums) std::cout << value << '\n';
range.stop();
```

Expected: two lines of `42`. `analysis.cpp` holds the same second block, so you
can run `%cpp load analysis.cpp` instead of typing it.

Try these adaptors on the same snapshot: `first()` returns the first optional
match, `take(1)` limits iteration, and `enumerate()` pairs each event with its
index. `%cpp reset` resets the interpreter and its capture handles; attached
sessions and probes stay in place. See [event ranges](../../../docs/native-repl.md#event-ranges).

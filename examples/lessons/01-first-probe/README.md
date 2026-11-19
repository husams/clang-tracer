# Lesson 1: first probe

Guide section: [Create a target and a basic probe](../../../docs/guide/user.md#create-a-target-and-a-basic-probe).

| File | Purpose |
| --- | --- |
| `target.cpp` | Prints its PID and calls `add(20, 22)` each time you press Enter. |
| `hook.cpp` | Observer that records `left` and `right` in PRE and `sum` in POST. |

Terminal one:

```bash
make
./target
```

Terminal two, in this directory, start `clang-tracer-repl` and enter:

```bash
%attach 12345
%symbols --filter "add("
%probe resolve "add(int, int)"
%probe validate "add(int, int)" --callback hook.cpp
%probe inject "add(int, int)" --callback hook.cpp --print-arguments --print-result
%collect --name "add(int, int)" --count 2 --timeout 30 --as first
```

Press Enter in terminal one while `%collect` waits, then inspect the pair:

```bash
%events list first
%events show first --index 1
%events calls first
```

```cpp
auto first_call = tracer.batch("first");
first_call.at(1).result_as<int>()
```

```bash
%detach
%quit
```

Expected: the PRE event shows `left=20 right=22`, the POST event shows
`sum=42 result=42`, and the C++ expression prints `(int) 42`.

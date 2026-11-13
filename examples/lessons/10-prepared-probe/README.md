# Lesson 10: prepare a reusable probe

Guide section: [Prepare a reusable probe](../../../docs/guide/tutorials.md#prepare-a-reusable-probe).

| File | Purpose |
| --- | --- |
| `add.hpp` | Declaration of `add`, moved out of `target.cpp`. |
| `target.cpp` | The first lesson's target, including `add.hpp`. |
| `hook.cpp` | The first lesson's observer. |
| `Makefile` | Builds `target` and writes `compile_commands.json` for this directory. |

Run `make`, start `./target`, then start `clang-tracer-repl` in this directory.

```bash
%probe prepare "add(int, int)" --include add.hpp --translation-unit target.cpp --module ./target --output prepared
%attach 12345
%probe validate "add(int, int)" --callback hook.cpp --prepared prepared --prepared-mode require
%probe inject "add(int, int)" --callback hook.cpp --prepared prepared --prepared-mode require --print-result
%collect --count 2 --timeout 30 --as prepared_call
%events show prepared_call --index 1
%detach
```

Press Enter in the target terminal while `%collect` waits.

Expected: validation reports `prepared_used: true` and the POST event shows
`sum=42 result=42`. Run `make clean` before repeating the lesson so
`prepared` can be created again.

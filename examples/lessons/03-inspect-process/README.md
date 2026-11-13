# Lesson 3: inspect a process and a function entry

Guide section: [Inspect a process and a function entry](../../../docs/guide/tutorials.md#inspect-a-process-and-a-function-entry).

Files: the first lesson's `target.cpp` and `hook.cpp`. Run `make`, start
`./target`, then start `clang-tracer-repl` in this directory.

```bash
%attach 12345
%threads
%modules --details
%symbols --filter "add(" --addresses --limit 20
%memory maps
%probe entry "add(int, int)" --mapping MEMORY_TOKEN
%memory read ENTRY_ADDRESS --mapping MEMORY_TOKEN --count 16
%memory scan ENTRY_ADDRESS --mapping MEMORY_TOKEN --count 16 --pattern "55 ??" --limit 4
%symbols --address ENTRY_ADDRESS --mapping MODULE_TOKEN
%detach
```

Where the placeholders come from:

| Placeholder | Take it from |
| --- | --- |
| `MODULE_TOKEN` | The `mapping=...` line printed by `%modules --details`. |
| `MEMORY_TOKEN` | The `memory mapping=...:mem:...` line printed by `%memory maps`. |
| `ENTRY_ADDRESS` | The `address=0x...` value printed by `%symbols --addresses`. |

Expected: `%probe entry` disassembles `add` starting with `55  pushq %rbp`,
the hex dump starts with `55 48 89 e5`, and the scan reports a match at the
entry address.

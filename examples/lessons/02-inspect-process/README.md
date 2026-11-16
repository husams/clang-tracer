# Lesson 2: inspect a process and a function entry

Guide section: [Inspect a process and a function entry](../../../docs/guide/tutorials.md#inspect-a-process-and-a-function-entry).

You will look at a process without changing it: its threads, modules, symbols,
memory map, and the machine code at the entry of `add`. No probe is installed.

`target.cpp` is the lesson 1 target. Build and start it, then attach.

```bash
make
./target
```

```bash
%attach 12345
%threads
%modules --details
%symbols --filter "add(" --addresses --limit 20
%memory maps
```

Copy the token on the first line of `%memory maps` (it looks like
`SESSION:mem:HASH`) and use it below as `MEMORY_TOKEN`.

```bash
%probe entry "add(int, int)" --mapping MEMORY_TOKEN
%memory read ENTRY_ADDRESS --mapping MEMORY_TOKEN --count 16
%memory scan ENTRY_ADDRESS --mapping MEMORY_TOKEN --count 16 --pattern "55 ??" --limit 4
```

`ENTRY_ADDRESS` is the runtime address `%probe entry` prints. Replace the first
byte of the pattern with the first byte `%memory read` showed; `??` matches any byte.

To go from an address back to a function, take the `mapping=` token that
`%symbols --filter "add(" --addresses` printed (it looks like `SESSION:HASH`) and run:

```bash
%symbols --address ENTRY_ADDRESS --mapping MODULE_TOKEN
%detach
```

If a token is rejected, the mappings changed: run `%memory maps` or `%modules --details` again.
For a longer symbol list, pass the next-page token with the same filter and page size.

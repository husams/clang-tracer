# Lesson 22: catch single writes inside a string and a variant

Reference: [Follow logical collection entries during writes](../../../docs/native-repl.md#follow-logical-collection-entries-during-writes)
(REPL walkthrough, the paragraphs on `std::string`, `std::variant` and
`std::any` storage; the user guide summarises it under
[Trace changes to C++ objects](../../../docs/guide/user.md#trace-changes-to-c-objects)).

Lesson 18 compared a whole string before and after a method call. That tells
you the string changed, not which instruction changed it, and it misses a
write that is undone before the method returns. A write watch on the string
itself reports each store into its characters, with the source line and the
bytes before and after. The same works for the value held by a
`std::variant`.

This lesson needs a Linux x86-64 build of the tools that includes `%object`.

| File | Purpose |
| --- | --- |
| `label.hpp` | `shop::Label` with private members `text_` (a string) and `code_` (a `std::variant<int, std::string>`). |
| `label.cpp` | `Label::mark`, which overwrites two characters of `text_`, and `Label::bump`, which increments the `int` in `code_`. |
| `target.cpp` | Calls `mark` and `bump` on one label each time you press Enter. |
| `Makefile` | Builds `target` with `-O2 -g`. |

Run `make`, start `./target`, then start `clang-tracer-repl` in this directory.

## Part 1: prepare both watches

```bash
%object prepare shop::Label --module ./target --field text_ --code "shop::Label::mark()" --first-seen --synchronization thread-confined --backend code-cache --output ./text-watch
%object prepare shop::Label --module ./target --field code_ --code "shop::Label::bump()" --first-seen --synchronization thread-confined --backend code-cache --output ./code-watch
```

- `--field text_` selects the string itself, not a member inside it. The
  members are private; the watch reads the layout from the debug
  information.
- `--first-seen` accepts a label that was built before you attached. The
  watch identifies it for the length of one call.
- `--synchronization thread-confined` states that only one thread uses the
  label. Writes to strings, variants and `std::any` require a declared
  policy.

Expected: the first prepare reports `encoding="std::string storage"` and
three `store` sites. Two are the character writes. The third is the write to
`symbol_` at the end of `mark`; it does not touch `text_`, so it produces no
event. The second prepare reports `encoding="std::variant storage"` and one
`store` site.

## Part 2: record the writes to the string

```bash
%attach 12345
%object preview --plan ./text-watch
%object start --plan ./text-watch
%record text.db --sqlite --name object.trace --count 4 --timeout 30
```

Press Enter once in the target terminal while `%record` waits. Expected:
`received=4` and `complete=true`. The target prints `label is "*al*"`.

```bash
%object stop
%detach
%events open text.db --as text
%events list text
```

Expected: four events.

```text
0 FIRST_SEEN       shop::Label#1 ... function="shop::Label::mark()"
1 FIELD_CHANGED    shop::Label::text_ STRING_DATA offset=0 size=1 bytes=73 -> bytes=2a ... label.cpp:12
2 FIELD_CHANGED    shop::Label::text_ STRING_DATA offset=3 size=1 bytes=65 -> bytes=2a ... label.cpp:13
3 OBSERVATION_ENDED shop::Label#1
```

## Part 3: record the write to the variant

A second `%record` in the same session would save the string events again,
because they are still in the session's buffer. A new session starts with
an empty buffer, so attach again for the second watch:

```bash
%attach 12345
%object start --plan ./code-watch
%record code.db --sqlite --name object.trace --count 3 --timeout 30
```

Press Enter once. Expected: `received=3` and `complete=true`.

```bash
%object stop
%detach
%events open code.db --as code
%events list code
```

Expected: three events. The number depends on how often you pressed Enter
before; here the code went from 8 to 9.

```text
0 FIRST_SEEN       shop::Label#1 ...
1 FIELD_CHANGED    shop::Label::code_ REPRESENTATION offset=0 size=4 bytes=08000000 -> bytes=09000000 ... function="shop::Label::bump()" label.cpp:19
2 OBSERVATION_ENDED shop::Label#1
```

## How to read the events

- `STRING_DATA` means the store went into the string's characters. `offset`
  is the position in the text and `size` is the number of bytes written.
  `bytes=73 -> bytes=2a` is `s` replaced by `*` in hexadecimal, and
  `bytes=65` is `e`.
- `REPRESENTATION` means the store went into the object's own bytes. For the
  variant these hold the active value: `08000000` is the `int` 8 in
  little-endian byte order.
- Each store is a separate event with its own source line. A method snapshot
  of `mark` would report one change, from `sale` to `*al*`.
- `%events show text --index 1` prints the same write as `old_bytes`,
  `new_bytes` and a `storage_write` block with the region, address and size.
  In a JSON export the bytes are base64 text under `oldBytes` and
  `newBytes`.

## Limits you will meet

- The writer must store with a plain move. If you change `mark` to
  `chars[0] ^= 0x20`, the compiler emits an exclusive-or on memory, and
  prepare stops with `no supported field-width store`.
- A writer that takes a `char` argument, such as `mark(char)`, prepares but
  cannot be started in a live process. Start reports
  `unsupported debug parameter ABI`.
- The watch reports stores made by the function named in `--code`. A write
  made inside `std::string::append` or another library call is not covered.
  Use a method snapshot from lesson 18 for those.

## Things to try

- Add `--no-values` to the first prepare, with a different `--output`. The
  two stores are then reported as `FIELD_WRITTEN` with their offset and
  size, and without the bytes.
- Construct the label in `target.cpp` with a text longer than 15 characters.
  The string then keeps its characters on the heap. Run `make clean`, `make`
  and repeat Part 2: the events look the same, and `destination` is now a
  heap address.

Run `make clean` before repeating the lesson so `text-watch` and `code-watch`
can be created again.

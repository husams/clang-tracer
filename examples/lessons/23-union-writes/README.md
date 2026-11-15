# Lesson 23: trace writes to a union and to its tag

Reference: [Trace individual writes to a nested union](../../../docs/native-repl.md#trace-individual-writes-to-a-nested-union)
(REPL walkthrough; the user guide summarises it under
[Trace changes to C++ objects](../../../docs/guide/user.md#trace-changes-to-c-objects)).

The members of a union share the same bytes. A store into those bytes does
not say which member was written. Programs record that in a separate field,
often called a tag. This lesson watches a union nested inside a record,
tells the watch which tag value means which member, and gets typed values
back. It also shows the event for a store that changes the tag.

This lesson needs a Linux x86-64 build of the tools that includes `%object`.

| File | Purpose |
| --- | --- |
| `discount.hpp` | `shop::Discount` with `state.kind` (the tag) and `state.value`, a union of `amount` (a `long long`) and `rate` (a `double`). |
| `discount.cpp` | `Discount::raise`, which writes `amount`, and `Discount::to_rate`, which changes the tag and writes `rate`. |
| `target.cpp` | Calls `raise`, then `to_rate`, then restores the amount each time you press Enter. |
| `Makefile` | Builds `target` with `-O2 -g`. |

Run `make`, start `./target`, then start `clang-tracer-repl` in this directory.

## Part 1: prepare both watches

```bash
%object prepare shop::Discount --module ./target --field state.value --code "shop::Discount::raise()" --union-tag state.kind --union-case 1=amount --union-case 2=rate --first-seen --synchronization thread-confined --backend code-cache --output ./raise-watch
%object prepare shop::Discount --module ./target --field state.value --code "shop::Discount::to_rate()" --union-tag state.kind --union-case 1=amount --union-case 2=rate --first-seen --synchronization thread-confined --backend code-cache --output ./switch-watch
```

- `--field state.value` is the path to the union itself, not to one of its
  members.
- `--union-tag state.kind` is the path to the field that says which member
  is in use.
- `--union-case 1=amount` and `--union-case 2=rate` give the member for each
  tag value. The numbers are the values of `Kind::amount` and `Kind::rate`.
- `--synchronization thread-confined` states that only one thread uses the
  discount. The watch reads the tag and the union together and relies on
  this to do so safely.

Expected: both report `encoding="union storage"`. The first plan lists one
`store` site. The second lists two: the write to the tag and the write to
`rate`.

## Part 2: record a write to the member in use

```bash
%attach 12345
%object preview --plan ./raise-watch
%object start --plan ./raise-watch
%record raise.db --sqlite --name object.trace --count 3 --timeout 30
```

Press Enter once in the target terminal while `%record` waits. Expected:
`received=3` and `complete=true`.

```bash
%object stop
%detach
%events open raise.db --as raise
%events list raise
```

Expected: three events.

```text
0 FIRST_SEEN       shop::Discount#1 ... function="shop::Discount::raise()"
1 FIELD_CHANGED    shop::Discount::state.value REPRESENTATION offset=0 size=8 tag=1 amount=100 -> tag=1 amount=250 bytes=6400000000000000 -> bytes=fa00000000000000 ... discount.cpp:6
2 OBSERVATION_ENDED shop::Discount#1
```

The tag is 1 before and after the store, so the watch knows the bytes are an
`amount` and prints `100` and `250`. The raw bytes are still there.

## Part 3: record a change of the tag

As in lesson 22, attach again so the second recording starts with an empty
buffer:

```bash
%attach 12345
%object start --plan ./switch-watch
%record switch.db --sqlite --name object.trace --count 4 --timeout 30
```

Press Enter once. Expected: `received=4` and `complete=true`.

```bash
%object stop
%detach
%events open switch.db --as switch
%events list switch
```

Expected: four events. The amount in the old bytes depends on how often you
pressed Enter before.

```text
0 FIRST_SEEN       shop::Discount#1 ... function="shop::Discount::to_rate()"
1 FIELD_CHANGED    shop::Discount::state.value UNION_TAG offset=0 size=4 tag=1 amount= -> tag=2 rate= bytes=01000000 -> bytes=02000000 ... discount.cpp:11
2 FIELD_CHANGED    shop::Discount::state.value REPRESENTATION offset=0 size=8 tag=2 rate= -> tag=2 rate=0.25 bytes=2602000000000000 -> bytes=000000000000d03f ... discount.cpp:12
3 OBSERVATION_ENDED shop::Discount#1
```

## How to read the events

- `UNION_TAG` is the store to `state.kind`. It reports the switch from
  `amount` to `rate` and carries no value, because the union's bytes did not
  change in that instruction.
- `REPRESENTATION` is a store into the union's own bytes.
- In event 2 the value before the arrow is empty. The bytes there were
  written as an `amount` and the tag now says `rate`, so the watch does not
  decode them as a `double`. The value after the arrow, `0.25`, was written
  under the new tag and is decoded.
- `%events show switch --index 1` prints the same event with
  `old_union_member`, `new_union_member` and a `storage_write` block that
  holds `before_discriminator` and `after_discriminator`.

## Things to try

- Prepare `to_rate` again without `--union-tag` and both `--union-case`
  options, with a different `--output`. The watch then follows the bytes
  only. You get three events instead of four: the store to `rate` appears
  with its bytes and the note `untagged union storage`, with no member name
  and no typed value, and the store to the tag is not reported.
- Leave out `--union-case 2=rate`. The tag value 2 is then unknown to the
  watch. Check what event 2 shows for the value after the arrow.

## A limit you will meet

The writer must store with a plain move. `state.value.amount += 50` compiles
to an add on memory, and prepare stops with `no supported field-width store`.
`raise` computes the new amount first for that reason.

Run `make clean` before repeating the lesson so `raise-watch` and
`switch-watch` can be created again.

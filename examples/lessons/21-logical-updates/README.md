# Lesson 21: see an entry move, disappear and come back

Reference: [Follow logical collection entries during writes](../../../docs/native-repl.md#follow-logical-collection-entries-during-writes)
(REPL walkthrough; the user guide summarises it under
[Trace changes to C++ objects](../../../docs/guide/user.md#trace-changes-to-c-objects)).

Lessons 19 and 20 reported stores: instructions that write the watched
field. Some changes to an entry are not stores to its field at all. The line
with `id == 12` can move to another index, be erased, or be inserted again
with a new value. `--mode logical-writes` reports those changes at the
functions that make them, next to the ordinary stores.

This lesson needs a Linux x86-64 build of the tools that includes `%object`.

| File | Purpose |
| --- | --- |
| `cart.hpp` | `shop::Cart` with a vector of `Line{id, quantity}`. |
| `cart.cpp` | `move_to_front`, `drop_front` and `add_line`, which each change the vector in one way, and `Cart::rebuild`, which calls all three and then writes line 12. |
| `target.cpp` | Calls `rebuild` on one cart each time you press Enter. |
| `Makefile` | Builds `target` with `-O2 -g`. |

Run `make`, start `./target`, then start `clang-tracer-repl` in this directory.

## Part 1: prepare the watch

```bash
%object prepare shop::Cart --module ./target --mode logical-writes --field quantity --vector lines --element-key id=12 --code "shop::Cart::rebuild()" --synchronization thread-confined --mutation "shop::move_to_front" --mutation "shop::drop_front" --mutation "shop::add_line" --output ./updates-watch
%attach 12345
%object preview --plan ./updates-watch
%object start --plan ./updates-watch
```

- `--mode logical-writes` asks for a `LOGICAL_UPDATE` event whenever a
  selected mutation changes the entry.
- `--mutation` is repeated, once for each function that changes the shape of
  the vector. Each one must take the vector as its first argument.
- `--synchronization thread-confined` states that only one thread uses the
  cart. The watch reads the vector before and after each mutation and relies
  on this to do so safely. Logical writes require it.

Expected: prepare lists a `writer`, three `container mutation` sites, and one
`store` site. Preview reports `binding=same-build`.

## Part 2: record one rebuild

```bash
%record updates.db --sqlite --name object.trace --count 11 --timeout 30
```

Press Enter once in the target terminal while `%record` waits. Expected:
`received=11` and `complete=true`.

```bash
%object stop
%detach
%events open updates.db --as saved
%events list saved
```

Expected: eleven events. The cart starts as line 11, then line 12 with
quantity 3.

```text
 0 FIRST_SEEN       shop::Cart#1 ... function="shop::Cart::rebuild()"
 1 RANGE_BOUND      shop::Cart::lines[id=12].quantity 3
 2 RANGE_SUSPENDED  ... function="shop::move_to_front(...)"
 3 LOGICAL_UPDATE   shop::Cart::lines[id=12].quantity MOVED mutation="shop::move_to_front" before_index=1 after_index=0 before_size=2 after_size=2 3 -> 3 ... cart.cpp:6
 4 RANGE_BOUND      shop::Cart::lines[id=12].quantity 3 -> 3
 5 RANGE_SUSPENDED  ... function="shop::drop_front(...)"
 6 LOGICAL_UPDATE   shop::Cart::lines[id=12].quantity ERASED mutation="shop::drop_front" before_index=0 before_size=2 after_size=1 3 -> ... cart.cpp:10
 7 LOGICAL_UPDATE   shop::Cart::lines[id=12].quantity INSERTED mutation="shop::add_line" after_index=1 before_size=1 after_size=2 1 ... cart.cpp:15
 8 RANGE_BOUND      shop::Cart::lines[id=12].quantity 1
 9 FIELD_CHANGED    shop::Cart::lines[id=12].quantity 1 -> 5 ... function="shop::Cart::rebuild()" cart.cpp:23
10 OBSERVATION_ENDED shop::Cart#1
```

## How to read the events

- `MOVED`: line 12 went from index 1 to index 0. Its quantity stayed 3.
- `ERASED`: line 12 left the vector. The event keeps the last quantity seen
  before the call, and there is no value after it. No `RANGE_BOUND` follows,
  because there is nothing to watch.
- `INSERTED`: a line with `id == 12` appeared at index 1 with quantity 1. The
  watch binds it and follows it from here.
- `FIELD_CHANGED`: the one real store in `rebuild`. It is reported as before,
  with the line of the assignment.
- A `LOGICAL_UPDATE` names a function, not a store. Its source line is the
  mutation that was called, and it describes the state before the call and
  after the return. It cannot tell you which instruction inside
  `add_line` wrote the value 1.

## Part 3: read one update as JSON

```bash
%events export saved --format json --output updates.json
```

Open `updates.json` and find the fourth event. Expected, inside its
`objectObservation`:

```text
"locationKind": "CALL_BOUNDARY",
"logicalUpdate": {
  "change": "MOVED",
  "beforeIndex": "1",
  "afterIndex": "0",
  "beforeSize": "2",
  "afterSize": "2",
  "mutation": "shop::move_to_front"
}
```

The store in event 9 has `"locationKind": "STORE"` instead. Use that field to
tell the two kinds of evidence apart in a script.

## Things to try

Use a different `--output` for each new plan. In both cases fewer than eleven
events arrive, so `%record --count 11` waits for its timeout.

- Prepare a plan without `--mode logical-writes` and without
  `--synchronization`. You still get the bind and suspend events and the
  store, but no `LOGICAL_UPDATE`. The erase shows up only as a
  `COVERAGE_GAP` with the note `logical vector element key is unavailable`.
- Keep `--mode logical-writes` and leave out `--mutation "shop::drop_front"`.
  Prepare still succeeds. The `MOVED` update is reported, but the erase and
  the insert are not. In their place is a `COVERAGE_GAP` with the note
  `collection range changed outside a verified mutation return`. A logical
  trace is complete only when every function that changes the vector is
  declared, so treat this gap as a missing `--mutation`.

Run `make clean` before repeating the lesson so `updates-watch` can be created
again.

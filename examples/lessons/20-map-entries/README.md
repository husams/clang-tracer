# Lesson 20: follow a nested field in a hash map entry

Reference: [Follow logical collection entries during writes](../../../docs/native-repl.md#follow-logical-collection-entries-during-writes)
(REPL walkthrough; the user guide summarises it under
[Trace changes to C++ objects](../../../docs/guide/user.md#trace-changes-to-c-objects)).

Lesson 19 followed an `int` in a vector element. A write watch can also
follow a field that sits deeper: inside a nested record, inside the value of
a `std::map` or `std::unordered_map` entry, and with a type other than
`int`. This lesson watches `pricing.unit`, a `double`, in the entry with key
`12` of a `std::unordered_map<int, Item>`.

This lesson needs a Linux x86-64 build of the tools that includes `%object`.

| File | Purpose |
| --- | --- |
| `stock.hpp` | `shop::Stock` with `items`, a hash map from an item number to `Item{pricing{unit}}`. |
| `stock.cpp` | `shop::rehash`, which rebuilds the bucket table, and `Stock::reprice`, which writes the price of item 12, rehashes, and writes it again. |
| `target.cpp` | Calls `reprice` on one stock each time you press Enter. |
| `Makefile` | Builds `target` with `-O2 -g`. |

Run `make`, start `./target`, then start `clang-tracer-repl` in this directory.

## Part 1: prepare the watch

```bash
%object prepare shop::Stock --module ./target --field pricing.unit --map items --map-key 12 --code "shop::Stock::reprice()" --mutation _ZN4shop6rehashERSt13unordered_mapIiNS_4ItemESt4hashIiESt8equal_toIiESaISt4pairIKiS1_EEE --output ./price-watch
%attach 12345
%object preview --plan ./price-watch
%object start --plan ./price-watch --stack-depth 4
```

- `--map items --map-key 12` selects the entry of `items` whose key is 12.
  Quote a negative key, as in `--map-key "-7"`.
- `--field pricing.unit` is a path inside the mapped value. Each step is a
  member name.
- `--mutation` is the function that can change the map's storage. It must
  take the map as its first argument. The name here is the linker symbol of
  `shop::rehash`; list it with `nm target | grep rehash`. The short name
  `shop::rehash` does not work for this function, because the compiler also
  emitted a `shop::rehash(...) [clone .cold]` part and the short name matches
  both.

Expected: prepare reports `encoding="floating point (8 bytes)"` and lists a
`writer`, a `container mutation`, and two `store` sites, one for each write
in `reprice`. Preview reports `binding=same-build`.

## Part 2: record one reprice

```bash
%record prices.db --sqlite --name object.trace --count 7 --timeout 30
```

Press Enter once in the target terminal while `%record` waits. Expected:
`received=7` and `complete=true`.

```bash
%object stop
%detach
%events open prices.db --as saved
%events list saved
```

Expected: seven events. The addresses differ on your machine; the pattern
does not.

```text
0 FIRST_SEEN       shop::Stock#1 ... function="shop::Stock::reprice()"
1 RANGE_BOUND      shop::Stock::items[12].pricing.unit destination=0x...3f10
2 FIELD_CHANGED    shop::Stock::items[12].pricing.unit destination=0x...3f10 ... stock.cpp:10
3 RANGE_SUSPENDED  shop::Stock::items[12].pricing.unit ... function="shop::rehash(...)"
4 RANGE_BOUND      shop::Stock::items[12].pricing.unit destination=0x...3f10 ... function="shop::rehash(...)"
5 FIELD_CHANGED    shop::Stock::items[12].pricing.unit destination=0x...3f10 ... stock.cpp:12
6 OBSERVATION_ENDED shop::Stock#1
```

## Part 3: read the typed values

The list does not print a `double`. Open one event to see it:

```bash
%events show saved --index 2
```

Expected, near the end of the output:

```text
  location_kind: STORE
  storage_generation: 1
  snapshot_before {
    real_value: 9.5
  }
  snapshot_after {
    real_value: 10
  }
```

Event 5 shows `10` and `10.25` in the same way. The `stack` block above it
names `reprice` and its caller in `main`, because of `--stack-depth 4`.

## How to read the events

- `RANGE_BOUND`: the watch looked up key 12 in the hash table and now watches
  the address of that entry's `pricing.unit`.
- `RANGE_SUSPENDED`: `rehash` was entered. The table is being rebuilt, so the
  watch stops trusting what it knew. When `rehash` returns, it looks key 12
  up again in the new table.
- Compare `destination` in events 1 and 4. They are equal. A hash map keeps
  each entry in its own node, and a rehash only relinks the nodes. In
  lesson 19 the vector moved its elements, so the address changed. The watch
  checks again in both cases and does not assume either result.
- A rehash on its own produces no `FIELD_CHANGED`. Only the two stores in
  `reprice` do.

## Things to try

- Use a key that does not exist, such as `--map-key 99`, in a new plan with a
  different `--output`. The watch reports two `COVERAGE_GAP` events instead
  of a binding, and `%object status` counts them in `coverage_gaps`.
- Select item 11 with `--map-key 11`. `reprice` never writes that entry, so
  you get the bind and suspend events but no `FIELD_CHANGED`.

In both cases `%record --count 7` waits for its timeout, because fewer than
seven events arrive.

Run `make clean` before repeating the lesson so `price-watch` can be created
again.

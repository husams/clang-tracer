# Lesson 16: capture polymorphic items and their containers

Reference: [Strings and class/struct values](../../../docs/native-repl.md#strings-and-classstruct-values)
(REPL walkthrough; the user guide has no section for this lesson yet).

Earlier lessons captured integers and one flat record. Here the traced
functions work with `Item`, a handle whose data lives behind an interface
pointer. The factory picks one of three implementations, and each stores more
than the last: inherited fields, a vector, a map, and nested containers. The
lesson shows that a probe captures the implementation the pointer really
refers to, with everything it stores.

| File | Purpose |
| --- | --- |
| `item.hpp` | `Item`: the public handle. Its private `impl` points to an `ItemInterface`. |
| `item_interface.hpp` | `ItemInterface`: the abstract base with `description()` and `encode()`. |
| `item_v1.hpp` | `ItemV1`: a `label` string and a `revision` number. |
| `item_v2.hpp` | `ItemV2`: derives from `ItemV1` and adds `tags` (vector) and `attributes` (map). |
| `item_v3.hpp` | `ItemV3`: `parts` (vector of records), `stock` (map of vectors), `history` (list of vectors). |
| `item_factory.hpp` | `ItemFactory::create`: builds an `Item` and chooses the implementation by version name. |
| `target.cpp` | Creates and encodes a `v1`, `v2` and `v3` item each time you press Enter. |
| `report.cpp` | Prints every created item with its fields, for part 1. |
| `encoded.cpp` | Prints which implementation served each `encode` call, for part 2. |
| `Makefile` | Builds `target` and writes `compile_commands.json` for this directory. |

Run `make`, start `./target`, then start `clang-tracer-repl` in this directory.

## Part 1: the items the factory returns

`ItemFactory::create` returns an `Item*`. `%probe prepare` reads the headers,
so the probe knows `Item`, the interface, and the three implementations.
`--snapshot-return-pointee` copies the returned `Item` instead of keeping only
its address.

```bash
%attach 12345
%probe prepare "ItemFactory::create(const std::string&) const" --include item_factory.hpp --translation-unit target.cpp --output prepared-create
%probe inject "ItemFactory::create(const std::string&) const" --prepared prepared-create --prepared-mode require --snapshot-return-pointee
%collect --count 3 --timeout 30 --phase POST --as items
```

`%probe prepare` also makes the REPL import the item classes, which is why
`Item` and `ItemV3` are usable below. If you restart the REPL and inject from
the existing `prepared-create` directory without preparing again, import them
yourself first with `%cpp import item_factory.hpp`.

Press Enter once in the target terminal while `%collect` waits, then:

```bash
%events list items
%cpp load report.cpp
```

Expected: `%events list` shows three POST events whose `result` is only an
address. The report prints what was behind each address:

```text
item 1: ItemV1
  label=basic widget revision=4
item 2: ItemV2
  label=tagged widget revision=7
  tags: blue fragile
  attributes: shelf=3 weight=12
item 3: ItemV3
  title=assembled widget
  part frame 40x25.5 batches: 101 102
  part cover 41x26 batches: 201
  stock north: 5 8 13
  stock south: 21
  change: created by:ana
  change: resized by:ben width:40
```

`%events show items --index 2` prints the same snapshot without any C++.

You can also rebuild a captured `Item` as a local C++ object and walk it like
the original, private members included:

```cpp
auto item = tracer.batch("items").at(2).result_as<Item>();
item->id
auto v3 = static_cast<ItemV3*>(item->impl);
v3->parts.at(1).size.height
```

Expected: `3` and `26`.

## Part 2: the item a method is called on

`Item::encode` is one function, but each call is dispatched to a different
implementation. The probe captures the receiver (`this`), so each event says
which implementation ran. You can stay in the session of part 1 (skip
`%attach`) or start a new one. In the same session the REPL already has the
item classes from the first prepared package, so it keeps them and prints
`reused imported types: Item ItemInterface ...` for the second.

```bash
%attach 12345
%probe prepare "Item::encode(const std::string&) const" --include item_factory.hpp --translation-unit target.cpp --output prepared-encode
%probe inject "Item::encode(const std::string&) const" --prepared prepared-encode --prepared-mode require
%collect --name "Item::encode(const std::string&) const" --count 3 --timeout 30 --phase POST --as encoded
```

`--name` keeps the events of the part 1 probe out of this collection. Press
Enter once in the target terminal, then:

```bash
%cpp load encoded.cpp
%detach
```

Expected:

```text
item 1 (ItemV1) encode(text) = text:basic widget#4
item 2 (ItemV2) encode(text) = text:tagged widget#7 +blue +fragile shelf=3 weight=12
item 3 (ItemV3) encode(text) = text:assembled widget [frame x2] [cover x1] warehouses=2 changes=2
```

In this part the `Item` is the receiver, not the result: `encode` returns a
`std::string`, so `result_as<Item>()` fails with
`event record type mismatch: std::basic_string<char>`. Decode the receiver
instead:

```cpp
auto call = tracer.batch("encoded").at(0);
auto item = call.receiver_snapshot().as<Item>();
item->id
static_cast<ItemV1*>(item->impl)->revision
auto text = call.result_as<std::string>();
```

Expected: `1` and `4`, and `text` holds `text:basic widget#4`.

## How the capture works

- `Item` has a raw pointer member, `impl`. A prepared probe follows it and
  stores the copy as the field `impl_snapshot`, next to the address in `impl`.
- `impl` is declared as `ItemInterface*`. The probe reads the object's
  run-time type and copies the matching implementation, so
  `impl_snapshot.cpp_type()` is `ItemV1`, `ItemV2` or `ItemV3`. An
  implementation is only recognized if a header passed to `%probe prepare`
  defines it.
- `ItemV2` reports `label` and `revision` as well as its own fields, because
  inherited fields are captured with the derived class.
- Read one field with `field("name").as<T>()`, where `T` can be a standard
  container such as `std::map<std::string, int>`. `report.cpp` does this for
  `ItemV2`.
- Decode a whole implementation with `as<ItemV3>()`, or the whole handle with
  `result_as<Item>()`. These classes are rebuilt field by field without
  running their constructors, so use `->` to reach the fields and do not call
  their virtual functions. `report.cpp` does this for `ItemV3`.

## Things to try

- Add an `ItemV4` in its own header with another container, for example
  `std::vector<std::map<std::string, double>>`, and return it from the factory
  for `"v4"`. Run `make clean` and `make`, restart the target, and repeat part 1.
- Ask the factory for an unknown version. The item has a null `impl`, no
  `impl_snapshot` field, and the report prints `no implementation captured`.
- Move one implementation's definition out of the headers given to
  `%probe prepare`. Its items keep the `impl` address but lose the snapshot.

Run `make clean` before repeating the lesson so `prepared-create` and
`prepared-encode` can be created again.

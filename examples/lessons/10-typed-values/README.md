# Lesson 10: decode objects, containers, and strings

Guide section: [Decode objects, containers, and method receivers](../../../docs/guide/tutorials.md#decode-objects-containers-and-method-receivers).

One call receives a struct by reference, a `std::string`, a `std::vector<int>`
and a `std::map<std::string, int>`. You will read each as a typed C++ value in
the REPL without writing a decoder.

| File | Purpose |
| --- | --- |
| `types.hpp` | `shop::Order` and the declaration of `shop::price(...)`. |
| `target.cpp` | Calls `shop::price` with an order, a coupon, a vector of fees and a map of stock. |
| `hook.cpp` | An empty observer: the arguments and result are recorded by the probe itself. |
| `analysis.cpp` | Reads the captured event as C++ values. |

```bash
make
./target
```

Inject with the header so the tracer knows `Order`, then capture one call:

```bash
%attach 12345
%probe inject "shop::price" --callback hook.cpp --include types.hpp --cwd . --print-arguments --print-result
%collect --count 2 --timeout 30 --as orders
%events show orders --index 1
```

`--cwd .` resolves the relative `--include` from this directory. Press Enter once
while `%collect` waits. `%events show` prints the decoded
arguments: the struct fields, the string, the vector and the map.

Now let the REPL compile the same header and map `Order` into a real C++ type:

```bash
%cpp options add "-I."
%cpp import types.hpp --type shop::Order
%cpp load analysis.cpp
```

Expected output:

```text
order 7 qty=3 unit_price=19.5 coupon=SAVE5 fees=2 stock=2 result=60.5
```

`std::string`, `std::vector<int>` and `std::map<std::string, int>` need no
mapping; only your own records do. You can also read one field of a record
without any mapping: `sample.argument_snapshot(0).field("unit_price").as<double>()`.
Automatic mapping requires supported public members and an accessible default
constructor; use `EventCodec<T>` when you must construct through a factory.

The installed [typed values example](../../native-repl-values/README.md) runs
every supported container with `bash run.sh all`. The
[order-arguments example](../../order-arguments/README.md) covers pointers,
values and headerless DWARF recovery.

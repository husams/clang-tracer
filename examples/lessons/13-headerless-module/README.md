# Lesson 13: probe a library with no headers

Guide section: [Prepare a probe when you only have a binary](../../../docs/guide/user.md#prepare-a-probe-when-you-only-have-a-binary).
Reference: [native REPL walkthrough](../../../docs/native-repl.md#prepare-a-probe-from-a-binary-without-headers).

Sometimes you have a compiled library and no source. The module index reads
its DWARF types and ELF exports, so you can browse them and build a probe from
the binary alone.

| File | Purpose |
| --- | --- |
| `demo.hpp`, `demo.cpp` | `demo::Packet` and `demo::score(const demo::Packet&)`, built into `libdemo.so`. Pretend you do not have them. |
| `target.cpp` | Links `libdemo.so` and calls `score` on each Enter. |
| `observer.cpp` | Reads the packet through a generated view; no `demo.hpp` involved. |

```bash
make
./target
```

Index the library and browse it:

```text
%module index --file ./libdemo.so
%module types --filter "demo::*"
%module symbols --exported --filter "demo::*"
%module show --type demo::Packet
%module show --symbol "demo::score(demo::Packet const&)"
```

Generate the declarations and accessors, then a prepared package from them:

```text
%module emit --symbol "demo::score(demo::Packet const&)" --output recovered
%probe prepare "demo::score(demo::Packet const&)" --generated recovered --output prepared-score
```

Attach, validate and inject:

```text
%attach 12345
%probe validate "demo::score(demo::Packet const&)" --callback observer.cpp --prepared prepared-score --prepared-mode require
%probe inject "demo::score(demo::Packet const&)" --callback observer.cpp --prepared prepared-score --prepared-mode require --print-result
%collect --count 2 --timeout 30 --as scores
%events show scores --index 0
```

Press Enter once while `%collect` waits. Expected: PRE carries the packet
`value=4`, read through the generated view, and POST carries `score=42`.

The index is saved as `libdemo.so.tracer.sqlite` and becomes the session's
default; `%module use INDEX` selects it in a later REPL. The generated
`views.hpp` provides read-only accessors only for fixed-offset fields. Its
manifest lists any field it had to leave out.

# Lesson 4: edit, validate, reload, and restore

Guide section: [Edit, validate, reload, and restore](../../../docs/guide/tutorials.md#edit-validate-reload-and-restore).

| File | Purpose |
| --- | --- |
| `target.cpp`, `hook.cpp` | The first lesson's target and observer. |
| `updated.cpp` | `hook.cpp` with the POST field renamed from `sum` to `new_sum`. |

Run `make`, start `./target`, then start `clang-tracer-repl` in this directory.

```bash
%attach 12345
%interceptor load hook.cpp --as demo
%interceptor show demo
%interceptor validate demo --target "add(int, int)"
%interceptor upload demo --target "add(int, int)" --print-result
%probes
```

Replace `PROBE_ID` with the `id:` shown by `%probes`:

```bash
%probe reload PROBE_ID --callback updated.cpp
%collect --count 2 --timeout 30 --as updated
%events show updated --index 1
%probes
```

Press Enter in the target terminal while `%collect` waits. Reloading gives
the probe a new ID, so use the ID from the second `%probes` to remove it:

```bash
%probe remove NEW_PROBE_ID
%detach
```

Expected: the POST event includes `new_sum=42` and the target still prints 42.

# Lesson 3: edit, validate, reload, and restore

Guide section: [Edit, validate, reload, and restore](../../../docs/guide/tutorials.md#edit-validate-reload-and-restore).

You will keep an observer as a named draft, validate and upload it, then swap
it for a new version without detaching and remove it.

| File | Purpose |
| --- | --- |
| `hook.cpp` | The lesson 1 observer; POST records `sum`. |
| `updated.cpp` | The same observer with the field renamed to `new_sum`. |

```bash
make
./target
```

```bash
%attach 12345
%interceptor load hook.cpp --as demo
%interceptor show demo
%interceptor validate demo --target "add(int, int)"
%interceptor upload demo --target "add(int, int)" --print-result
%probes
```

`%interceptor edit demo` lets you replace the draft's source in the REPL; end
the input with `.end` to save it or `.cancel` to discard it.

Take the probe ID from `%probes`, then reload it with the new source:

```bash
%probe reload PROBE_ID --callback updated.cpp
%collect --count 2 --timeout 30 --as updated
%events show updated --index 1
%probes
```

Press Enter in the target terminal while `%collect` waits. Expected: the POST
event has `new_sum=42`, and the target still prints 42. A reload compiles the
new observer first and switches generations, so calls already running finish on the old one.

`%probes` now lists the old generation as inactive and the new one with
`active: true`. Finish by restoring the original code with the active ID:

```bash
%probe remove PROBE_ID
%detach
```

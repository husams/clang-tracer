# Lesson 4: trace a function template instantiation

Guide section: [Trace a function template instantiation](../../../docs/guide/tutorials.md#trace-a-function-template-instantiation).

A template is not one function: each instantiation is its own symbol. You will
trace `echo<int>` and `echo<double>` with one observer.

| File | Purpose |
| --- | --- |
| `target.cpp` | Calls `echo<int>(42)` and `echo<double>(1.5)` on each Enter. |
| `echo-hook.cpp` | One observer using `tracer::ResolvedSignature`. |

```bash
make
./target
```

```bash
%attach 12345
%symbols --filter "echo<"
%probe validate "echo<int>(int)" --callback echo-hook.cpp --prepared-mode off
%probe inject "echo<int>(int)" --callback echo-hook.cpp --prepared-mode off --print-result
%collect --name "echo<int>(int)" --count 2 --timeout 30 --as echo_int
%events list echo_int
```

Press Enter in the target terminal while `%collect` waits. Expected: PRE has
`input=42` and POST has `output=42`.

Now install the same source on the other instantiation:

```bash
%probe inject "echo<double>(double)" --callback echo-hook.cpp --prepared-mode off --print-result
%collect --name "echo<double>(double)" --count 2 --timeout 30 --as echo_double
%events show echo_double --index 1
```

Expected: `input=1.5` and `output=1.5`.

Quote the whole selector, including the concrete template arguments and the
parameter list. `--prepared-mode off` makes the tracer recover the signature
from DWARF. For a member template, give the complete selector of the concrete
method: class, template arguments, parameters and qualifiers.

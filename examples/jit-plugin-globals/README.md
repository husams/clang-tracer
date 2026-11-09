# JIT plugin global isolation

Both plugins define the C-linkage symbols `plugin_counter` and `tick`. The
standalone host loads their relocatable objects into separate ORC `JITDylib`
scopes within one JIT session. Each scope resolves its own `plugin_counter`.

## Run

Use a C++23 compiler, CMake, and an LLVM development installation. From the
worktree root, with that installation's `llvm-config` on `PATH`, run the
following commands. On macOS with Homebrew LLVM, add `$(brew --prefix llvm)/bin`
to `PATH` first.

```sh
cmake -S examples/jit-plugin-globals -B /tmp/clang-tracer-jit-plugin-globals \
  -DLLVM_DIR="$(llvm-config --cmakedir)"
cmake --build /tmp/clang-tracer-jit-plugin-globals --target run-jit-plugin-globals
```

## Example output

```text
A generation 1  address=0x7ff3de3a8000  value=0
B generation 1  address=0x7ff3de3a5000  value=0
A before reload  address=0x7ff3de3a8000  value=2
B before reload  address=0x7ff3de3a5000  value=20
A generation 2  address=0x7ff3de36e000  value=0
B after reload   address=0x7ff3de3a5000  value=20
A after tick     address=0x7ff3de36e000  value=1
B after tick     address=0x7ff3de3a5000  value=30
PASS
```

Addresses vary by run. Reload links A's existing object into a fresh scope and
then removes A's original scope. The executable checks the distinct addresses,
A's reset to zero, and B's preserved address and value before printing `PASS`.

## Run in the native REPL

From the worktree root, start a native REPL built from this branch and enter
the following commands. A fresh cache reports `compiled` for both loads.
The reload reports `cache hit`, followed by `a.value = 0` and `b.value = 10`.

```text
%plugin load examples/jit-plugin-globals/plugin_a.cpp --as a
%plugin load examples/jit-plugin-globals/plugin_b.cpp --as b
%plugin run a address
%plugin run b address
%plugin run a tick
%plugin run b tick
%plugin reload a
%plugin run a value
%plugin run b value
```

[LLVM ORC symbol resolution](https://llvm.org/docs/ORCv2.html#design-overview)

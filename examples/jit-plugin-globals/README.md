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

[LLVM ORC symbol resolution](https://llvm.org/docs/ORCv2.html#design-overview)

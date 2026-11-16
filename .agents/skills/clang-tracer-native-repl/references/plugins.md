# Load C++ plugins on `c++17`

## Run a plugin in the REPL

Use `%plugin load FILE --as NAME` for the current REPL. Use
`%plugin commands NAME` to discover commands and `%plugin run NAME COMMAND`
to invoke them. The REPL compiles C++17 source with its current compiler
profile or reuses a compatible cached object. `%plugin reload NAME` stages
a fresh generation. On success its globals reset while peer plugins keep
their state. On failure the old generation stays active.

From the repository root with a matching REPL:

```text
%plugin load examples/jit-plugin-globals/plugin_a.cpp --as a
%plugin load examples/jit-plugin-globals/plugin_b.cpp --as b
%plugin run a tick
%plugin run b tick
%plugin reload a
%plugin run a value
%plugin run b value
```

Expect `a.value = 0` and `b.value = 10` after reload. See
[local plugin setup](../../../../docs/native-repl.md#load-local-c-plugins)
for startup registration and compiler options.

## Run a plugin inside the target

Start the server with `CLANG_TRACER_ALLOW_AGENT_PLUGINS=1` and attach to the
Linux x86-64 process. Use the same environment variable on the REPL launch
when the REPL owns its server. Load trusted C++ source with
`%plugin agent load FILE --as NAME` and run commands with
`%plugin agent run NAME COMMAND`. The agent reports whether it accepted a
compatible REPL object, used its own cache, or compiled the source. For
reload and attachment lifetime, see the
[injected agent workflow](../../../../docs/native-repl.md#load-plugins-in-an-injected-agent).

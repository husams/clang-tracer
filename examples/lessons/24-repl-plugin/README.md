# Lesson 24: write, load, and reload a REPL plugin

Reference: [Load local C++ plugins](../../../docs/native-repl.md#load-local-c-plugins)
(REPL walkthrough; the user guide has no section for this lesson yet).

A plugin is a small C++ class that the REPL compiles and keeps loaded. Each of
its commands is a function that takes no arguments and returns a number. Use
one to keep your own counters, or to run a check you repeat during a session.
This lesson loads a plugin, calls its commands, edits it, reloads it, and
shows that a reload that fails leaves the working plugin in place.

This lesson needs no target process and no attach.

| File | Purpose |
| --- | --- |
| `plugin.cpp` | The plugin: a counter with the commands `tick` and `value`. |
| `edits/v1.cpp` | A copy of the original `plugin.cpp`. `make clean` restores it. |
| `edits/v2.cpp` | An edit: `tick` adds ten and there is a new command, `reset`. |
| `edits/broken.cpp` | An edit that does not compile. |
| `Makefile` | Has nothing to build. `make clean` restores `plugin.cpp`. |

Start `clang-tracer-repl` in this directory.

## Part 1: load the plugin and run its commands

```bash
%plugin load plugin.cpp --as counter
%plugin commands counter
%plugin run counter tick
%plugin run counter tick
%plugin run counter value
```

Expected:

```text
counter generation 1 active (compiled)
%plugin run counter tick  Add one to the count and return it.
%plugin run counter value  Return the count.
counter.tick = 1
counter.tick = 2
counter.value = 2
```

- `--as counter` is the name you use in later commands.
- `compiled` means the REPL built the source. Load it again in a new session
  with the same compiler inputs and it says `cache hit`.
- The count lives in the plugin, so each `tick` continues from the last.

## Part 2: edit the plugin and reload it

Replace `plugin.cpp` with the edited version, then reload:

```bash
%shell "cp edits/v2.cpp plugin.cpp"
%plugin reload counter
%plugin commands counter
%plugin run counter value
%plugin run counter tick
%plugin run counter reset
```

Expected: `counter generation 2 active (compiled)`, a third command named
`reset`, then `counter.value = 0`, `counter.tick = 10` and `counter.reset = 0`.

`%plugin reload` compiles the same file again, so you edit `plugin.cpp` and
reload; you do not name a new file. A successful reload starts a fresh
instance. The count was 2 before the reload and is 0 after it.

## Part 3: a failed reload keeps the old plugin

```bash
%shell "cp edits/broken.cpp plugin.cpp"
%plugin reload counter
%plugin run counter tick
%plugin run counter value
```

Expected: the reload prints `error: plugin compilation failed` with the
compiler's message (`use of undeclared identifier 'missing_name'`). The next
commands still work and report `counter.tick = 10` and `counter.value = 10`.
The count was 0 after `reset`, one `tick` made it 10, and the failed reload
changed nothing.

Run `%quit`, then `make clean` to put `plugin.cpp` back.

## Rules for a plugin

- Include `clang_tracer_repl_plugin.hpp` and define `tracer::repl::Plugin` in
  one source file.
- List the commands in a static `commands` array. Each command is a
  `noexcept` member that takes no arguments and returns a value that converts
  to `uint64_t`.
- Keep state in plain members or globals with constant initialisation.
  A plugin cannot call into libc or the C++ runtime. A call it cannot resolve
  fails the load and names the missing symbol.

## Things to try

- Load `plugin.cpp` a second time with `--as other`. The two plugins count
  separately, and reloading one does not reset the other.
- Run `%plugin enable plugin.cpp --as counter` to load the plugin at every
  start. `%plugin disable counter` removes that again.
- Start the REPL with `CLANG_TRACER_ALLOW_AGENT_PLUGINS=1`, attach to a
  process, and use `%plugin agent load plugin.cpp --as counter` to run the
  same plugin inside the target. See
  [Load plugins in an injected agent](../../../docs/native-repl.md#load-plugins-in-an-injected-agent).

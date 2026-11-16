# Lesson 14: automate tracing with Python

Guide sections: [Automate tracing with Python](../../../docs/guide/tutorials.md#automate-tracing-with-python)
and [Trace from the Python console or the native CLI](../../../docs/guide/tutorials.md#trace-from-the-python-console-or-the-native-cli).
More scripts: [Python API examples](../../python/README.md).

Everything the REPL did by hand, a script can do: start a server, attach,
inject a probe, stream events and clean up. This lesson reuses the lesson 1
target and observer.

| File | Purpose |
| --- | --- |
| `target.cpp`, `hook.cpp` | The lesson 1 target and observer. |
| `trace.py` | Attaches to a PID, installs `hook.cpp` on `add(int, int)`, prints four events, removes the probe and detaches. |

Install the Python client once, into a virtual environment:

```bash
python3 -m venv .venv
.venv/bin/python -m pip install ../../../python
```

```bash
make
./target
```

In another terminal:

```bash
.venv/bin/python trace.py TARGET_PID
```

Press Enter twice in the target terminal. Expected: a PRE with `left`/`right`
and a POST with `sum=42` for each press, and then the script exits with the
probe removed.

The same work is available without Python through the command-line client:
`clang-tracer attach`, `trace`, `events`, `detach` (see lesson 12) and
`clang-tracer events PID --sqlite copy.db` to copy events to disk. The full
command list is in the [command reference](../../../docs/guide/commands.md#native-command-line-client).
Other scripts: `stream.py` streams events live and `record.py` records them to a file.

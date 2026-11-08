# Python API examples

## Run the installed examples

On `linux-dev`, the installed API interpreter and examples can be used without a
source checkout. The target is compiled from the installed C++ example source:

```sh
TRACER_PYTHON="$HOME/.local/share/uv/tools/clang-tracer-client/bin/python"
TRACER_EXAMPLES=/usr/local/share/clang-tracer/examples
TRACER_RUN=$(mktemp -d /tmp/clang-tracer-example.XXXXXX)
clang++ -std=c++23 -g -O0 -fno-omit-frame-pointer -fcf-protection=none \
  "$TRACER_EXAMPLES/cpp-interceptors/target.cpp" -o "$TRACER_RUN/target"
"$TRACER_RUN/target" >"$TRACER_RUN/target.log" 2>&1 &
target_pid=$!
stop_target() {
  kill -TERM "$target_pid" 2>/dev/null || true
  wait "$target_pid" 2>/dev/null || true
}
trap stop_target EXIT
"$TRACER_PYTHON" "$TRACER_EXAMPLES/python/trace.py" "$target_pid"
"$TRACER_PYTHON" "$TRACER_EXAMPLES/python/stream.py" "$target_pid"
"$TRACER_PYTHON" "$TRACER_EXAMPLES/python/record.py" \
  "$target_pid" "$TRACER_RUN/capture.pb"
kill -TERM "$target_pid"
wait "$target_pid"
trap - EXIT
"$TRACER_PYTHON" "$TRACER_EXAMPLES/python/server_lifecycle.py"
```

Use your own client environment's Python if it was installed elsewhere. Native
server/agent discovery uses the existing installation and `PATH`. The fresh run
directory contains the target output and recorded traces for inspection.

## Run from a source checkout

Run these commands from the repository root on Linux x86-64, after configuring
the native build as described in the [root README](../../README.md).

```sh
cmake --build build -j4 --target \
  clang-tracer-server clang_tracer_agent cpp-interceptors-target
```

On `linux-dev`, use the installed client interpreter:

```sh
TRACER_PYTHON=/home/husam/.local/share/uv/tools/clang-tracer-client/bin/python
export PATH="$PWD/build:$PATH"
target_log=$(mktemp)
./build/cpp-interceptors-target >"$target_log" 2>&1 &
target_pid=$!
stop_target() {
  kill -TERM "$target_pid" 2>/dev/null || true
  wait "$target_pid" 2>/dev/null || true
}
trap stop_target EXIT
"$TRACER_PYTHON" examples/python/trace.py "$target_pid"
"$TRACER_PYTHON" examples/python/trace.py "$target_pid" \
  --selector 'Calculator::multiply(int, int) const' \
  --callback examples/cpp-interceptors/method.cpp
"$TRACER_PYTHON" examples/python/stream.py "$target_pid" --count 6
"$TRACER_PYTHON" examples/python/record.py "$target_pid" "$target_log.pb" --count 6
kill -TERM "$target_pid"
wait "$target_pid"
trap - EXIT
"$TRACER_PYTHON" examples/python/server_lifecycle.py
```

Elsewhere, create a virtual environment and install the Python client before
running the same scripts:

```sh
python3.12 -m venv .venv
.venv/bin/python -m pip install ./python
TRACER_PYTHON=.venv/bin/python
```

Each PID example starts a local server through `Client.start_server()` when
`--endpoint` is omitted; the server and agent must be discoverable from the
native build or installation, and `clang-tracer-server` must be on `PATH`.
Pass either `--endpoint unix:///absolute/path/control.sock` or
`--endpoint 127.0.0.1:50051` to use an already-running server. That server must
be built from a revision supporting `ResolveProbe`. The examples attach only
the target PID they receive and remove their probe and detach in cleanup.
`trace.py` prints event sequence, PRE/POST phase, call correlation, typed argument
and result values, and captured receiver fields. `stream.py` prints native stack
functions and the advancing resume cursor, and cancels its stream at the count.
`stream.py` and `record.py` also accept `--selector` and `--callback` for the
method example. `record.py` creates its output exclusively, then validates and
displays the length-prefixed Protobuf events. `server_lifecycle.py` shows
managed process state, client readiness/session listing and graceful SIGTERM
shutdown. Event counts depend on the target's call rate and include PRE and
POST events.

On a graceful-shutdown timeout, an owned server's private directory is retained
until it exits; the script reports its PID and endpoint for recovery. Normal
completion removes the temporary directory.

The [manual validation notes](manual-validation.md) record observed Linux API
results and example executions; addresses, PIDs, session IDs and counters vary.

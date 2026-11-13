# Lesson 12: shared context and thread propagation

Guide section: [Observe shared context and thread propagation](../../../docs/guide/tutorials.md#observe-shared-context-and-thread-propagation).

| File | Purpose |
| --- | --- |
| `target.cpp`, `interceptor.cpp` | Copies of the [shared-context example](../../shared-context/README.md) sources. |
| `Makefile` | Builds `shared-context-target` here, so no CMake build directory is needed. |

This lesson uses the `clang-tracer` CLI with one shell. It replaces the
guide's `cmake --build` line with `make` and uses local paths.

```bash
make
export CLANG_TRACER_ENDPOINT=127.0.0.1:50051
clang-tracer-server --listen "$CLANG_TRACER_ENDPOINT" > server.log 2>&1 &
server_pid=$!
./shared-context-target > target.log 2>&1 &
target_pid=$!
sleep 2
clang-tracer attach "$target_pid"
callback="$PWD/interceptor.cpp"
clang-tracer trace "$target_pid" 'process_request(int)' --callback "$callback" --callback-print-arguments true
clang-tracer trace "$target_pid" 'calculate_total(int)' --callback "$callback" --callback-print-arguments true
clang-tracer events "$target_pid" --context-field request_id,calls,last_result --count 24 --timeout 20
clang-tracer detach "$target_pid"
kill "$target_pid" "$server_pid"
unset CLANG_TRACER_ENDPOINT
```

Expected: `request.step` events whose `RequestState` lines share a request ID.
The worker reports twice the request ID and the outer call reports the worker
result plus one.

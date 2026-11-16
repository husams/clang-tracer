# Lesson 12: share state across probes and threads

Guide section: [Observe shared context and thread propagation](../../../docs/guide/tutorials.md#observe-shared-context-and-thread-propagation).
Reference: [Shared context](../../../docs/shared-context.md).

`process_request` starts a worker thread that calls `calculate_total`. One probe
on each function should see the same request state, even though the second call
runs on another thread.

| File | Purpose |
| --- | --- |
| `target.cpp` | `process_request(int)` joins a worker thread that runs `calculate_total(int)`. It loops once per second. |
| `interceptor.cpp` | `RequestState : shared_state<RequestScope>`, updated by both probes, published as `request.step`. |

```bash
make
./target
```

This lesson uses the command-line client, which needs a running server.

```bash
export CLANG_TRACER_ENDPOINT="unix://$PWD/control.sock"
clang-tracer-server --listen "$CLANG_TRACER_ENDPOINT" > server.log 2>&1 &
clang-tracer list    # repeat until it succeeds
clang-tracer attach TARGET_PID
callback="$PWD/interceptor.cpp"
clang-tracer trace TARGET_PID 'process_request(int)' --callback "$callback" --callback-print-arguments true
clang-tracer trace TARGET_PID 'calculate_total(int)' --callback "$callback" --callback-print-arguments true
clang-tracer events TARGET_PID --context-field request_id,calls,last_result --count 24 --timeout 20
clang-tracer detach TARGET_PID
kill %1   # stop the server
unset CLANG_TRACER_ENDPOINT
```

Expected: `request.step` events that share one `request_id`. The worker reports
`last_result` equal to twice the request ID; the outer call reports that value plus one.
`calls` reaches 2 because both probes updated the same object.

The worker inherits the context through supported `pthread_create` propagation.
Lesson 15 uses the same mechanism for a request that spans several functions
and threads at once.

# Legacy entry-only callback

[callback.cpp](callback.cpp) uses the older `TraceContext` interface. It reads
the first signed integer argument and returns `argument * 100 + 17` as a
callback value. It runs on entry, on the application's calling thread.

After starting the [C++ example target](../cpp-interceptors/README.md#run-the-normal-cli)
and attaching to its PID:

```bash
clang-tracer trace "$target_pid" 'add(int, int)' \
  --callback "$PWD/examples/legacy-callbacks/callback.cpp"
clang-tracer events "$target_pid" --count 10 --timeout 10
clang-tracer detach "$target_pid"
```

Use [basic callbacks](../basic-callbacks/README.md) to learn the PRE/POST class API.

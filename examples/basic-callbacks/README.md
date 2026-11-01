# Basic PRE and POST callbacks

[interceptor.cpp](interceptor.cpp) reads the first integer argument in PRE and
the original integer result in POST. Each invocation gets one interceptor
object shared by its two hooks.

Use the small target from [C++ objects](../cpp-interceptors/README.md). From a
configured Linux repository build, with an installed tracer:

```bash
cmake --build build --target cpp-interceptors-target
```

Start the server and target, then attach using the
[manual startup commands](../cpp-interceptors/README.md#run-the-normal-cli).
With that shell's `target_pid` and `CLANG_TRACER_ENDPOINT`:

```bash
clang-tracer trace "$target_pid" 'add(int, int)' \
  --callback "$PWD/examples/basic-callbacks/interceptor.cpp"
clang-tracer events "$target_pid" --count 10 --timeout 10
clang-tracer detach "$target_pid"
```

PRE returns `first_argument * 100 + 17` as the callback value; POST returns the
application's original result. Stop the demo target and server when finished.

# Mock application calls

Use `%live mock` for typed native mocks with fixtures or application bindings.
Use scalar replacement for an eligible function already marked for opt-in
control by a probe.

## Install a typed native mock

On a `c++17` runtime with `%help live mock`, attach to the Linux x86-64
target and use its exact C++ signature. Supply the C++17 mock source,
entry function, and any fixture file. This example uses the repository's
[ProductStore mock fixture](../../../../tests/fixtures/live_mock/product_fixture.cpp).

```text
%live mock "ProductStore::load(int)" --file product_fixture.cpp --function load_fixture --fixture products.txt
%live status
```

`--bind "ALIAS=SIGNATURE"` resolves a verified application callable.
`ctx.original(...)` calls the implementation underneath the mock, including
any active live update. Use `--recursion original` when recursive calls
need to route through it. Verify the caller's result and ownership contract.
`%live revert` restores the previous dispatch. Exercise the fixture with
`ctest --test-dir build -R '^native_repl_live_mock$' --output-on-failure`.
See the [native mock options](../../../../docs/native-repl.md#live-code-replacement-on-linux-x86-64)
for how mocks interact with source updates.

## Mock a scalar free function

`c++17` supports opt-in scalar replacement through a native interceptor.
`Call::replace_with(value)` in PRE supplies the result and skips the
application body. Use `replace_with()` for a void result. This path
requires a local Unix control socket, an unprepared DWARF binding and a
Clang-built Linux x86-64 SysV free function marked
`TRACER_SCALAR_OVERRIDE_TARGET`. All arguments and any nonvoid result must
use supported scalar types. Match the type and width exactly. These
requirements also apply to `Call::call_original<T>()` in PRE. Enable it
with `--allow-call-original` on its own or with POST result override.
The first call runs the original with the entry arguments, and repeated
calls return the cached result. The original-call option excludes
replacement and PRE argument override.

For a runnable example, use a full Clang build of the `c++17` branch with
tests enabled. From the repo root, build the existing launch fixture:

```sh
cmake --build build --target repl-overrides-target
./build/clang-tracer-repl --server ./build/clang-tracer-server --agent ./build/libclang_tracer_agent.so --fail-fast
```

Before launching the REPL, save this C++17 interceptor as `mock.cpp` in its
working directory. Save it as `mock-v2.cpp` with `11.0f` in place of
`9.0f` for the reload step.

```cpp
struct Interceptor {
  void pre(const Call& call) {
    call.replace_with(9.0f);
  }
};
```

Paste the commands in order, using each printed probe ID in the next
operation. The four calls should return `3`, `9`, `11`, and `3`.
`%launch` owns and pauses this fixture, so `%call` can invoke its explicitly
registered `float_sample` at the startup checkpoint.

```text
%launch ./build/repl-overrides-target
%call float_sample --arg 2
%probe validate "float_sample(float)" --callback mock.cpp --allow-replacement
%probe inject "float_sample(float)" --callback mock.cpp --allow-replacement
%call float_sample --arg 2
%probe reload PROBE_ID --callback mock-v2.cpp
%call float_sample --arg 2
%probe remove NEW_PROBE_ID
%call float_sample --arg 2
%terminate
%quit
```

Use the [scalar override guide](../../../../docs/native-repl.md#opt-in-to-scalar-overrides)
for the target marker and per-operation flags. Pass `--prepared-mode off`
to validation and injection if that selector already has a prepared package.

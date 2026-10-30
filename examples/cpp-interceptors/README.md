# Complete native C++ interceptor example

The target below uses ordinary C++ declarations, with no compiler attributes.
LLVM reads the running binary's DWARF; Clang constructs a checked receiver type
so `call.self()` reads object fields without target source or a copied class.
Control and events remain gRPC and binary Protobuf; the CLI displays readable events.

Run on Linux x86-64 with a configured tracer build. The existing example target
uses `-g` and `-O0` in CMake, without enabling executable symbol exports.
Debug information is required; source files and compilation databases are not.
DWARF supplies the relocated entry even when the function is absent from both
ELF symbol tables. Inlined calls and functions without a patchable concrete entry
are outside this example's coverage.

## target.cpp

The target prints its PID, arguments, member fields, and original results.

```cpp
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <ctime>

#include <unistd.h>

namespace {
volatile std::sig_atomic_t keep_running = 1;
int initialization_count = [] {
  std::puts("INITIALIZER fired");
  std::fflush(stdout);
  return 1;
}();

void stop_target(int) { keep_running = 0; }
} // namespace

int add(int left, int right) {
  volatile int saved_left = left;
  volatile int result = saved_left + right;
  return result;
}

class Calculator {
public:
  int bias = 7;
  mutable int calls = 0;

  int multiply(int value, int factor) const {
    ++calls;
    volatile int saved_value = value;
    volatile int result = saved_value * factor + bias + calls;
    return result;
  }

private:
  int hidden = 99;
};

int main() {
  std::signal(SIGINT, stop_target);
  std::signal(SIGTERM, stop_target);

  const Calculator calculator;
  int iteration = 1;
  std::printf("TARGET pid=%ld\n", static_cast<long>(::getpid()));
  std::fflush(stdout);

  while (keep_running != 0) {
    const int left = iteration;
    const int right = 10 + iteration;
    const int factor = 2 + iteration;
    const int sum = add(left, right);
    const int product = calculator.multiply(left, factor);
    std::printf("CALL init=%d left=%d right=%d add=%d value=%d factor=%d bias=%d calls=%d multiply=%d\n",
                initialization_count, left, right, sum, left, factor,
                calculator.bias, calculator.calls, product);
    std::fflush(stdout);

    iteration = iteration == 5 ? 1 : iteration + 1;
    timespec delay{0, 200'000'000};
    while (keep_running != 0 && ::nanosleep(&delay, &delay) != 0 &&
           errno == EINTR) {
    }
  }

  std::puts("TARGET stopped");
}
```

## method.cpp

The method interceptor reads bias and calls before and after multiply runs.

```cpp
// Clang constructs the receiver type from binary DWARF; no target source is
// needed.
class Interceptor {
public:
  void pre(const Call &call) {
    call.argument<int>(0);
    call.argument<int>(1);
    call.field(call.self()->bias);
    call.field(call.self()->calls);
  }

  void post(const Call &call) {
    call.argument<int>(0);
    call.argument<int>(1);
    call.field(call.self()->bias);
    call.field(call.self()->calls);
  }
};
```

`call.self()` is a const-qualified Calculator pointer reconstructed from DWARF.
`pre()` sees `calls` before the method increments it; `post()` sees the new value.
Argument 0 is `value`, argument 1 is `factor`; their names come from debug data.
`call.field(call.self()->bias)` infers `bias` from the callback's Clang AST.
There are no string labels in these interceptors. POST carries the original
result automatically. Private/protected fields remain inaccessible.

## free_function.cpp

The free-function interceptor captures both add arguments and its original result.

```cpp
// Parameter names come from DWARF and values are carried in Protobuf.
class Interceptor {
public:
  void pre(const Call &call) {
    call.argument<int>(0);
    call.argument<int>(1);
  }

  void post(const Call &call) {
    call.argument<int>(0);
    call.argument<int>(1);
  }
};
```

## Run the normal CLI

Use one shell in the Linux repository root so the endpoint and PID variables
stay available.
The following commands build only the requested executables and use the public CLI.

Build the target, server, client, and agent:

```bash
cmake --build build -j4 --target \
  clang-tracer-server clang-tracer clang_tracer_agent cpp-interceptors-target
```

Start the server with a private endpoint:

```bash
run_dir=$(mktemp -d /tmp/clang-tracer-demo.XXXXXX)
export CLANG_TRACER_ENDPOINT="unix://$run_dir/control.sock"

./build/clang-tracer-server \
  --agent "$PWD/build/libclang_tracer_agent.so" \
  --listen "$CLANG_TRACER_ENDPOINT" \
  --timeout 30 >"$run_dir/server.log" 2>&1 &
server_pid=$!

for attempt in $(seq 1 100); do
  ./build/clang-tracer list >/dev/null 2>&1 && break
  sleep 0.1
done
./build/clang-tracer list
```

Start the target and attach by PID:

```bash
./build/cpp-interceptors-target >"$run_dir/target.log" 2>&1 &
target_pid=$!

sleep 1
./build/clang-tracer attach "$target_pid"
```

Install both interceptors by their C++ names:

```bash
./build/clang-tracer trace "$target_pid" \
  'Calculator::multiply(int, int) const' \
  --callback "$PWD/examples/cpp-interceptors/method.cpp"

./build/clang-tracer trace "$target_pid" \
  'add(int, int)' \
  --callback "$PWD/examples/cpp-interceptors/free_function.cpp"
```

Read 20 events, then compare with the target's own output:

```bash
./build/clang-tracer events "$target_pid" --count 20 --timeout 10
tail -n 5 "$run_dir/target.log"
```

Detach while leaving the target running:

```bash
./build/clang-tracer detach "$target_pid" &&
  kill -0 "$target_pid" &&
  echo "Detached; target is still running."
```

After successful detach, optionally stop the demo processes:

```bash
kill "$target_pid" "$server_pid"
wait "$target_pid" "$server_pid"
```

No target source, build database, or include is needed to attach or trace.
An optional `--include /absolute/target_header.hpp` can provide richer receiver
declarations, still checked against the binary's debug layout. Missing or
unsupported DWARF produces an explicit error. This version uses embedded DWARF
and simple scalar/pointer receiver layouts; external/split debug files,
inheritance, virtual layout, bitfields, and aggregate fields are unsupported.
The Linux host must permit attaching to the target.

## Recorded normal CLI run

The [recorded output](example-output.txt) was collected with the target source
and compilation database temporarily unavailable, and without optional includes.
Both interceptors produced named arguments and inferred field labels. Both
entries were restored on detach; the target continued running with TracerPid=0.
No integration tests were run.

For the same lifecycle with automatic startup and cleanup, run:

```bash
./examples/cpp-interceptors/run.sh
```

That helper uses the normal CLI, then stops the processes it started after detach.
Its optional first argument selects a different configured build directory.

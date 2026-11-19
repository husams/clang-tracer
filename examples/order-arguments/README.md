# DWARF-typed class-instance arguments

`call.argument<Index>()` exposes any supported argument position using the type
resolved from DWARF. No Order/Coupon declaration or target header is needed in
the callback. This example mixes two different class types, a by-value Order,
references, a pointer, an integer, and a double.

| Index in total | Target type | Callback access |
| --- | --- | --- |
| 0 | Order& | const auto& order = call.argument<0>() |
| 1 | double | auto fee = call.argument<1>() |
| 2 | const Coupon& | const auto& coupon = call.argument<2>() |
| 3 | Order | const auto& sample = call.argument<3>() |
| 4 | int | auto count = call.argument<4>() |

Views of class arguments are const-qualified. Reference and pointer views refer
to the actual argument object; POST sees its updates. A by-value argument exposes
the wrapper's entry snapshot, which remains unchanged in POST even if the callee
modifies its own copy. `call.result<T>()` exposes the original result in POST.

Field paths are inferred from the originating argument, such as `order.quantity`
and `sample.quantity`; the callback's local alias need not match the target name.
Local scalar captures infer their labels too. Transport remains gRPC/Protobuf.

## order.hpp (target-owned, optional for tracing)

```cpp
#pragma once

struct Order {
  int quantity;
  double unit_price;
};
struct Coupon {
  double amount;
  int uses;
};
```

## target.cpp

```cpp
#include "order.hpp"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <unistd.h>

// The tracer obtains these argument types solely from the binary's DWARF.
double total(Order &order, double fee, const Coupon &coupon, Order sample,
             int count) {
  volatile double saved_fee = fee;
  const double result = order.quantity * order.unit_price + saved_fee -
                        coupon.amount + sample.quantity * sample.unit_price;
  order.quantity += count;
  sample.quantity +=
      10; // A local by-value copy: the caller's sample stays unchanged.
  return result;
}

double pointer_total(Order *order, int extra) {
  volatile int saved_extra = extra;
  if (!order)
    return 0;
  const double result = order->quantity * order->unit_price + saved_extra;
  order->quantity += extra;
  return result;
}

namespace {
volatile std::sig_atomic_t running = 1;
void stop(int) { running = 0; }
} // namespace
int main() {
  std::signal(SIGINT, stop);
  std::signal(SIGTERM, stop);
  std::printf("TARGET pid=%ld\n", static_cast<long>(::getpid()));
  std::fflush(stdout);
  int calls = 0;
  int failures = 0;
  while (running) {
    Order order{3, 12.50};
    const Coupon coupon{2.0, 1};
    Order sample{2, 3.50};
    const double result = total(order, 1.25, coupon, sample, 2);
    const int after_total = order.quantity;
    const double pointer_result = pointer_total(&order, 3);
    if (result != 43.75 || after_total != 5 || order.quantity != 8 ||
        sample.quantity != 2 || pointer_result != 65.5)
      ++failures;
    std::printf("CALL calls=%d order_before=3 order_after_total=%d "
                "order_after_pointer=%d sample=2 fee=1.25 coupon=2 count=2 "
                "total=%.2f pointer=%.2f failures=%d\n",
                ++calls, after_total, order.quantity, result, pointer_result,
                failures);
    std::fflush(stdout);
    timespec delay{0, 200'000'000};
    while (running && ::nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
  }
}
```

## interceptor.cpp

```cpp
// No Order/Coupon definition or target headers are needed here.
class Interceptor {
  static void capture(const Call &call) {
    const auto &order = call.argument<0>();
    const auto fee = call.argument<1>();
    const auto &coupon = call.argument<2>();
    const auto &sample = call.argument<3>();
    const auto count = call.argument<4>();
    call.field(order.quantity);
    call.field(order.unit_price);
    call.field(coupon.amount);
    call.field(coupon.uses);
    call.field(sample.quantity);
    call.field(sample.unit_price);
    call.field(fee);
    call.field(count);
  }

public:
  void pre(const Call &call) { capture(call); }
  void post(const Call &call) { capture(call); }
};
```

## pointer.cpp

```cpp
class Interceptor {
  static void capture(const Call &call) {
    const auto *order = call.argument<0>();
    const auto extra = call.argument<1>();
    if (order) {
      call.field(order->quantity);
      call.field(order->unit_price);
    }
    call.field(extra);
  }

public:
  void pre(const Call &call) { capture(call); }
  void post(const Call &call) { capture(call); }
};
```

## Run using the normal CLI

On Linux x86-64, in a configured repository checkout:

```bash
cmake --build build -j4 --target \
  clang-tracer clang-tracer-server clang_tracer_agent order-arguments-target
./examples/order-arguments/run.sh
```

The helper uses only normal CLI attach/trace/events/detach commands, verifies
PRE/POST from both probes and progress after detach, then stops its demo processes.
No integration tests are invoked. The example build uses -g and -O0, without
enabling executable symbol exports; the target has no attributes. DWARF supplies
the concrete entry and its ASLR relocation. For individual commands in one shell:

```bash
run_dir=$(mktemp -d /tmp/clang-tracer-order-demo.XXXXXX)
export CLANG_TRACER_ENDPOINT="unix://$run_dir/control.sock"
./build/clang-tracer-server --agent "$PWD/build/libclang_tracer_agent.so" \
  --listen "$CLANG_TRACER_ENDPOINT" --timeout 30 >"$run_dir/server.log" 2>&1 &
server_pid=$!
for attempt in $(seq 1 100); do
  ./build/clang-tracer list >/dev/null 2>&1 && break
  sleep 0.1
done
./build/clang-tracer list
./build/order-arguments-target >"$run_dir/target.log" 2>&1 &
target_pid=$!
sleep 1
./build/clang-tracer attach "$target_pid"
./build/clang-tracer trace "$target_pid" \
  'total(Order&, double, const Coupon&, Order, int)' \
  --callback "$PWD/examples/order-arguments/interceptor.cpp"
./build/clang-tracer trace "$target_pid" 'pointer_total(Order*, int)' \
  --callback "$PWD/examples/order-arguments/pointer.cpp"
./build/clang-tracer events "$target_pid" --count 20 --timeout 10
./build/clang-tracer detach "$target_pid"
sleep 1
kill -0 "$target_pid"
grep '^TracerPid:' "/proc/$target_pid/status"
tail -n 3 "$run_dir/target.log"
# After successful detach, stop the demo processes when finished:
kill "$target_pid" "$server_pid"
wait "$target_pid" "$server_pid"
```

No source, compilation database or include options are used by tracing. The
[recorded output](example-output.txt) was captured with the target source and
compilation database temporarily unavailable. total showed order.quantity 3 -> 5,
sample.quantity 2 -> 2, fee 1.25, coupon.amount 2, count 2, result 43.75;
pointer_total showed order.quantity 5 -> 8 and result 65.5. Both entries restored,
TracerPid became 0, the target continued, and its result-failure count stayed 0.

## Optional headers and compile_commands.json

The commands above work without the target's headers, sources or build database.
When a target header is available, Clang can use its original declarations:

```bash
./build/clang-tracer trace "$target_pid" \
  'total(Order&, double, const Coupon&, Order, int)' \
  --callback examples/order-arguments/interceptor.cpp \
  --include examples/order-arguments/order.hpp --compdb build
```

`--compdb` accepts a directory or a compile_commands.json file. Clang infers the
header's command from source entries using filename and directory similarity;
the CLI prints which entry it selected. It uses the command's flags and working
directory to parse a synthetic header-only translation unit. The selected .cpp
does not need to exist. Without `--compdb`, the CLI searches parent directories
of the first header for a database; if none exists, standalone headers use C++20.
All supplied headers share the first header's inferred command. The resulting
class layout and ABI must still match the binary's DWARF before patching.

## Supported boundary

Up to eight explicit arguments and 16 named arguments/fields per hook. Flat named
class/struct types must have complete embedded DWARF and matching ordinary layout
with supported scalar/builtin-pointer members. The agent constructs all argument
record declarations with Clang APIs and checks type, access, size and every
member offset. Any conflicting layouts fail before patching.

By-value classes require explicit DWARF pass-by-value calling-convention evidence;
Clang then performs the native aggregate ABI lowering. Nontrivial/unsupported
passing conventions, inheritance, virtual layout, bitfields, nested aggregate
members, class results, and missing/incomplete debug information are rejected.
Callback-side class construction/copying and method invocation remain rejected;
the generated native wrapper may copy only the validated by-value argument.
Keep views within the invocation and obey the target's object-lifetime and
synchronization rules. Optional headers are not required.

Native bootstrap ABI is 13; Protobuf adds OBJECT=8 for by-value snapshot addresses.
The event protocol remains version 2 and named member values remain typed scalars.

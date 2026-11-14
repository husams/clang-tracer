# Lesson 15: collect every call of one request

Guide section: [Collect every call of one request](../../../docs/guide/tutorials.md#collect-every-call-of-one-request).
State API: [Shared context](../../../docs/shared-context.md).

The target is a small checkout service. One request, `handle_checkout`, calls
`authenticate`, `price_item` two or three times, and `charge_card`. Each Enter
serves two requests on two threads at once, so their calls interleave and a
plain event list cannot tell you which call belongs to which request.

This lesson installs four probes with four different hook files and connects
them through one shared state. The step probes append to the state; the
request probe publishes it as a single event when the request returns.

| File | Purpose |
| --- | --- |
| `target.cpp` | The checkout service. It has no dependency on the tracer. |
| `state.hpp` | `RequestState`: the request number and the list of steps. Every hook includes it. |
| `request.cpp` | Probe for `handle_checkout`: records the request number, publishes `request.completed` on return. |
| `authenticate.cpp`, `price_item.cpp`, `charge_card.cpp` | One probe per step: each appends its call to `RequestState`. |
| `report.cpp` | Prints the steps of each collected request, for `%cpp load report.cpp`. |

Run `make`, start `./target`, then start `clang-tracer-repl` in this directory.

```bash
%attach 12345
%probe inject "handle_checkout(int, int)" --callback request.cpp --print-arguments --print-result
%probe inject "authenticate(int)" --callback authenticate.cpp
%probe inject "price_item(int, int)" --callback price_item.cpp
%probe inject "charge_card(int, int)" --callback charge_card.cpp
%collect --name request.completed --count 2 --timeout 30 --as requests
```

Press Enter once in the target terminal while `%collect` waits, then:

```bash
%events list requests
%cpp load report.cpp
%detach
```

Expected: `%events list` shows two POST events of `handle_checkout`, one per
request, and the report prints each request with its own calls:

```text
request 1: authenticate(41)=1 price_item(120)=24 price_item(350)=35 charge_card(41)=9041
request 2: authenticate(77)=1 price_item(120)=24 price_item(350)=35 price_item(990)=297 charge_card(77)=9077
```

`%events show requests --index 0` prints the same state as a
`decoded state RequestState {...}` line, without any C++.

## How the probes are connected

- All four hooks declare `Interceptor(State<RequestState>)`. Because
  `RequestState` derives from `shared_state<RequestScope>`, probes that run
  inside the same request receive the same object.
- The state starts with the outermost probed call and ends when that call
  returns. Here that is `handle_checkout`, so each request gets a fresh
  `RequestState` and the two overlapping requests never mix.
- The step probes publish nothing. Only `request.cpp` calls
  `call.context().event("request.completed")`, and `%collect --name` selects
  that event, so you receive one event per request instead of one per call.

To trace another step, add a hook file like `price_item.cpp` and inject it;
the other probes do not change.

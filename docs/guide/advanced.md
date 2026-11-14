# Advanced capture and custom adapters

Start with [the polymorphic items lesson](tutorials.md#lesson-16-follow-polymorphic-items-through-two-probes). It uses automatic capture. Use the following controls when you need to select what is copied, change its shape, or interpret the saved data differently.

| Goal | Where the code runs | Extension point |
| --- | --- | --- |
| Select which pointers a prepared probe follows | Preparation tool | [FollowPointers policy](#write-a-followpointers-policy) |
| Copy a smaller or redacted record from the target | Target agent during the probe | [Custom snapshot encoder](#write-a-target-side-snapshot-encoder) |
| Construct your own type from an existing event | Client REPL, also offline | [EventCodec decoder](#write-a-client-side-decoder) |
| Report logical method changes and register any payloads | Object-watch adapter in the target | [Method snapshots and AnyPayloads](#watch-logical-state-at-method-boundaries) |

## Write a FollowPointers policy

In the [lesson 16 project](../../examples/lessons/16-polymorphic-items/README.md), write `item-policy.hpp` beside `item_factory.hpp`:

```cpp
#pragma once
#include "item.hpp"

namespace tracer_snapshot {
template<class Record> struct FollowPointers;

template<> struct FollowPointers<Item> {
  static constexpr const char* fields[] = {"impl"};
};
}
```

Put the callable's declaration header first and repeat `--include` for the policy and any additional implementation definitions. From the attached lesson REPL, prepare a **new** package and inject it:

```text
%probe prepare "ItemFactory::create(const std::string&) const" --include item_factory.hpp --include item-policy.hpp --translation-unit target.cpp --output prepared-policy
%probe inject "ItemFactory::create(const std::string&) const" --prepared prepared-policy --prepared-mode require --snapshot-return-pointee
%collect --name "ItemFactory::create(const std::string&) const" --phase POST --count 3 --timeout 30 --as policy_items
```

Press Enter once in the target while collecting. Inspect the concrete implementation:

```cpp
auto root = tracer.batch("policy_items").at(0).result_snapshot();
if (root.fields().count("impl_snapshot")) {
  std::cout << root.field("impl_snapshot").cpp_type() << '\n';
}
```

Expect `ItemV1` for the first item. The record keeps `impl` as an address and adds `impl_snapshot` as an owned copy. The policy names direct raw pointer or reference members for that record, in arguments, receivers, and results. It replaces automatic selection for that record. The agent needs a compatible admitted layout and a live, stable pointee at capture time; null or unknown dynamic types can leave only an address. The outer pointer result requires `--snapshot-return-pointee`; an outer reference result already captures its referent.

For a comparison, prepare a separate package without the policy and with `--no-automatic-members`. Expect the address and no followed member. Read the [followed-member limits](../native-repl.md#followed-member-limits) before increasing capture depth, breadth, or byte budgets. A runtime limit cannot restore a member left out during preparation.

## Write a target-side snapshot encoder

The [custom PIMPL example](../../examples/prepared-injectors/README.md#polymorphic-pointer-returns) contains the [encoder source](../../examples/prepared-injectors/pimpl/encoder.cpp) and [REPL script](../../examples/prepared-injectors/pimpl/custom.repl). The prepared adapter declares the implementation type before compiling this callback. Add an overload for the concrete type you want to project:

```cpp
namespace tracer_snapshot {
struct V2Projection {
  int summary;
};

V2Projection encode(const HFW::ItemV2& item) noexcept {
  return {static_cast<int>(item.generation) + 1000};
}
}
```

The encoder runs on the target thread. Return a supported record **by value**, keep the work bounded, and avoid reading storage whose lifetime or synchronization is uncertain. Pass the encoder as `--callback` at injection; the file can contain just the encoder. Other implementations keep automatic capture. The payload retains its original C++ type name even though its fields have changed, so do not decode it using an automatic layout that expects the old fields.

From the repository root on Linux, after a full build in `build`, run:

```bash
./examples/prepared-injectors/run-native.sh --pimpl-custom --smoke
```

Expect `NATIVE_PIMPL_CUSTOM=1`. The check verifies a one-field projection with `summary=1002` for `HFW::ItemV2` and unchanged automatic fields for V1 and V3. Omit `--smoke` to get the commands at an interactive prompt.

## Write a client-side decoder

After collecting the custom encoder events as `captured` (the interactive example does this), define a separate local type. Put this declaration in a C++ file and load it with `%cpp load` before the first typed conversion:

```cpp
struct ItemSummary {
  int summary;
};

template<> struct tracer_repl::EventCodec<ItemSummary> {
  static ItemSummary decode(const tracer_repl::EventValue& root) {
    return {root.field("_imp_snapshot").field("summary").as<int>()};
  }
};
```

At the REPL, check the payload before using the decoder because the collection also contains other implementations:

```cpp
for (const auto& e : tracer.batch("captured")) {
  if (e.result() == "0x0") continue;
  auto item = e.result_snapshot();
  if (item.fields().count("_imp_snapshot") &&
      item.field("_imp_snapshot").fields().count("summary")) {
    std::cout << e.result_as<ItemSummary>().summary << '\n';
  }
}
```

Expect `1002` for a projected item. The decoder can call a constructor or factory, rename fields, or explicitly handle older schemas. It runs on owned data, so the same decoder works on a compatible recording after the target exits. For a default-constructible record with assignable members, [EventRecord](api-reference.md#eventrecord-and-eventcodec) is a simpler mapping. If decoding yields a `Reconstructed<T>`, use it for inspection: application constructors and destructors did not run, virtual dispatch is unavailable, and embedded pointers refer to local copies.

## Watch logical state at method boundaries

A method watch compares receiver state at verified entry and return. It shows net changes; use a [write watch](tutorials.md#lesson-17-trace-writes-without-rebuilding-the-target) for individual admitted stores. Work through the [Portfolio method-watch commands](../native-repl.md#prepare-an-object-watch-for-method-snapshots) for compilation database, plan creation, preview, recording, and replay.

For a direct `std::any` field, register specific types in the policy header given to `%object prepare --policy`. The [snapshot test policy](../../tests/fixtures/object_snapshots/policy.hpp) is a full example. For a Portfolio with a field called `extension` holding a `demo::RiskState`:

```cpp
#include "portfolio.hpp"
#include <tuple>

namespace tracer_snapshot {
template<class Record> struct AnyPayloads;

template<> struct AnyPayloads<demo::Portfolio> {
  using extension = std::tuple<demo::RiskState>;
};
}
```

Select `--field extension_snapshot` to report the typed projection. Register only types the original compilation inputs define. An unregistered payload can appear as a coverage gap. For map or vector snapshots, `--key PATH=MEMBER` specifies a stable logical key so a reorder need not masquerade as a change. Use capture limits for large data and inspect gaps before interpreting absent changes.

### Compare two recorded receiver samples

For a prepared method probe, enable independent return sampling and record both phases. This is useful when you already need the PRE and POST call events as well as the delta:

```text
%probe inject "demo::Portfolio::rebalance()" --prepared prepared-rebalance --prepared-mode require --snapshot-receiver-at-return
%record rebalance.db --sqlite --probe PROBE_ID --count 2 --timeout 20
%events open rebalance.db --as rebalance
%events diff rebalance --pre 0 --post 1 --key legs=id --key risk=key
```

Prepare `prepared-rebalance` with [%probe prepare](command-reference.md#command-probe-prepare) and the same header, policy as an extra include, translation unit and ELF used for the method watch. Invoke rebalance once during recording. Use the actual PRE and POST indexes if your recording has other events. The diff checks the invocation, receiver, and order before reporting deltas.

Choose the synchronization contract based on the target: `thread-confined` for a graph accessed by one thread, `caller-lock` if the caller protects the full graph through both samples, or `stable-at-boundaries` if the target ensures stable entry and return samples. The flag declares an existing guarantee; it does not lock application memory for you.

Use the [C++ API appendix](api-reference.md) for extension signatures and the [command index](command-reference.md) for command details.

// Calls every BoundedProtoWriter entry point that the agent keeps local. The JIT only
// declares them (context_codec.hpp), so each one must be bound by the agent.
#include "context_codec.hpp"

template <> class Interceptor<int(int)> {
public:
  long long pre(const Call &call) {
    std::byte storage[256];
    tracer::context_engine::BoundedProtoWriter writer(storage);
    std::size_t selected = 0;
    writer.u64(1, 7);
    writer.s64(2, -3);
    writer.boolean(3, true);
    writer.f32(4, 1.5f);
    writer.f64(5, 2.5);
    writer.string(6, "hidden");
    writer.begin_message(7);
    writer.container_items(10, 4, selected);
    writer.end_message();
    const auto bytes = writer.finish();
    return static_cast<long long>(bytes.size()) * 1000 + call.arg<int>(0);
  }
  long long post(const Call &call) { return call.result<int>(); }
};

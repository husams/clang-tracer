// Calls every BoundedProtoWriter entry point that the agent keeps local. The JIT only
// declares them (context_codec.hpp), so each one must be bound by the agent. The
// encoded bytes, each call's result and the selected item count are appended to the
// file named by HIDDEN_WRITER_OUT, one line per call, so the test can compare them
// with the expected encoding. (Context state cannot carry them: the context analysis
// rejects calls to these members, which have no visible body.)
#include "context_codec.hpp"

#include <cstdio>
#include <cstdlib>

template <> class Interceptor<int(int)> {
public:
  void pre(const Call &call) {
    std::byte storage[256];
    tracer::context_engine::BoundedProtoWriter writer(storage);
    std::size_t selected = 0;
    int results = 0;
    int bit = 0;
    auto note = [&](bool ok) { results |= (ok ? 1 : 0) << bit++; };
    note(writer.u64(1, 7));
    note(writer.s64(2, -3));
    note(writer.boolean(3, true));
    note(writer.f32(4, 1.5f));
    note(writer.f64(5, 2.5));
    note(writer.string(6, "hidden"));
    note(writer.begin_message(7));
    note(writer.container_items(10, 4, selected));
    note(writer.end_message());
    const auto bytes = writer.finish();
    std::string hex;
    for (const auto byte : bytes) {
      const int value = static_cast<int>(byte);
      hex += "0123456789abcdef"[value >> 4];
      hex += "0123456789abcdef"[value & 15];
    }
    if (const char *path = std::getenv("HIDDEN_WRITER_OUT"))
      if (std::FILE *out = std::fopen(path, "a")) {
        std::fprintf(out, "%s %zu %zu %d %d\n", hex.c_str(), bytes.size(),
                     selected, results, static_cast<int>(writer.status()));
        std::fclose(out);
      }
  }
  void post(const Call &) {}
};

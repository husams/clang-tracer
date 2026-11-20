// Declares a BoundedProtoWriter::string with a signature the agent does not implement.
// The JIT linker must report it as an unresolved symbol rather than binding the
// agent's string(unsigned, std::string_view) to a call with different parameters.
namespace tracer::context_engine {
struct BoundedProtoWriter {
  bool string(unsigned field, const char *text) noexcept;
};
} // namespace tracer::context_engine

template <> class Interceptor<int(int)> {
public:
  void pre(const Call &) {
    tracer::context_engine::BoundedProtoWriter writer;
    writer.string(1, "mismatch");
  }
  void post(const Call &) {}
};

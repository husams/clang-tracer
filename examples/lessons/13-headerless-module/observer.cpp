// The prepared package embeds the recovered declarations and read-only views,
// so the callback never includes demo.hpp.
template <>
class Interceptor<int(const demo::Packet&)> {
public:
  void pre(const Call& call) {
    call.field("value", tracer_recovered::demo::PacketView::value(call.argument<0>()));
  }

  void post(const Call& call) { call.field("score", call.result<int>()); }
};

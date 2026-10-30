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

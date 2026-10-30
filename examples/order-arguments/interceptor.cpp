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

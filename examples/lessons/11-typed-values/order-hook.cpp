// The argument type comes from the target's debug information, so this
// observer needs no application header.
class Interceptor {
public:
  void pre(const Call& call) {
    const auto& order = call.argument<0>();
    call.field(order.id);
    call.field(order.quantity);
  }

  void post(const Call& call) {
    call.field("total", call.result());
  }
};

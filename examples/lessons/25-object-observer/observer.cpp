// A custom observer for shop::Account::deposit(int). The tracer supplies
// Interceptor, Call and ResolvedSignature; ResolvedSignature stands for the
// signature of whichever method this package was prepared for.
template <> class Interceptor<tracer::ResolvedSignature> {
public:
  // Runs before deposit(). Reads the argument and the receiver's fields.
  void pre(const Call &call) {
    call.field("object_id", call.object_id());
    call.field("generation", call.object_generation());
    call.field("amount", call.argument<int>(0));
    call.field("balance_before", call.self()->balance);
  }

  // Runs after deposit() returns normally.
  void post(const Call &call) {
    call.field("object_id", call.object_id());
    call.field("balance_after", call.self()->balance);
    call.field("deposits", call.self()->deposits);
    call.field("returned", call.result<int>());
  }
};

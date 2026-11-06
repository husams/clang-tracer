// The prepared package supplies the application declarations. The scope holds
// an owned copy of the incoming values until the named event is published.
template <>
class Interceptor<prepared_demo::Receipt(const std::string &, std::vector<int>,
                                         prepared_demo::Adjustment,
                                         std::unique_ptr<int>,
                                         const std::shared_ptr<int> &,
                                         const std::weak_ptr<int> &,
                                         prepared_demo::Choice)> {
public:
  void pre(const Call &call) {
    call.context().init_scope<prepared_demo::ReadingScope>(
        prepared_demo::Reading{call.argument<0>(), call.argument<1>(),
                               call.argument<2>()});
  }

  void post(const Call &call) { call.context().event("account.adjusted"); }
};

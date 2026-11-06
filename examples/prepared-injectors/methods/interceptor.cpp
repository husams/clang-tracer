// User-owned callback. The package generator writes its own files separately.
template <>
class Interceptor<prepared_demo::Receipt(const std::string &, std::vector<int>,
                                         prepared_demo::Adjustment,
                                         std::unique_ptr<int>,
                                         const std::shared_ptr<int> &,
                                         const std::weak_ptr<int> &,
                                         prepared_demo::Choice)> {
public:
  long long pre(const Call &call) {
    const auto &units = call.argument<3>();
    const auto &factor = call.argument<4>();
    return call.self()->account_id + call.argument<2>().delta +
           (units && factor ? *units * *factor : 0) +
           std::get<2>(call.argument<6>()).size();
  }

  long long post(const Call &call) { return call.result().total; }
};

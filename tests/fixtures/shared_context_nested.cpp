#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace demo {

struct RequestScope {};

struct Item {
  std::string sku;
  std::uint32_t quantity;
};

struct ServiceStats {
  std::uint64_t calls = 0;
  std::vector<double> durations_ms;
};

struct Failure {
  std::int32_t code;
  std::string message;
};

struct RequestState : shared_state<RequestScope> {
  std::string request_id;
  std::vector<Item> items;
  std::map<std::string, ServiceStats> services;
  std::optional<Failure> failure;
};

struct RequestInterceptor {
  RequestInterceptor(State<RequestState>);

  void pre(const Call&, RequestState& state) {
    state.request_id = "request-42";
    state.items.push_back({"book", 2});
  }

  void post(const Call& call, RequestState& state) {
    auto& billing = state.services["billing"];
    ++billing.calls;
    billing.durations_ms.push_back(12.5);

    call.context().event("request.completed");

    // Alternatively, cap each container at 100 captured items:
    // call.context().event("request.completed", 100);
  }
};

} // namespace demo

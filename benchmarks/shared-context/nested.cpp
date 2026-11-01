#include <map>
#include <optional>
#include <string>
#include <vector>
struct BenchScope {};
struct Item { std::string sku; unsigned quantity; };
struct Service { unsigned long long calls = 0; std::vector<double> durations; };
struct Failure { int code; std::string message; };
struct BenchState : shared_state<BenchScope> {
  unsigned long long calls = 0;
  std::string request_id = "request-42";
  std::vector<Item> items = {{"book", 2}, {"long-product-identifier-1234567890", 4}, {"pen", 3}, {"case", 1}};
  std::map<std::string, Service> services = {{"billing", {0, {1.5, 2.5, 3.5, 4.5}}}, {"shipping", {0, {2.0, 3.0, 4.0, 5.0}}}};
  std::optional<Failure> failure = Failure{7, "retryable simulated failure"};
};
struct Interceptor {
  Interceptor(State<BenchState>);
  void pre(const Call&, BenchState& state) { ++state.calls; }
  void post(const Call& call, BenchState& state) {
    ++state.calls;
    ++state.services["billing"].calls;
    call.context().event("bench.nested");
  }
};

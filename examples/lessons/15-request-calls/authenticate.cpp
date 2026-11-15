// Probe for authenticate: appends this call to the request it runs in.
#include "state.hpp"

struct Interceptor {
  Interceptor(State<RequestState>);

  void post(const Call& call, RequestState& state) {
    state.steps.push_back({"authenticate", call.arg<int>(0), call.result<int>()});
  }
};

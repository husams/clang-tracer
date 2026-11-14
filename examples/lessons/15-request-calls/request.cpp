// Probe for handle_checkout(int, int): it opens the request and reports it.
#include "state.hpp"

struct Interceptor {
  // Asks the engine to create or reuse RequestState. Declaration only.
  Interceptor(State<RequestState>);

  // The outermost call of the request: the state is fresh here.
  void pre(const Call& call, RequestState& state) {
    state.request = call.arg<int>(0);
  }

  // By now the step probes have appended every call the request made.
  // Publish the whole list as one event.
  void post(const Call& call, RequestState&) {
    call.field("receipt", call.result<int>());
    call.context().event("request.completed");
  }
};

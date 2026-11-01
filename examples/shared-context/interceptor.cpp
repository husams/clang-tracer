// The callback compiler supplies Call, State and shared_state.
// This file is compiled independently for each function we trace.

struct RequestScope {};

// One state object belongs to the outer request. The tracer propagates this
// shared scope to threads created while that request is active.
struct RequestState : shared_state<RequestScope> {
  int request_id = 0;
  int calls = 0;
  int last_result = 0;
};

struct Interceptor {
  // This declaration asks the engine to construct or reuse RequestState.
  // The engine supplies the constructor; do not add a definition.
  Interceptor(State<RequestState>);

  // State parameters are injected and guarded by the shared-context engine.
  void pre(const Call& call, RequestState& state) {
    if (state.calls == 0) {
      state.request_id = call.arg<int>(0); // First entry is process_request.
    }
    ++state.calls; // Outer PRE: 1. Worker PRE: 2, using the same state.
  }

  void post(const Call& call, RequestState& state) {
    const int result = call.result<int>();

    // Add one named scalar to this hook's call metadata.
    call.field("hook_result", result);

    // Save the result in the state shared by the outer and worker probes.
    state.last_result = result;
    // Publish a typed snapshot. The CLI decodes it without a hand-written schema.
    // --callback-print-arguments true includes the call fields recorded above.
    // Worker POST publishes 2 * request_id; outer POST publishes that value + 1.
    call.context().event("request.step");
  }
};

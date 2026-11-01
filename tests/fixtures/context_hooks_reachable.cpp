namespace engine { struct user_symbol {}; }

struct HookScope {};
struct HookState : shared_state<HookScope> { int value = 0; };

struct HookInterceptor {
  HookInterceptor(State<HookState> request);

  void helper(const Call& call) {
    call.context().get<HookState>().value++;
    call.context().event("changed");
  }
  void pre(const Call& call) { helper(call); }
};

struct ContextOnly {
  void helper(const Call& call) {
    call.context().with<HookState>([](HookState& state) { ++state.value; });
  }
  void post(const Call& call) { helper(call); }
};

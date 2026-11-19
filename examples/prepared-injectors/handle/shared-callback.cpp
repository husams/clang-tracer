// A callback with shared state takes the shared-probe install path.
struct SnapshotScope {};
struct SnapshotState : state<SnapshotScope> {
  int calls = 0;
};
struct Interceptor {
  Interceptor(State<SnapshotState>);
  void post(const Call &, SnapshotState &state) { ++state.calls; }
};

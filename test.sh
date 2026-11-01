#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

run_dir=$(mktemp -d /tmp/clang-tracer-demo.XXXXXX)
export CLANG_TRACER_ENDPOINT="unix://$run_dir/control.sock"

# Start the server.
./build/clang-tracer-server \
  --agent "$PWD/build/libclang_tracer_agent.so" \
  --listen "$CLANG_TRACER_ENDPOINT" >"$run_dir/server.log" 2>&1 &
server_pid=$!

# Start the sample C++ application.
./build/trace-target >"$run_dir/target.log" 2>&1 &
target_pid=$!
trap 'kill "$server_pid" "$target_pid" 2>/dev/null || true' EXIT

# Wait for readiness.
for attempt in {1..100}; do
  if [ -s "$run_dir/target.log" ] &&
     ./build/clang-tracer list >/dev/null 2>&1; then
    break
  fi
  sleep 0.1
done

# Create the native C++ callback file.
cat > "$run_dir/callback.cpp" <<'CPP'
extern "C" long long trace_callback(const TraceContext& context) {
  return context.arguments[0].signed_value * 100 + 17;
}
CPP

# Attach by PID.
./build/clang-tracer attach "$target_pid"

# Clang resolves the method and signature from the original source.
./build/clang-tracer trace "$target_pid" 'Fixture::overloaded(int)' \
  --source tests/fixtures/target.cpp \
  --callback "$run_dir/callback.cpp"

# Display five readable events.
./build/clang-tracer events "$target_pid" --count 5

# Restore the method and detach.
./build/clang-tracer detach "$target_pid"

#!/usr/bin/env bash
# Ordinary CLI verification against an existing debug binary; no target build.
set -euo pipefail
run_dir=$1
target_binary=$2
selector=$3
shift 3
mkdir -p "$run_dir"
export CLANG_TRACER_ENDPOINT="unix://$run_dir/control.sock"
target_pid=""; server_pid=""; attached=""
cleanup() {
  status=$?; trap - EXIT
  if [[ -n "$attached" ]]; then
    if ! ./build/clang-tracer detach "$attached"; then
      echo "Preserving target=$target_pid server=$server_pid for recovery"; exit 1
    fi
  fi
  if [[ -n "$target_pid" ]]; then kill -TERM "$target_pid"; wait "$target_pid" || true; fi
  if [[ -n "$server_pid" ]]; then kill -TERM "$server_pid"; wait "$server_pid" || true; fi
  exit "$status"
}
trap cleanup EXIT
exec > >(tee "$run_dir/direct-cli.txt") 2>&1
sha256sum "$target_binary" > "$run_dir/hash.before"
"$target_binary" > "$run_dir/target.log" 2>&1 &
target_pid=$!
./build/clang-tracer-server --agent "$PWD/build/libclang_tracer_agent.so" \
  --listen "$CLANG_TRACER_ENDPOINT" --timeout 30 > "$run_dir/server.log" 2>&1 &
server_pid=$!
for attempt in $(seq 1 100); do
  ./build/clang-tracer list >/dev/null 2>&1 && break
  sleep 0.1
done
attached=$target_pid
./build/clang-tracer attach "$target_pid"
./build/clang-tracer trace "$target_pid" "$selector" \
  --callback examples/call-stacks/symbolized.cpp --stack 24 \
  --callback-print-arguments true --callback-print-result true "$@"
./build/clang-tracer events "$target_pid" --count 6 --timeout 15 | tee "$run_dir/events.log"
./build/clang-tracer --format protobuf --output "$run_dir/events.pb" events "$target_pid" --count 2 --timeout 15
before=$(grep -Ec '^(CALL |TARGET )' "$run_dir/target.log")
./build/clang-tracer detach "$target_pid"
attached=""
sleep 1
after=$(grep -Ec '^(CALL |TARGET )' "$run_dir/target.log")
tracer_pid=$(awk '/^TracerPid:/ {print $2}' "/proc/$target_pid/status")
[[ "$after" -gt "$before" && "$tracer_pid" == 0 ]]
sha256sum --check "$run_dir/hash.before"
if grep -Eq 'failures=[1-9]' "$run_dir/target.log"; then exit 1; fi
echo "After detach: target rows $before -> $after, TracerPid=$tracer_pid"

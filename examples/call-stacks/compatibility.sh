#!/usr/bin/env bash
# Ordinary CLI check of the existing typed-argument/header/database workflow.
set -euo pipefail
run_dir=${1:-$(mktemp -d /tmp/clang-tracer-stack-compat.XXXXXX)}
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
./build/order-arguments-target > "$run_dir/target.log" 2>&1 &
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
./build/clang-tracer trace "$target_pid" \
  'total(Order&, double, const Coupon&, Order, int)' \
  --callback examples/order-arguments/interceptor.cpp \
  --include examples/order-arguments/order.hpp --compdb build --stack 16 | tee "$run_dir/probe.txt"
./build/clang-tracer events "$target_pid" --count 6 --timeout 15 | tee "$run_dir/total-events.log"
probe_id=$(sed -n 's/.* probe=\([0-9]*\) .*/\1/p' "$run_dir/probe.txt")
cursor=$(sed -n 's/^cursor=\([0-9]*\) .*/\1/p' "$run_dir/total-events.log" | tail -1)
./build/clang-tracer remove "$target_pid" "$probe_id"
./build/clang-tracer trace "$target_pid" 'pointer_total(Order*, int)' \
  --callback examples/order-arguments/pointer.cpp --stack 16
./build/clang-tracer events "$target_pid" --count 10 --after "$cursor" --timeout 15 | tee "$run_dir/pointer-events.log"
before=$(grep -c '^CALL ' "$run_dir/target.log")
./build/clang-tracer detach "$target_pid"
attached=""
sleep 1
after=$(grep -c '^CALL ' "$run_dir/target.log")
tracer_pid=$(awk '/^TracerPid:/ {print $2}' "/proc/$target_pid/status")
[[ "$after" -gt "$before" && "$tracer_pid" == 0 ]]
if grep -Eq 'failures=[1-9]' "$run_dir/target.log"; then exit 1; fi
echo "Typed arguments/header/compdb: calls $before -> $after, TracerPid=$tracer_pid, failures=0"

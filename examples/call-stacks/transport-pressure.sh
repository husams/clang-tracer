#!/usr/bin/env bash
# Public CLI proof: the target completes a batch while every server thread is
# SIGSTOPed. Resume, detach immediately, then read the retained binary events.
set -euo pipefail
run_dir=${1:?Provide a fresh absolute directory}
count=${2:-1500}
[[ "$run_dir" == /* && ! -e "$run_dir" ]]
mkdir -p "$run_dir"
target_pid=""; server_pid=""; attached=""; paused=""
cleanup() {
  status=$?; trap - EXIT
  if [[ -n "$paused" ]]; then kill -CONT "$server_pid"; fi
  if [[ -n "$attached" ]]; then
    if ! ./build/clang-tracer detach "$attached" >> "$run_dir/cleanup.log" 2>&1; then
      echo "Detach needs recovery; preserving target=$target_pid server=$server_pid"
      exit 1
    fi
  fi
  if [[ -n "$target_pid" ]]; then kill -TERM "$target_pid"; wait "$target_pid" || true; fi
  if [[ -n "$server_pid" ]]; then kill -TERM "$server_pid"; wait "$server_pid" || true; fi
  exit "$status"
}
trap cleanup EXIT
exec > >(tee "$run_dir/direct-cli.txt") 2>&1
export CLANG_TRACER_ENDPOINT="unix://$run_dir/control.sock"
mkfifo "$run_dir/input" "$run_dir/output"
exec 5<>"$run_dir/input" 6<>"$run_dir/output"
taskset -c 2 ./build/call-stacks-benchmark <&5 >&6 5<&- 6>&- \
  2> "$run_dir/target-stderr.log" &
target_pid=$!
IFS= read -r -t 10 ready <&6
[[ "$ready" == "READY $target_pid" ]]
echo "$ready"
taskset -c 3 ./build/clang-tracer-server --agent "$PWD/build/libclang_tracer_agent.so" \
  --listen "$CLANG_TRACER_ENDPOINT" --timeout 30 5<&- 6>&- \
  > "$run_dir/server.log" 2>&1 &
server_pid=$!
for attempt in $(seq 1 100); do
  ./build/clang-tracer list >/dev/null 2>&1 && break
  sleep 0.05
done
attached=$target_pid
./build/clang-tracer attach "$target_pid" | tee "$run_dir/attach.log"
stack_args=()
if [[ -n "${PRESSURE_STACK_DEPTH:-}" ]]; then
  stack_args=(--stack "$PRESSURE_STACK_DEPTH" --stack-application-only true)
fi
./build/clang-tracer trace "$target_pid" 'stack_add(int, int)' \
  --callback examples/call-stacks/automatic.cpp "${stack_args[@]}"
printf '100\n' >&5
IFS= read -r -t 10 warm <&6
[[ "$warm" =~ ^RESULT,100,[0-9]+,[0-9]+,[0-9]+,1$ ]]
./build/clang-tracer events "$target_pid" --count 200 --timeout 10 > "$run_dir/warm-events.log"
kill -STOP "$server_pid"
paused=1
for attempt in $(seq 1 100); do
  [[ $(awk '/^State:/ {print $2}' "/proc/$server_pid/status") == T ]] && break
  sleep 0.01
done
[[ $(awk '/^State:/ {print $2}' "/proc/$server_pid/status") == T ]]
printf '%s\n' "$count" >&5
IFS= read -r -t 5 result <&6
[[ "$result" =~ ^RESULT,$count,[0-9]+,[0-9]+,[0-9]+,1$ ]]
[[ $(awk '/^State:/ {print $2}' "/proc/$server_pid/status") == T ]]
echo "Producer returned while server remained SIGSTOPed: $result"
printf '%s\n' "$result" > "$run_dir/paused-result.txt"
kill -CONT "$server_pid"
paused=""
# Detach before reading/draining the event stream: stop must flush the queued
# sends and the server must publish them before unregistering the probe.
./build/clang-tracer --format protobuf --output "$run_dir/detach.pb" detach "$target_pid"
attached=""
tail -c +5 "$run_dir/detach.pb" | protoc -Iproto \
  --decode=clangtracer.v1.Session proto/tracer.proto > "$run_dir/detach.txt"
grep -q 'original_bytes_restored: true' "$run_dir/detach.txt"
! grep -q 'callback_errors:' "$run_dir/detach.txt"
dropped=$(awk '/agent_dropped_events:/ {print $2}' "$run_dir/detach.txt")
dropped=${dropped:-0}
expected=$(((100 + count) * 2))
received=$((expected - dropped))
retained=$received
if [[ "$retained" -gt 4096 ]]; then retained=4096; fi
session=$(sed -n 's/.* session=\([^ ]*\) .*/\1/p' "$run_dir/attach.log")
printf 'session_ids: "%s"\n' "$session" > "$run_dir/watch.textproto"
./build/clang-tracer watch --request "$run_dir/watch.textproto" \
  --output "$run_dir/events.pb" --count "$retained" --timeout 15
[[ $(awk '/^TracerPid:/ {print $2}' "/proc/$target_pid/status") == 0 ]]
printf '10\n' >&5
IFS= read -r -t 10 restored <&6
[[ "$restored" =~ ^RESULT,10,[0-9]+,[0-9]+,[0-9]+,1$ ]]
echo "After detach: $restored, TracerPid=0"
printf 'expected,received,agent_dropped,retained\n%s,%s,%s,%s\n' \
  "$expected" "$received" "$dropped" "$retained" > "$run_dir/event-counts.csv"
cat "$run_dir/event-counts.csv"
printf '0\n' >&5
wait "$target_pid"
target_pid=""
kill -TERM "$server_pid"
wait "$server_pid"
server_pid=""
exec 5>&- 6>&-
rm "$run_dir/input" "$run_dir/output"

#!/usr/bin/env bash
# Run on the configured Linux checkout. Times exclude startup/attach/JIT and
# callback stdout. Each fresh process uses the identical benchmark/library.
set -euo pipefail
run_dir=${1:?Provide a fresh absolute evidence directory}
rounds=${BENCH_ROUNDS:-9}
target_cpu=${BENCH_TARGET_CPU:-2}
server_cpu=${BENCH_SERVER_CPU:-3}
[[ "$run_dir" == /* && ! -e "$run_dir" ]]
mkdir -p "$run_dir"
target_pid=""; server_pid=""; attached=""
cleanup() {
  status=$?; trap - EXIT
  if [[ -n "$attached" ]]; then
    if ! ./build/clang-tracer detach "$attached" >> "$run_dir/cleanup.log" 2>&1; then
      echo "Detach needs recovery; preserving target=$target_pid server=$server_pid"
      exit 1
    fi
  fi
  if [[ -n "$target_pid" ]]; then
    kill -TERM "$target_pid" 2>/dev/null || true
    wait "$target_pid" || true
  fi
  if [[ -n "$server_pid" ]]; then
    kill -TERM "$server_pid" 2>/dev/null || true
    wait "$server_pid" || true
  fi
  exit "$status"
}
trap cleanup EXIT
cli() {
  printf '%q ' ./build/clang-tracer "$@" >> "$run_dir/commands.log"
  printf '\n' >> "$run_dir/commands.log"
  ./build/clang-tracer "$@"
}
batch() {
  printf '%s\n' "$1" >&5
  IFS= read -r -t 60 batch_result <&6
  [[ "$batch_result" =~ ^RESULT,$1,[0-9]+,[0-9]+,[0-9]+,1$ ]]
  printf '%s\n' "$batch_result" >> "$trial/target.log"
}
sha256sum build/call-stacks-benchmark build/libcall-stacks-calculator.so \
  build/clang-tracer build/clang-tracer-server build/libclang_tracer_agent.so \
  > "$run_dir/hashes.before"
{
  uname -srmo
  lscpu
  clang++ --version
  grep -E 'CMAKE_BUILD_TYPE:|CMAKE_CXX_FLAGS(:|_)' build/CMakeCache.txt
  printf 'target_cpu=%s server_cpu=%s rounds=%s\n' "$target_cpu" "$server_cpu" "$rounds"
} > "$run_dir/environment.txt"
printf 'round,mode,calls,wall_ns,cpu_ns,checksum,valid\n' > "$run_dir/measurements.csv"
printf 'round,mode,expected_events,received_events,agent_dropped_events\n' > "$run_dir/event-counts.csv"
for ((round=1; round<=rounds; ++round)); do
  # Rotate order to spread warm-host and time-of-run effects across modes.
  case $((round % 3)) in
    1) modes=(off no-stack app-stack) ;;
    2) modes=(no-stack app-stack off) ;;
    0) modes=(app-stack off no-stack) ;;
  esac
  for mode in "${modes[@]}"; do
    trial="$run_dir/round-$round-$mode"
    mkdir "$trial"
    mkfifo "$trial/input" "$trial/output"
    exec 5<>"$trial/input" 6<>"$trial/output"
    taskset -c "$target_cpu" ./build/call-stacks-benchmark <&5 >&6 \
      5<&- 6>&- 2>"$trial/target-stderr.log" &
    target_pid=$!
    IFS= read -r -t 10 ready <&6
    [[ "$ready" == "READY $target_pid" ]]
    printf '%s\n' "$ready" > "$trial/target.log"
    export CLANG_TRACER_ENDPOINT="unix://$trial/control.sock"
    taskset -c "$server_cpu" ./build/clang-tracer-server \
      --agent "$PWD/build/libclang_tracer_agent.so" \
      --listen "$CLANG_TRACER_ENDPOINT" --timeout 30 \
      5<&- 6>&- > "$trial/server.log" 2>&1 &
    server_pid=$!
    for attempt in $(seq 1 100); do
      ./build/clang-tracer list >/dev/null 2>&1 && break
      kill -0 "$server_pid"
      sleep 0.05
    done
    if [[ "$mode" != off ]]; then
      attached=$target_pid
      cli attach "$target_pid" > "$trial/attach.log"
      stack_args=()
      if [[ "$mode" == app-stack ]]; then
        stack_args=(--stack 24 --stack-application-only true)
      fi
      cli trace "$target_pid" 'stack_add(int, int)' \
        --callback examples/call-stacks/automatic.cpp "${stack_args[@]}" \
        > "$trial/trace.log"
    fi
    # Warm JIT execution, CFI/images and symbol caches outside the measurement.
    batch 100
    if [[ "$mode" == off ]]; then count=5000000; else count=1500; fi
    batch "$count"
    measured_result=$batch_result
    printf '%s,%s,%s\n' "$round" "$mode" "${batch_result#RESULT,}" \
      >> "$run_dir/measurements.csv"
    if [[ "$mode" != off ]]; then
      # 100 warmup + 1500 timed calls = 3200 events, below the 4096 history
      # bound. Record all received events and the final authoritative agent
      # counter; hot-loop drops are a result, not a successful lossless run.
      if ! cli events "$target_pid" --count 3200 --timeout 2 \
          > "$trial/events.log" 2> "$trial/events-status.log"; then
        grep -q 'events timed out after receiving' "$trial/events-status.log"
      fi
      if [[ "$mode" == app-stack ]]; then
        grep -q 'entry-stack COMPLETE.*application-only=true' "$trial/events.log"
        ! grep 'entry-stack' "$trial/events.log" | grep -v 'COMPLETE.*application-only=true'
      else
        ! grep -q 'entry-stack' "$trial/events.log"
      fi
      cli --format protobuf --output "$trial/detach.pb" detach "$target_pid"
      attached=""
      tail -c +5 "$trial/detach.pb" | protoc -Iproto \
        --decode=clangtracer.v1.Session proto/tracer.proto > "$trial/detach.txt"
      grep -q 'original_bytes_restored: true' "$trial/detach.txt"
      ! grep -q 'callback_errors:' "$trial/detach.txt"
      received=$(grep -c '^cursor=' "$trial/events.log")
      dropped=$(awk '/agent_dropped_events:/ {print $2}' "$trial/detach.txt")
      dropped=${dropped:-0}
      [[ $((received + dropped)) == 3200 ]]
      ! grep 'lost(agent=' "$trial/events.log" | grep -v 'server=0)'
      printf '%s,%s,3200,%s,%s\n' "$round" "$mode" "$received" "$dropped" \
        >> "$run_dir/event-counts.csv"
    fi
    [[ $(awk '/^TracerPid:/ {print $2}' "/proc/$target_pid/status") == 0 ]]
    batch 10 # Original results/progress after detach.
    printf '0\n' >&5
    wait "$target_pid"
    target_pid=""
    kill -TERM "$server_pid"
    wait "$server_pid"
    server_pid=""
    exec 5>&- 6>&-
    rm "$trial/input" "$trial/output"
    printf 'round=%s mode=%s %s\n' "$round" "$mode" "$measured_result"
  done
done
sha256sum --check "$run_dir/hashes.before" > "$run_dir/hashes.after"
printf 'Completed %s rounds; timing rows: %s\n' "$rounds" "$run_dir/measurements.csv"

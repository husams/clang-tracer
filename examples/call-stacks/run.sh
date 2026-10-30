#!/usr/bin/env bash
# Run from the project root after building clang-tracer, clang-tracer-server,
# clang_tracer_agent and call-stacks-target. Uses ordinary public CLI commands.
set -euo pipefail
run_dir=${1:-$(mktemp -d /tmp/clang-tracer-call-stacks.XXXXXX)}
mode=${2:-complete}
target_binary=${STACK_TARGET_BINARY:-./build/call-stacks-target}
filter_args=()
if [[ -n "${STACK_APPLICATION_ONLY+x}" ]]; then
  filter_args=(--stack-application-only "$STACK_APPLICATION_ONLY")
fi
print_args=()
if [[ -n "${STACK_PRINT_ARGUMENTS+x}" ]]; then
  print_args+=(--callback-print-arguments "$STACK_PRINT_ARGUMENTS")
fi
if [[ -n "${STACK_PRINT_RESULT+x}" ]]; then
  print_args+=(--callback-print-result "$STACK_PRINT_RESULT")
fi
target_args=(--wait)
stack_args=(--stack 24)
function_callback=${STACK_FUNCTION_CALLBACK:-examples/call-stacks/function.cpp}
method_callback=${STACK_METHOD_CALLBACK:-examples/call-stacks/method.cpp}
case "$mode" in
  complete) ;;
  partial) target_args+=(--partial) ;;
  truncated) stack_args=(--stack 2); function_callback=${STACK_FUNCTION_CALLBACK:-examples/call-stacks/automatic.cpp} ;;
  maximum) stack_args=(--stack 64); function_callback=${STACK_FUNCTION_CALLBACK:-examples/call-stacks/automatic.cpp} ;;
  bridge-limit) stack_args=(--stack 3); function_callback=${STACK_FUNCTION_CALLBACK:-examples/call-stacks/automatic.cpp} ;;
  disabled) stack_args=(); function_callback=${STACK_FUNCTION_CALLBACK:-examples/call-stacks/automatic.cpp} ;;
  *) echo "Unknown mode: $mode" >&2; exit 2 ;;
esac
function_stack_args=("${stack_args[@]}")
if [[ "$function_callback" == examples/call-stacks/function.cpp ||
      "${STACK_FUNCTION_CAPTURE:-}" == callback ]]; then
  function_stack_args=() # The callback requests capture without a CLI flag.
fi
mkdir -p "$run_dir"
export CLANG_TRACER_ENDPOINT="unix://$run_dir/control.sock"
server_pid=""
target_pid=""
attached=""
cleanup() {
  status=$?
  trap - EXIT
  for source in calculator.cpp calculator.hpp target.cpp no_cfi.cpp; do
    if [[ -f "$run_dir/$source.unavailable" ]]; then
      mv "$run_dir/$source.unavailable" "examples/call-stacks/$source"
    fi
  done
  if [[ -f "$run_dir/compile_commands.unavailable" ]]; then
    mv "$run_dir/compile_commands.unavailable" build/compile_commands.json
  fi
  if [[ -n "$attached" ]]; then
    if ! ./build/clang-tracer detach "$attached"; then
      echo "Detach needs recovery; preserving target=$target_pid server=$server_pid logs=$run_dir"
      exit 1
    fi
  fi
  if [[ -n "$target_pid" ]]; then
    kill -TERM "$target_pid" 2>/dev/null || true
    wait "$target_pid" 2>/dev/null || true
  fi
  if [[ -n "$server_pid" ]]; then
    kill -TERM "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
  fi
  echo "Logs: $run_dir"
  exit "$status"
}
trap cleanup EXIT
exec > >(tee "$run_dir/direct-cli.txt") 2>&1
sha256sum "$target_binary" build/libcall-stacks-calculator.so > "$run_dir/hashes.before"
if [[ -n "${STACK_TARGET_PID:-}" ]]; then
  target_pid=$STACK_TARGET_PID
  kill -0 "$target_pid"
else
  "$target_binary" "${target_args[@]}" > "$run_dir/target.log" 2>&1 &
  target_pid=$!
fi
echo "Existing debug target PID=$target_pid (no target changes after startup)"
if [[ "${STACK_HIDE_SOURCES:-0}" == 1 ]]; then
  for source in calculator.cpp calculator.hpp target.cpp no_cfi.cpp; do
    mv "examples/call-stacks/$source" "$run_dir/$source.unavailable"
  done
  mv build/compile_commands.json "$run_dir/compile_commands.unavailable"
  echo 'Target sources, header and compilation database unavailable; no includes supplied'
fi
./build/clang-tracer-server --agent "$PWD/build/libclang_tracer_agent.so" \
  --listen "$CLANG_TRACER_ENDPOINT" --timeout 30 > "$run_dir/server.log" 2>&1 &
server_pid=$!
for attempt in $(seq 1 100); do
  ./build/clang-tracer list >/dev/null 2>&1 && break
  kill -0 "$server_pid"
  sleep 0.1
done
attached=$target_pid
./build/clang-tracer attach "$target_pid"
./build/clang-tracer trace "$target_pid" 'StackCalculator::multiply(int, int)' \
  --callback "$method_callback" "${stack_args[@]}" "${filter_args[@]}" "${print_args[@]}"
./build/clang-tracer trace "$target_pid" 'stack_add(int, int)' \
  --callback "$function_callback" "${function_stack_args[@]}" "${filter_args[@]}" "${print_args[@]}"
if ./build/clang-tracer trace "$target_pid" 'stack_add(int, int)' \
    --callback "$function_callback" --stack 65 > "$run_dir/invalid-depth.log" 2>&1; then
  echo 'Unexpected acceptance of depth 65'; exit 1
fi
cat "$run_dir/invalid-depth.log"
kill -USR1 "$target_pid"
./build/clang-tracer events "$target_pid" --count 24 --timeout 15 | tee "$run_dir/events.log"
before=$(grep -c '^CALL ' "$run_dir/target.log")
./build/clang-tracer detach "$target_pid"
attached=""
sleep 1
after=$(grep -c '^CALL ' "$run_dir/target.log")
tracer_pid=$(awk '/^TracerPid:/ {print $2}' "/proc/$target_pid/status")
[[ "$after" -gt "$before" && "$tracer_pid" == 0 ]]
if grep -Eq 'failures=[1-9]' "$run_dir/target.log"; then exit 1; fi
sha256sum --check "$run_dir/hashes.before"
echo "After detach: calls $before -> $after, TracerPid=$tracer_pid, failures=0"
tail -3 "$run_dir/target.log"

#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${1:-"$repo_root/build"}"
if [[ $# -gt 1 ]]; then
  echo "usage: $0 [build-directory]" >&2
  exit 2
fi
if [[ "$(uname -s)" != Linux || "$(uname -m)" != x86_64 ]]; then
  echo "This example currently requires Linux x86_64." >&2
  exit 2
fi

build_dir="$(cd "$build_dir" && pwd)"
client="$build_dir/clang-tracer"
server="$build_dir/clang-tracer-server"
agent="$build_dir/libclang_tracer_agent.so"
for executable in "$client" "$server"; do
  if [[ ! -x "$executable" ]]; then
    echo "Required executable is missing: $executable" >&2
    exit 1
  fi
done
if [[ ! -f "$agent" ]]; then
  echo "Required build output is missing: $agent" >&2
  exit 1
fi

cmake --build "$build_dir" --target cpp-interceptors-target
target="$build_dir/cpp-interceptors-target"
if [[ ! -x "$target" ]]; then
  echo "Target build did not create $target" >&2
  exit 1
fi

run_dir="$(mktemp -d "${TMPDIR:-/tmp}/clang-tracer-cpp-interceptors.XXXXXX")"
chmod 700 "$run_dir"
endpoint="unix://$run_dir/control.sock"
server_pid=""
target_pid=""
attached=0
preserve_processes=0

stop_owned_process() {
  local pid="$1"
  if [[ -z "$pid" ]] || ! kill -0 "$pid" 2>/dev/null; then
    wait "$pid" 2>/dev/null || true
    return
  fi

  kill -TERM "$pid" 2>/dev/null || true
  for _ in $(seq 1 50); do
    if ! kill -0 "$pid" 2>/dev/null; then
      wait "$pid" 2>/dev/null || true
      return
    fi
    sleep 0.1
  done

  kill -KILL "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
}

stop_owned_processes() {
  stop_owned_process "$target_pid"
  stop_owned_process "$server_pid"
}

cleanup() {
  status=$?
  trap - EXIT INT TERM
  if [[ "$attached" == 1 ]]; then
    if CLANG_TRACER_ENDPOINT="$endpoint" "$client" detach "$target_pid"; then
      attached=0
    else
      preserve_processes=1
      echo "Detach failed; leaving the demo target and server running for recovery." >&2
      echo "Endpoint: $endpoint" >&2
      echo "Target PID: $target_pid; server PID: $server_pid" >&2
    fi
  fi
  if [[ "$preserve_processes" == 0 ]]; then
    stop_owned_processes
  else
    echo "Logs and socket are in $run_dir" >&2
    if [[ "$status" == 0 ]]; then
      status=1
    fi
    exit "$status"
  fi
  echo "Demo logs: $run_dir"
  exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

"$server" --agent "$agent" --listen "$endpoint" >"$run_dir/server.log" 2>&1 &
server_pid=$!
for _ in $(seq 1 100); do
  if [[ -S "$run_dir/control.sock" ]]; then
    break
  fi
  if ! kill -0 "$server_pid" 2>/dev/null; then
    echo "Demo server stopped during startup; see $run_dir/server.log" >&2
    exit 1
  fi
  sleep 0.05
done
if [[ ! -S "$run_dir/control.sock" ]]; then
  echo "Demo server did not create its socket; see $run_dir/server.log" >&2
  exit 1
fi

"$target" >"$run_dir/target.log" 2>&1 &
target_pid=$!
for _ in $(seq 1 50); do
  if grep -q '^TARGET pid=' "$run_dir/target.log"; then
    break
  fi
  if ! kill -0 "$target_pid" 2>/dev/null; then
    echo "Demo target stopped during startup; see $run_dir/target.log" >&2
    exit 1
  fi
  sleep 0.05
done
if ! grep -q '^TARGET pid=' "$run_dir/target.log"; then
  echo "Demo target did not report its PID; see $run_dir/target.log" >&2
  exit 1
fi

export CLANG_TRACER_ENDPOINT="$endpoint"
echo "Attaching to demo PID $target_pid"
attached=1
"$client" attach "$target_pid"

echo "Installing free-function interceptor for add(int, int)"
free_trace_output=$("$client" trace "$target_pid" 'add(int, int)' \
  --callback "$repo_root/examples/cpp-interceptors/free_function.cpp")
echo "$free_trace_output"
if [[ "$free_trace_output" =~ probe=([0-9]+) ]]; then
  free_probe="${BASH_REMATCH[1]}"
else
  echo "Could not read the free-function probe ID from the client output." >&2
  exit 1
fi

echo "Installing typed-this const-method interceptor for Calculator::multiply(int, int)"
method_trace_output=$("$client" trace "$target_pid" 'Calculator::multiply(int, int) const' \
  --callback "$repo_root/examples/cpp-interceptors/method.cpp")
echo "$method_trace_output"
if [[ "$method_trace_output" =~ probe=([0-9]+) ]]; then
  method_probe="${BASH_REMATCH[1]}"
else
  echo "Could not read the typed-this method probe ID from the client output." >&2
  exit 1
fi

echo "Reading events until both probes show PRE and POST (up to 60 seconds)"
cursor=0
free_pre=0
free_post=0
method_pre=0
method_post=0
for _ in $(seq 1 6); do
  batch=$("$client" events "$target_pid" --after "$cursor" --count 10 --timeout 10)
  printf '%s\n' "$batch"
  while IFS= read -r line; do
    if [[ "$line" =~ cursor=([0-9]+) ]]; then
      event_cursor="${BASH_REMATCH[1]}"
      if (( event_cursor > cursor )); then
        cursor="$event_cursor"
      fi
    fi
    if [[ "$line" =~ probe=([0-9]+).*phase=(pre|post) ]]; then
      event_probe="${BASH_REMATCH[1]}"
      event_phase="${BASH_REMATCH[2]}"
      if [[ "$event_probe" == "$free_probe" ]]; then
        if [[ "$event_phase" == pre ]]; then free_pre=1; else free_post=1; fi
      elif [[ "$event_probe" == "$method_probe" ]]; then
        if [[ "$event_phase" == pre ]]; then method_pre=1; else method_post=1; fi
      fi
    fi
  done <<< "$batch"
  if [[ "$free_pre" == 1 && "$free_post" == 1 &&
        "$method_pre" == 1 && "$method_post" == 1 ]]; then
    break
  fi
done
if [[ "$free_pre" != 1 || "$free_post" != 1 ||
      "$method_pre" != 1 || "$method_post" != 1 ]]; then
  echo "Timed out waiting for PRE and POST events from both probes." >&2
  echo "Seen: free PRE=$free_pre POST=$free_post; method PRE=$method_pre POST=$method_post." >&2
  exit 1
fi

echo "Detaching while the target is still running"
"$client" detach "$target_pid"
attached=0
if ! kill -0 "$target_pid" 2>/dev/null; then
  echo "Target exited before the detach check." >&2
  exit 1
fi
echo "Detach succeeded; target PID $target_pid is still alive."

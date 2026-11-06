#!/usr/bin/env bash
set -euo pipefail

script=$(readlink -f -- "${BASH_SOURCE[0]}")
root=$(cd -- "$(dirname -- "$script")/../.." && pwd)
build=$(realpath -m -- "${CLANG_TRACER_PREPARED_BUILD:-$root/build}")
repl=${CLANG_TRACER_REPL:-$build/clang-tracer-repl}
server=${CLANG_TRACER_SERVER:-$build/clang-tracer-server}
agent=${CLANG_TRACER_AGENT:-$build/libclang_tracer_agent.so}
target=${CLANG_TRACER_PREPARED_TARGET:-$build/examples/prepared-injectors/prepared-injectors-target}
compdb=${CLANG_TRACER_PREPARED_COMPDB:-$build/compile_commands.json}

if [[ ${1:-} == --help ]]; then
  printf '%s\n' 'Run without arguments for the native C++ REPL, or use --smoke for an automated demonstration.'
  exit 0
fi
if [[ $# -gt 1 || ( $# == 1 && $1 != --smoke ) ]]; then
  printf '%s\n' 'Usage: run-native.sh [--smoke|--help]' >&2
  exit 2
fi
for required in "$repl" "$server" "$agent" "$target" "$compdb"; do
  [[ -f $required ]] || { printf 'Missing demo input: %s\n' "$required" >&2; exit 1; }
done

run=$(mktemp -d "${TMPDIR:-/tmp}/native-prepared-demo.XXXXXXXX")
ln -s "$root/examples/prepared-injectors" "$run/demo"
ln -s "$(realpath -- "$compdb")" "$run/compile_commands.json"
"$target" >"$run/target.log" 2>&1 &
target_pid=$!
cleanup() {
  kill "$target_pid" 2>/dev/null || true
  wait "$target_pid" 2>/dev/null || true
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM HUP
cd -- "$run"
unset CLANG_TRACER_ENDPOINT

printf 'Native C++ REPL example: %s\nTarget PID: %s\n' "$run" "$target_pid"
if [[ ${1:-} == --smoke ]]; then
  "$repl" --server "$server" --agent "$agent" --attach "$target_pid" --timeout 60 --fail-fast <<'REPL'
%probe prepare "prepared_demo::compute(int)" --include demo/calculator.hpp --output prepared
%probe resolve "prepared_demo::compute(int)"
%probe validate "prepared_demo::compute(int)" --callback demo/interceptor.cpp
%probe inject "prepared_demo::compute(int)" --callback demo/interceptor.cpp --print-arguments --print-result
%collect --count 4 --timeout 15 --phase POST --as captured
auto captured = tracer.batch("captured");
std::cout << "NATIVE_EVENT_COUNT=" << captured.size() << std::endl;
bool correct = captured.size() == 4;
for (const auto& e : captured) { int value = e.argument_as<int>(0); correct = correct && e.result_as<int>() == value * 3 + 7; }
std::cout << "NATIVE_RESULTS=" << correct << std::endl;
tracer.detach(session);
%quit
REPL
else
  cat <<'STEPS'
Paste each command at the native tracer prompt:

%probe prepare "prepared_demo::compute(int)" --include demo/calculator.hpp --output prepared
%probe validate "prepared_demo::compute(int)" --callback demo/interceptor.cpp
%probe inject "prepared_demo::compute(int)" --callback demo/interceptor.cpp --print-arguments --print-result
%collect --count 4 --timeout 15 --phase POST --as captured
auto captured = tracer.batch("captured");
captured.size()
tracer.print(captured.at(0))
%probe remove PROBE_ID
%quit

Generated files: prepared/prepared.pb and prepared/generated/adapter.hpp, adapter.cpp.
Replace PROBE_ID with the number reported by injection. The callback stays
in demo/interceptor.cpp. Use %quit to restore all hooks and exit.
STEPS
  "$repl" --server "$server" --agent "$agent" --attach "$target_pid" --timeout 60
fi
printf 'Example files retained in %s\n' "$run"

#!/usr/bin/env bash
set -euo pipefail

usage() {
  printf '%s\n' 'Usage: run.sh BUILD_DIR [--external-server] [--plain]' >&2
  exit 2
}
[[ $# -ge 1 ]] || usage
build=$(realpath -- "$1")
shift
external=0
plain=0
for option in "$@"; do
  case $option in
    --external-server) external=1 ;;
    --plain) plain=1 ;;
    *) usage ;;
  esac
done

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
dir=$(mktemp -d "${TMPDIR:-/tmp}/handle-return.XXXXXXXX")
target_pid=
server_pid=
cleanup() {
  [[ -z $target_pid ]] || kill "$target_pid" 2>/dev/null || true
  [[ -z $server_pid ]] || kill "$server_pid" 2>/dev/null || true
  wait 2>/dev/null || true
  rm -rf -- "$dir"
}
trap cleanup EXIT
trap 'exit 130' INT

for source in item.hpp item-impl.hpp heap.cpp target.cpp snapshot-policy.hpp inspect.repl; do
  cp -- "$here/$source" "$dir/$source"
done
flags=(-std=c++17 -g -O0 -fno-omit-frame-pointer -fcf-protection=none)
if [[ $plain == 0 ]]; then
  flags+=(-DHANDLE_VIRTUAL_IMPL)
fi
cxx=${CXX:-clang++}
(
  cd -- "$dir"
  "$cxx" "${flags[@]}" -shared -fPIC -Wl,--build-id=sha1 heap.cpp -o libhandle_heap.so
  "$cxx" "${flags[@]}" target.cpp -L. -lhandle_heap '-Wl,-rpath,$ORIGIN' -o target
)
printf '[{"directory":"%s","file":"%s/target.cpp","arguments":["%s",%s,"-c","%s/target.cpp"]}]\n' \
  "$dir" "$dir" "$cxx" \
  "$(printf '"%s",' "${flags[@]}" | sed 's/,$//')" "$dir" >"$dir/compile_commands.json"

"$dir/target" >"$dir/target.log" 2>&1 &
target_pid=$!
ready=0
for _ in $(seq 100); do
  if grep -qx ready "$dir/target.log"; then ready=1; break; fi
  kill -0 "$target_pid" 2>/dev/null || break
  sleep 0.1
done
[[ $ready == 1 ]] || { printf 'target failed to start\n' >&2; exit 1; }

repl_args=()
if [[ $external == 1 ]]; then
  "$build/clang-tracer-server" --listen "unix://$dir/external.sock" \
    --agent "$build/libclang_tracer_agent.so" --timeout 60 >"$dir/server.log" 2>&1 &
  server_pid=$!
  for _ in $(seq 100); do
    [[ -S $dir/external.sock ]] && break
    sleep 0.1
  done
  [[ -S $dir/external.sock ]] || { printf 'server did not listen\n' >&2; exit 1; }
  repl_args=(--endpoint "unix://$dir/external.sock")
else
  repl_args=(--server "$build/clang-tracer-server" --agent "$build/libclang_tracer_agent.so")
fi

script=$dir/inspect.repl
(
  cd -- "$dir"
  env -u CLANG_TRACER_ENDPOINT \
    TRACER_PREPARE_EXECUTABLE="$build/clang-tracer-prepare-injectors-native" \
    XDG_CACHE_HOME="$dir/cache" \
    "$build/clang-tracer-repl" "${repl_args[@]}" --attach "$target_pid" --timeout 60 \
    --fail-fast <"$script"
)

printf '\nExpected output lines:\n'
printf '%s\n' 'ROOT type=HFW::HfwItem fields=2' \
  'IMPL type=HFW::HfwItemImpl revision=827 label=implementation label' \
  'TYPED revision=827 label=implementation label' \
  'HANDLE revision=827 label=implementation label'

#!/usr/bin/env bash
set -euo pipefail

if [[ $# -gt 1 || ( $# == 1 && $1 != --interactive ) ]]; then
  printf 'Usage: %s [--interactive]\n' "$0" >&2
  exit 2
fi
if [[ $(uname -s) != Linux || $(uname -m) != x86_64 ]]; then
  printf 'Run this example on Linux x86-64.\n' >&2
  exit 1
fi

script=$(readlink -f -- "${BASH_SOURCE[0]}")
example_dir=$(dirname -- "$script")
prefix=$(cd -- "$example_dir/../../../.." && pwd)
if [[ -x $prefix/bin/clang-tracer-repl && -x $prefix/bin/clang-tracer-server ]]; then
  repl_default=$prefix/bin/clang-tracer-repl
  server_default=$prefix/bin/clang-tracer-server
else
  repl_default=clang-tracer-repl
  server_default=clang-tracer-server
fi
repl=$(command -v "${CLANG_TRACER_REPL:-$repl_default}")
server=$(command -v "${CLANG_TRACER_SERVER:-$server_default}")
compiler=${CXX:-clang++-21}

run_dir=$(mktemp -d "${TMPDIR:-/tmp}/tracer-jit-demo.XXXXXXXX")
target_pid=
stop_target() {
  if [[ -n $target_pid ]]; then
    kill "$target_pid" 2>/dev/null || true
    kill -CONT "$target_pid" 2>/dev/null || true
    wait "$target_pid" 2>/dev/null || true
    target_pid=
  fi
}
trap stop_target EXIT
trap 'exit 130' INT
trap 'exit 143' TERM HUP

target=$example_dir/target
if [[ ! -x $target || -n ${CXX:-} ]]; then
  target=$run_dir/target
  "$compiler" -std=c++17 -g -O0 -fno-omit-frame-pointer -fcf-protection=none \
    -rdynamic "$example_dir/../cpp-interceptors/target.cpp" -o "$target"
fi
cp -- "$example_dir/observer.cpp" "$run_dir/"
cd -- "$run_dir"
unset CLANG_TRACER_ENDPOINT
"$target" >target.log 2>&1 &
target_pid=$!
printf 'Demo directory: %s\nTarget PID: %s\n' "$run_dir" "$target_pid"

if [[ ${1:-} == --interactive ]]; then
  printf 'Walkthrough: %s/README.md\n' "$example_dir"
  "$repl" --server "$server" --attach "$target_pid" --timeout 60
else
  "$repl" --server "$server" --attach "$target_pid" --timeout 60 --fail-fast \
    <"$example_dir/capture.repl"
  stop_target
  "$repl" --fail-fast <"$example_dir/replay.repl"
  printf 'SQLite recording: %s/capture.db\n' "$run_dir"
fi

#!/usr/bin/env bash
set -euo pipefail

example_dir=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
category=${1:-all}
interactive=${2:-}
case "$category" in
  all) categories=(user simple sequences map unordered_map set unordered_set) ;;
  associative) categories=(map unordered_map set unordered_set) ;;
  user|simple|sequences|map|unordered_map|set|unordered_set) categories=("$category") ;;
  *) printf 'Usage: %s [all|user|simple|sequences|associative|map|unordered_map|set|unordered_set] [--interactive]\n' "$0" >&2; exit 2 ;;
esac
if [[ $# -gt 2 || ( -n "$interactive" && "$interactive" != --interactive ) ]]; then
  printf 'The second argument must be --interactive.\n' >&2
  exit 2
fi
if [[ $(uname -s) != Linux || $(uname -m) != x86_64 ]]; then
  printf 'Run this native attach example on Linux x86-64.\n' >&2
  exit 1
fi
compiler=${CXX:-clang++-21}
repl=$(command -v "${CLANG_TRACER_REPL:-clang-tracer-repl}")
server=$(command -v "${CLANG_TRACER_SERVER:-clang-tracer-server}")
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/tracer-values.XXXXXX")
demo_pid=
cleanup() {
  if [[ -n "$demo_pid" ]]; then
    kill "$demo_pid" 2>/dev/null || true
    # Deliver termination even if a failed attach left our fixture stopped.
    kill -CONT "$demo_pid" 2>/dev/null || true
    wait "$demo_pid" 2>/dev/null || true
  fi
  rm -rf -- "$run_dir"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
target_executable=${CLANG_TRACER_VALUES_TARGET:-}
if [[ -z "$target_executable" ]]; then
  target_executable=$run_dir/target
  "$compiler" -std=c++17 -g -O0 -fno-omit-frame-pointer -fcf-protection=none \
    -rdynamic -pthread "$example_dir/target.cpp" -o "$target_executable"
fi
"$target_executable" >"$run_dir/target.log" 2>&1 &
demo_pid=$!
# Relative includes in the C++ REPL files resolve from this directory.
cd "$example_dir"
for current in "${categories[@]}"; do
  case "$current" in
    user) function_name=user_types ;;
    simple) function_name=simple_types ;;
    sequences) function_name=sequence_types ;;
    map|unordered_map|set|unordered_set) function_name=${current}_types ;;
  esac
  setup() {
    printf '%%probe inject "value_examples::%s" --callback "%s/interceptor.cpp" --include "%s/types.hpp"\n' "$function_name" "$example_dir" "$example_dir"
    printf '%%collect --count 6 --phase POST --timeout 20 --as demo\n'
    printf '%%cpp options add "-I%s"\n' "$example_dir"
    printf '%%cpp import "types.hpp"\n'
    printf '%%cpp load "%s/%s.cpp"\n' "$example_dir" "$current"
  }
  printf '\nRunning %s values example\n' "$current"
  if [[ "$interactive" == --interactive ]]; then
    printf 'Enter these commands one at a time; use %%quit when finished:\n'
    setup
    # Keep a real terminal so editing and Tab completion work normally.
    env -u CLANG_TRACER_ENDPOINT "$repl" --server "$server" \
      --attach "$demo_pid" --timeout 60
  else
    {
      setup
      printf '%%detach\n%%quit\n'
    } | env -u CLANG_TRACER_ENDPOINT "$repl" --server "$server" \
      --attach "$demo_pid" --timeout 60 --fail-fast
  fi
done

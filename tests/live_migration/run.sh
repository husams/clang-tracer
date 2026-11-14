#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -sm)" != "Linux x86_64" ]]; then
  echo "The production live JIT requires Linux x86-64." >&2
  exit 1
fi
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
shared_src=${1:-"$root/src"}
build=$(mktemp -d "${TMPDIR:-/tmp}/clang-tracer-story4.XXXXXX")
trap 'rm -rf -- "$build"' EXIT
compiler=${CXX:-clang++}
llvm_config=${LLVM_CONFIG:-llvm-config}
llvm_include=$("$llvm_config" --includedir)
llvm_lib=$("$llvm_config" --libdir)
read -r -a llvm_libs <<< "$("$llvm_config" --libs)"
"$compiler" -std=c++23 -O1 -pthread -Wall -Wextra \
  -I"$root/include" -I"$root/src" -I"$shared_src" -isystem "$llvm_include" \
  "-DTRACER_CLANG_RESOURCE_DIR=\"$("$compiler" -print-resource-dir)\"" \
  "$root/src/live_migration.cpp" "$shared_src/live_compile.cpp" "$shared_src/live_jit.cpp" \
  "$root/tests/live_migration/integration.cpp" -L"$llvm_lib" "-Wl,-rpath,$llvm_lib" \
  -lclang-cpp "${llvm_libs[@]}" -ldl -o "$build/integration"
"$build/integration" "$root/tests/live_migration" "$root/include"

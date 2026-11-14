#!/usr/bin/env bash
set -euo pipefail

# Run with the Story 1 shared JIT sources and a build of the merged baseline.
# The caller selects a private output directory for this fixture.
root=$(cd "$(dirname "$0")/.." && pwd)
common=${1:?provide the shared JIT source directory}
baseline=${2:?provide a configured baseline build directory}
output=${3:?provide a private fixture output directory}
cxx=${CXX:-clang++}
llvm=${LLVM_CONFIG:-llvm-config}
mkdir -p "$output"
resource=$("$cxx" -print-resource-dir)
llvm_include=$("$llvm" --includedir)
llvm_lib=$("$llvm" --libdir)
resolver="$baseline/CMakeFiles/clang_tracer_agent.dir/src/debug_resolver.cpp.o"

flags=(-std=c++23 -g -O0 -I"$root/src" -I"$common" -I"$root/include"
  -I"$baseline/generated" -isystem "$llvm_include"
  "-DTRACER_CLANG_RESOURCE_DIR=\"$resource\"")
sources=("$root"/src/live_data{,_scan,_jit,_command}.cpp
  "$common"/live_{compile,jit}.cpp "$root/tests/live_data_target.cpp"
  "$root/tests/fixtures/live_data/model.cpp")
"$cxx" "${flags[@]}" -Wl,--build-id=sha1 -rdynamic "${sources[@]}" "$resolver" \
  "$baseline/libtracer_proto.a" -L"$llvm_lib" -Wl,-rpath,"$llvm_lib" \
  -lclang-cpp -lLLVM -lprotobuf -ldl -pthread -o "$output/live_data_target"
build_id=$(readelf -n "$output/live_data_target" | awk '/Build ID/ {print $3}')
"$output/live_data_target" "$root/tests/fixtures/live_data" "$root/include" "$build_id"

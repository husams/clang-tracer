#!/usr/bin/env bash
set -euo pipefail

# Usage: run-metadata.sh SOURCE_ROOT NEW_OUTPUT_DIR [FUNCTIONS_PER_MODULE=10000]
source_root=$(cd "$1" && pwd)
mkdir -p "$2"
output=$(cd "$2" && pwd)
functions=${3:-10000}
compiler=${CXX:-clang++-21}
llvm_config=${LLVM_CONFIG:-llvm-config-21}
fixtures=${ATTACH_FIXTURES:-$output/fixtures}
if [[ -e "$output/identity.sha256" || -e "$output/modules-0.csv" ]]; then
  echo "Choose a new output directory; existing measurements are preserved" >&2
  exit 1
fi
mkdir -p "$fixtures"
fixtures=$(cd "$fixtures" && pwd)
if [[ -e "$fixtures/functions.txt" ]]; then
  if [[ $(cat "$fixtures/functions.txt") != "$functions" ]]; then
    echo "Fixture function count differs from requested count" >&2
    exit 1
  fi
else
  awk -v n="$functions" 'BEGIN {
    for (i=0; i<n; i++)
      printf "extern \"C\" __attribute__((noinline)) int fixture_%d(int x) { volatile int y = x + %d; return y; }\n", i, i;
  }' > "$fixtures/module.cpp"
  "$compiler" -shared -fPIC -O0 -g -fasynchronous-unwind-tables \
    "$fixtures/module.cpp" -o "$fixtures/module-0.so"
  for ((i=1; i<16; i++)); do
    cp "$fixtures/module-0.so" "$fixtures/module-$i.so"
  done
  cp "$fixtures/module-0.so" "$fixtures/injected.so"
  "$compiler" -O0 -g "$source_root/benchmarks/attach/target.cpp" -ldl \
    -o "$fixtures/target"
  echo "$functions" > "$fixtures/functions.txt"
fi

"$compiler" -O2 -g -std=c++20 -I"$source_root/src" \
  $("$llvm_config" --cxxflags | sed 's/-std=[^ ]*//g; s/-fno-exceptions//g') \
  "$source_root/benchmarks/attach/metadata.cpp" "$source_root/src/admission.cpp" \
  $("$llvm_config" --ldflags --libs --system-libs) -ldl -o "$output/metadata"
"$compiler" -O2 -g -std=c++20 -I"$source_root/src" \
  $("$llvm_config" --cxxflags | sed 's/-std=[^ ]*//g; s/-fno-exceptions//g') \
  "$source_root/benchmarks/attach/bootstrap.cpp" \
  "$source_root/src/admission.cpp" "$source_root/src/injector.cpp" \
  $("$llvm_config" --ldflags --libs --system-libs) -ldl -pthread \
  -o "$output/bootstrap"

"$compiler" --version > "$output/compiler.txt"
"$llvm_config" --version > "$output/llvm.txt"
sha256sum "$output/metadata" "$output/bootstrap" "$fixtures/target" \
  "$fixtures"/*.so > "$output/identity.sha256"
for count in 0 1 8 16; do
  modules=()
  for ((i=0; i<count; i++)); do modules+=("$fixtures/module-$i.so"); done
  "$output/metadata" "${modules[@]}" > "$output/modules-$count.csv"
  for sample in 0 1 2 3 4; do
    "$output/bootstrap" "$fixtures/injected.so" "$fixtures/target" "${modules[@]}" \
      > "$output/bootstrap-$count-$sample.csv"
  done
done

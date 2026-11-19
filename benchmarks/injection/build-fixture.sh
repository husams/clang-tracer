#!/usr/bin/env bash
set -euo pipefail

# Build once and reuse the same ELF inputs for baseline/candidate runs.
output=$1
functions=${2:-30000}
compiler=${CXX:-clang++-21}
compiler_flags=()
link_flags=()
if [[ -n "${TRACER_BENCH_CXX_FLAGS:-}" ]]; then
  read -r -a compiler_flags <<< "$TRACER_BENCH_CXX_FLAGS"
fi
if [[ -n "${TRACER_BENCH_LINK_FLAGS:-}" ]]; then
  read -r -a link_flags <<< "$TRACER_BENCH_LINK_FLAGS"
fi
if [[ ! "$functions" =~ ^[1-9][0-9]*$ ]]; then
  echo "Function count must be a positive integer" >&2
  exit 1
fi
if [[ -e "$output/identity.sha256" ]]; then
  echo "Use a new fixture directory; frozen inputs are preserved" >&2
  exit 1
fi
mkdir -p "$output"
source_root=$(cd "$(dirname "$0")/../.." && pwd)
cat > "$output/module.cpp" <<'CPP'
extern "C" __attribute__((noinline, visibility("default"), patchable_function_entry(16, 0)))
int injection_target(int argument) {
  volatile int scaled = argument * 3;
  return scaled + 7;
}
CPP
awk -v n="$functions" 'BEGIN {
  for (i=0; i<n; i++)
    printf "extern \"C\" __attribute__((noinline)) int unused_%d(int argument) { volatile int value = argument + %d; return value; }\n", i, i;
}' >> "$output/module.cpp"
"$compiler" "${compiler_flags[@]}" -std=c++17 -g -O0 -shared -fPIC -fno-omit-frame-pointer \
  -fcf-protection=none "$output/module.cpp" "${link_flags[@]}" -o "$output/module.so"
"$compiler" "${compiler_flags[@]}" -std=c++17 -g -O0 -fno-omit-frame-pointer -fcf-protection=none \
  "$source_root/benchmarks/injection/target.cpp" "${link_flags[@]}" -ldl -o "$output/target"
sha256sum "$output/module.cpp" "$output/module.so" "$output/target" \
  > "$output/identity.sha256"
"$compiler" --version > "$output/compiler.txt"

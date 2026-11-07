#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]] || [[ "$(uname -s)/$(uname -m)" != Linux/x86_64 ]]; then
  echo "Usage on Linux x86-64: $0 NEW_BUILD_DIRECTORY GUM_DEVKIT_DIRECTORY|-" >&2
  exit 2
fi

src="$(cd "$(dirname "$0")" && pwd)"
out="$1"
gum="$2"
mkdir -- "$out"
read -r -a link_flags <<< "${PILOT_LDFLAGS:-}"

"${CXX:-c++}" -std=c++17 -O2 -g -Wall -Wextra -Wpedantic -fPIE -pie \
  "$src/pilot.cpp" "$src/workload.S" -ldl "${link_flags[@]}" -o "$out/pilot"
if [[ "$gum" != - ]]; then
  "${CC:-cc}" -std=c11 -O2 -fPIC -shared -fvisibility=hidden -Wall -Wextra \
    -Wl,--exclude-libs,ALL,-z,defs -I "$gum" "$src/gum_backend.c" \
    "$gum/libfrida-gum.a" -ldl -lpthread -lresolv -lm -o "$out/gum-backend.so"
fi

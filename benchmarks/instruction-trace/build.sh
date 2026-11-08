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
  "${CXX:-c++}" -std=c++17 -O0 -g -Wall -Wextra -Wpedantic -fPIE -pie \
    -fno-omit-frame-pointer -fcf-protection=none \
    "$src/live_target.cpp" "$src/workload.S" -pthread \
    "${link_flags[@]}" -o "$out/live-target"
  "${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Wpedantic -fPIC -shared -fvisibility=hidden \
    -I "$src/../../include" "$src/verify_callback.cpp" "${link_flags[@]}" -o "$out/verify-callback.so"
  "$src/link-gum.sh" "$out" "$gum" "$gum/libfrida-gum.a"
fi

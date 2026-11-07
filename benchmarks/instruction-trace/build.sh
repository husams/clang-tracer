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
    -Wl,--no-as-needed -ldl -Wl,--as-needed "${link_flags[@]}" -o "$out/live-target"
  "${CC:-cc}" -std=c11 -O2 -fPIC -shared -fvisibility=hidden -Wall -Wextra \
    -Wl,--exclude-libs,ALL,-z,defs,-z,relro,-z,now,-z,noexecstack,--build-id=sha1 \
    -Wl,--wrap=__libc_dlopen_mode \
    -Xlinker --version-script -Xlinker "$src/gum.exports" \
    -Xlinker -Map -Xlinker "$out/gum-backend.map" -I "$gum" "$src/gum_backend.c" \
    "$gum/libfrida-gum.a" -ldl -lpthread -lresolv -lm -o "$out/gum-backend.so"
  readelf -WV "$out/gum-backend.so" > "$out/gum-backend.versions"
  if grep -qw GLIBC_PRIVATE "$out/gum-backend.versions"; then
    echo "Gum backend requires private glibc symbols" >&2
    exit 1
  fi
fi

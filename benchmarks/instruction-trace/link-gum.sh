#!/usr/bin/env bash
set -euo pipefail

src="$(cd "$(dirname "$0")" && pwd)"
out="$1"
headers="$2"
archive="$3"
cc="${CC:-cc}"
linker_args=()
# The SDK's BFD 2.30 predates the devkit's GNU properties. Clang can
# select the host BFD while retaining the SDK sysroot and startup objects.
if [[ "$("$cc" --version)" == *"clang version"* ]]; then
  linker_args=(--ld-path="${PILOT_GUM_LINKER:-/usr/bin/ld.bfd}")
elif [[ -n "${PILOT_GUM_LINKER:-}" ]]; then
  echo "PILOT_GUM_LINKER requires Clang; otherwise configure the compiler's linker." >&2
  exit 2
fi

"$cc" "${linker_args[@]}" -std=c11 -O2 -fPIC -shared -fvisibility=hidden -Wall -Wextra \
  -Wl,--fatal-warnings,--exclude-libs,ALL,-z,defs,-z,relro,-z,now,-z,noexecstack,--build-id=sha1 \
  -Wl,--wrap=__libc_dlopen_mode,--wrap=prctl,--wrap=gum_process_modify_thread \
  -Xlinker --version-script -Xlinker "$src/gum.exports" \
  -Xlinker -Map -Xlinker "$out/gum-backend.map" -I "$src/../../include/tracer" -I "$headers" \
  "$src/gum_backend.c" \
  "$archive" -ldl -lpthread -lresolv -lm -o "$out/gum-backend.so"
readelf -WV "$out/gum-backend.so" > "$out/gum-backend.versions"
if grep -qw GLIBC_PRIVATE "$out/gum-backend.versions"; then
  echo "Gum backend requires private glibc symbols" >&2
  exit 1
fi

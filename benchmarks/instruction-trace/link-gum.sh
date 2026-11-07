#!/usr/bin/env bash
set -euo pipefail

src="$(cd "$(dirname "$0")" && pwd)"
out="$1"
headers="$2"
archive="$3"

"${CC:-cc}" -std=c11 -O2 -fPIC -shared -fvisibility=hidden -Wall -Wextra \
  -Wl,--exclude-libs,ALL,-z,defs,-z,relro,-z,now,-z,noexecstack,--build-id=sha1 \
  -Wl,--wrap=__libc_dlopen_mode \
  -Xlinker --version-script -Xlinker "$src/gum.exports" \
  -Xlinker -Map -Xlinker "$out/gum-backend.map" -I "$headers" "$src/gum_backend.c" \
  "$archive" -ldl -lpthread -lresolv -lm -o "$out/gum-backend.so"
readelf -WV "$out/gum-backend.so" > "$out/gum-backend.versions"
if grep -qw GLIBC_PRIVATE "$out/gum-backend.versions"; then
  echo "Gum backend requires private glibc symbols" >&2
  exit 1
fi

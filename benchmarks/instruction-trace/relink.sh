#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 1 || "$(uname -s)/$(uname -m)" != Linux/x86_64 ]]; then
  echo "Usage on Linux x86-64: $0 NEW_BUILD_DIRECTORY" >&2
  exit 2
fi
bundle="$(cd "$(dirname "$0")" && pwd)"
mkdir -- "$1"
out="$(cd "$1" && pwd)"
prefix="$out/prefix"
export LC_ALL=C PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig" PKG_CONFIG_PATH=
meson="${MESON:-meson}"
jobs="${JOBS:-6}"

setup() {
  local project="$1"
  shift
  "$meson" setup "$out/$project" "$bundle/source/$project" --prefix "$prefix" \
    --libdir lib --wrap-mode=nodownload --buildtype=release \
    -Ddefault_library=static -Db_staticpic=true "$@"
}

build_dependency() {
  local project="$1"
  setup "$@"
  "$meson" compile -C "$out/$project" -j "$jobs"
  "$meson" install -C "$out/$project" --no-rebuild
}

build_dependency libffi -Dexe_static_tramp=false -Dtests=false -Ddoc=false
build_dependency pcre2 -Dgrep=false -Dtest=false
build_dependency zlib
setup glib -Dcocoa=disabled -Dselinux=disabled -Dxattr=false -Dlibmount=disabled \
  -Dsysprof=disabled -Dtests=false -Dnls=disabled -Dintrospection=disabled \
  -Dglib_debug=disabled -Dglib_assert=false -Dglib_checks=false
"$meson" compile -C "$out/glib" -j "$jobs" glib-2.0 gobject-2.0

libs=("$out/glib/glib/libglib-2.0.a" "$out/glib/gobject/libgobject-2.0.a"
      "$prefix/lib/libffi.a" "$prefix/lib/libpcre2-8.a")
mkdir "$out/link"
# Apply the pinned devkit's third-party symbol naming to the rebuilt libraries.
nm -g --defined-only -P "${libs[@]}" |
  awk '$2 ~ /^[TDBRC]$/ && $1 !~ /^(frida|_frida|gum|_gum)/ {print $1, "_frida_" $1}' |
  sort -u > "$out/link/renames"
for lib in "${libs[@]}"; do
  objcopy --redefine-syms="$out/link/renames" "$lib" "$out/link/${lib##*/}"
done
ln -s "$bundle/relink/libgum-base.a" "$out/link/libgum-base.a"
(
  cd "$out/link"
  ar -M <<'MRI'
create libfrida-gum.a
addlib libgum-base.a
addlib libgobject-2.0.a
addlib libglib-2.0.a
addlib libffi.a
addlib libpcre2-8.a
save
end
MRI
)
"$bundle/relink/link-gum.sh" "$out" "$bundle/relink" "$out/link/libfrida-gum.a"
if awk '/Name: GLIBC_[0-9]/{split($3, v, /[_.]/); if (v[2] > 2 || (v[2] == 2 && v[3] > 23)) newer=1}
        END {exit !newer}' "$out/gum-backend.versions"; then
  echo "Gum backend requires glibc newer than 2.23. Use the documented SDK." >&2
  exit 1
fi

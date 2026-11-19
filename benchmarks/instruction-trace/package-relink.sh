#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

if [[ $# != 2 || "$(uname -s)/$(uname -m)" != Linux/x86_64 ]]; then
  echo "Usage on Linux x86-64: $0 GUM_17.26.1_DEVKIT_TAR_XZ NEW_BUNDLE_DIRECTORY" >&2
  exit 2
fi
src="$(cd "$(dirname "$0")" && pwd)"
devkit="$(realpath -- "$1")"
expected=4e7247a5f990b15d319c997836af2f6fea38fccb493dc7f6b4b6142009df040e
printf '%s  %s\n' "$expected" "$devkit" | sha256sum --check --status
mkdir -- "$2"
bundle="$(cd "$2" && pwd)"
work="$(mktemp -d "$bundle/.package.XXXXXXXX")"
trap 'find "$work" -depth -delete' EXIT
mkdir "$work/devkit" "$work/objects" "$bundle/relink" "$bundle/source" "$bundle/notices"
tar -xJf "$devkit" -C "$work/devkit"
archive="$work/devkit/libfrida-gum.a"
"$src/link-gum.sh" "$work" "$work/devkit" "$archive"

sed -n 's/.*libfrida-gum\.a(\([^)]*\)).*/\1/p' "$work/gum-backend.map" |
  sort -u > "$work/linked"
readelf --debug-dump=info "$archive" |
  awk '
    /^File: / {member=$0; sub(/^.*\(/, "", member); sub(/\)$/, "", member)}
    /<0><[[:xdigit:]]+>.*DW_TAG_compile_unit/ {root=1}
    /<1><[[:xdigit:]]+>/ {root=0}
    root && /DW_AT_comp_dir.*\/(glib|pcre2|libffi)$/ {print member}
  ' | sort -u > "$work/replaced"
comm -23 "$work/linked" "$work/replaced" > "$bundle/relink/base-members.txt"
if [[ "$(wc -l < "$work/linked")" != 282 ||
      "$(wc -l < "$bundle/relink/base-members.txt")" != 149 ]]; then
  echo "Unexpected devkit member selection. Recheck the pinned license audit." >&2
  exit 1
fi
(
  cd "$work/objects"
  mapfile -t members < "$bundle/relink/base-members.txt"
  ar x "$archive" "${members[@]}"
  ar crsD "$bundle/relink/libgum-base.a" "${members[@]}"
)
cp "$work/devkit/frida-gum.h" "$bundle/relink/"
cp "$src/"{link-gum.sh,gum_backend.c,gum.exports,pilot.h} "$bundle/relink/"
cp "$src/../../include/tracer/instruction_trace.h" "$bundle/relink/"
cp "$src/"{relink.sh,GUM_LICENSE_AUDIT.md,gum-source-pins.tsv} "$bundle/"
cp "$src/RELINK.md" "$bundle/README.md"
cp "$src/../../LICENSE" "$bundle/notices/clang-tracer.txt"

while IFS=$'\t' read -r destination url; do
  mkdir -p "$bundle/source/$destination"
  curl -fsSL --retry 3 "$url" |
    tar -xz --strip-components=1 -C "$bundle/source/$destination"
done < "$bundle/gum-source-pins.tsv"
(
  cd "$bundle/source"
  cp --parents glib/{COPYING,proxy-libintl-static/COPYING.LIB.txt} \
    glib/subprojects/gvdb/{COPYING,LICENSES/LGPL-2.1-or-later.txt} \
    libffi/LICENSE pcre2/{LICENCE.md,deps/sljit/LICENSE} \
    zlib/LICENSE frida-gum/COPYING capstone/{LICENSE.TXT,LICENSE_LLVM.TXT} \
    xz/{COPYING,COPYING.0BSD} "$bundle/notices/"
)
"$bundle/relink.sh" "$work/rebuild"
strip --strip-unneeded -o "$bundle/gum-backend.so" "$work/rebuild/gum-backend.so"
(
  cd "$bundle"
  find . -path "./${work##*/}" -prune -o -type f ! -path ./SHA256SUMS -print0 |
    sort -z | xargs -0 sha256sum > SHA256SUMS
)

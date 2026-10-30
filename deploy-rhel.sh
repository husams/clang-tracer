#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
build_dir="$source_dir/build-rhel"; prefix=; destdir=
usage() {
  cat <<'TXT'
Usage: ./deploy-rhel.sh [--build-dir DIR] [--prefix DIR] [--destdir DIR]
Install the configured runtime, headers, schema and licenses with CMake.
  --build-dir DIR  Configured build directory (default: build-rhel)
  --prefix DIR     Override the prefix for relative install directories
  --destdir DIR    Stage under DIR instead of installing into the live filesystem
  --help          Show help
Binary/library subdirectories come from the build configuration. No sudo,
package installation, daemon setup, tracing-policy or loader-cache changes occur.
TXT
}
while (($#)); do
  [[ "$1" != --help && "$1" != -h ]] || { usage; exit 0; }
  [[ $# -ge 2 && -n "$2" ]] || { echo "Missing value for $1" >&2; exit 2; }
  case "$1" in
    --build-dir) build_dir=$2 ;;
    --prefix) prefix=$2 ;;
    --destdir) destdir=$2 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
  shift 2
done
[[ $(uname -s) == Linux ]] || { echo 'Install on Linux.' >&2; exit 2; }
[[ -f "$build_dir/cmake_install.cmake" ]] || { echo 'Configure/build first.' >&2; exit 2; }
args=(--install "$build_dir")
[[ -z "$prefix" ]] || args+=(--prefix "$prefix")
if [[ -n "$destdir" ]]; then
  mkdir -p -- "$destdir"
  destdir=$(realpath -- "$destdir")
  DESTDIR="$destdir" cmake "${args[@]}"
else
  cmake "${args[@]}"
fi

#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo 'Usage: tests/overlay_integration.sh ABSOLUTE_BUILD_DIR [CTEST_ARGUMENT ...]' >&2
  exit 2
fi
if [[ $1 != /* || ! -f $1/CTestTestfile.cmake ]]; then
  echo 'Supply an absolute configured CTest build directory.' >&2
  exit 2
fi
BUILD=$(cd -- "$1" && pwd -P)
shift
FIXTURE=$(mktemp -d /tmp/tracer-overlay-tests.XXXXXX)
cleanup() {
  # OverlayFS creates its private work/work directory with mode 000.
  chmod -R u+rwx "$FIXTURE" 2>/dev/null || true
  rm -rf -- "$FIXTURE"
}
trap cleanup EXIT
mkdir "$FIXTURE/lower" "$FIXTURE/upper" "$FIXTURE/work"

# Only this child namespace sees the overlay. The host build tree and its
# artifacts remain untouched; all CTest output is copied up into scratch.
unshare -Urm bash -s -- "$BUILD" "$FIXTURE" "$@" <<'NAMESPACE'
set -euo pipefail
BUILD=$1
FIXTURE=$2
shift 2
mount --make-rprivate /
mount --bind "$BUILD" "$FIXTURE/lower"
mount -o remount,bind,ro "$FIXTURE/lower"
mount -t overlay overlay \
  -o "userxattr,lowerdir=$FIXTURE/lower,upperdir=$FIXTURE/upper,workdir=$FIXTURE/work" \
  "$BUILD"
test "$(stat -f -c %T "$BUILD")" = overlayfs
ctest --test-dir "$BUILD" --output-on-failure "$@"
NAMESPACE

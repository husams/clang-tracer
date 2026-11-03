#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/gcc85-build.sh ABSOLUTE_SDK_ROOT ABSOLUTE_SCRATCH_ROOT [JOBS]
       [--sysroot ABSOLUTE_SYSROOT] [--dynamic-loader ABSOLUTE_LOADER]
       [--overlay-tests]

Build dependencies and clang-tracer with the GCC 8.5-era SDK sysroot. The
scratch root is preserved; existing dependency checkouts must match their pins.
The SDK path is required; no machine-specific default is used.
Sysroot and dynamic loader default to the supplied SDK's layout; both can be
overridden explicitly. Use a fresh scratch directory when changing the profile.
Host CMake defaults to `cmake` and can be overridden with
TRACER_CMAKE. Jobs defaults to 3.
The normal CTest suite runs after the application build and before install.
OverlayFS tests are opt-in with --overlay-tests.
EOF
}

if [[ ${1:-} == --help || ${1:-} == -h ]]; then
  usage
  exit 0
fi
if [[ $# -lt 2 ]]; then
  usage >&2
  exit 2
fi

SDK=$1
SCRATCH=$2
shift 2
JOBS=3
if [[ $# -gt 0 && $1 != --* ]]; then
  JOBS=$1
  shift
fi
SDK_SYSROOT=''
SDK_LOADER=''
OVERLAY_TESTS=0
while [[ $# -gt 0 ]]; do
  case $1 in
    --sysroot|--dynamic-loader)
      if [[ $# -lt 2 || -z $2 ]]; then
        echo "Missing path for $1" >&2
        exit 2
      fi
      if [[ $1 == --sysroot ]]; then
        SDK_SYSROOT=$2
      else
        SDK_LOADER=$2
      fi
      shift 2
      ;;
    --overlay-tests)
      OVERLAY_TESTS=1
      shift
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done
CMAKE=${TRACER_CMAKE:-cmake}
GRPC_COMMIT=02384e39185f109bd299eb8482306229967dc970
GRPC_TAG=v1.46.7
SQLITE_URL=https://www.sqlite.org/2021/sqlite-autoconf-3340100.tar.gz
SQLITE_EXPECTED_SHA256=${SQLITE_EXPECTED_SHA256:-2a3bca581117b3b88e5361d0ef3803ba6d8da604b1c1a47d902ef785c1b53e89}

if [[ $SDK != /* ]]; then
  echo "SDK root must be an absolute path: $SDK" >&2
  exit 2
fi
if [[ $SCRATCH != /* ]]; then
  echo "Scratch root must be an absolute path: $SCRATCH" >&2
  exit 2
fi
if [[ ! $JOBS =~ ^[1-9][0-9]*$ ]]; then
  echo "JOBS must be a positive integer: $JOBS" >&2
  exit 2
fi

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
SOURCE_ROOT=$(cd -- "$SCRIPT_DIR/.." && pwd -P)
SDK=$(cd -- "$SDK" && pwd -P)
DEPS="$SCRATCH/deps"
BUILD="$SCRATCH/build"
LOGS="$SCRATCH/logs"
GRPC_SOURCE="$DEPS/src/grpc"
GRPC_BUILD="$BUILD/grpc"
SQLITE_SOURCE="$DEPS/src/sqlite-autoconf-3340100"
SQLITE_ARCHIVE="$DEPS/src/sqlite-autoconf-3340100.tar.gz"
SQLITE_PREFIX="$DEPS"
APP_BUILD="$BUILD/clang-tracer"
APP_PREFIX="$SCRATCH/install"
SDK_TRIPLE="$SDK/x86_64-1a-linux-gnu"
SDK_SYSROOT=${SDK_SYSROOT:-"$SDK_TRIPLE/sysroot"}
SDK_LOADER=${SDK_LOADER:-"$SDK_SYSROOT/lib64/ld-linux-x86-64.so.2"}
if [[ $SDK_SYSROOT != /* || ! -d $SDK_SYSROOT ]]; then
  echo "Sysroot must be an existing absolute directory: $SDK_SYSROOT" >&2
  exit 2
fi
if [[ $SDK_LOADER != /* || ! -f $SDK_LOADER || ! -x $SDK_LOADER ]]; then
  echo "Dynamic loader must be an executable absolute file: $SDK_LOADER" >&2
  exit 2
fi

for tool in "$SDK/bin/clang" "$SDK/bin/clang++"; do
  if [[ ! -x $tool ]]; then
    echo "Required SDK tool is missing or not executable: $tool" >&2
    exit 1
  fi
done
if ! command -v "$CMAKE" >/dev/null 2>&1; then
  echo "Host CMake command not found: $CMAKE (set TRACER_CMAKE to its path)" >&2
  exit 1
fi
CMAKE_BIN=$(command -v "$CMAKE")
CTEST_BIN="$(dirname -- "$CMAKE_BIN")/ctest"
if [[ ! -x $CTEST_BIN ]]; then
  CTEST_BIN=$(command -v ctest || true)
fi
if [[ -z $CTEST_BIN || ! -x $CTEST_BIN ]]; then
  echo "Host CTest not found beside $CMAKE_BIN or in PATH" >&2
  exit 1
fi
GENERATOR_ARGS=()
if command -v ninja >/dev/null 2>&1; then
  GENERATOR_ARGS=(-G Ninja)
  NINJA_INFO=$(ninja --version)
else
  NINJA_INFO='unavailable; using the host make generator'
fi
for tool in git python3 curl sha256sum make; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "Required host tool not found in PATH: $tool" >&2
    exit 1
  fi
done

mkdir -p "$DEPS/src" "$BUILD" "$LOGS" "$APP_PREFIX"
{
  printf 'host_cmake %s\n' "$("$CMAKE_BIN" --version | head -n 1)"
  printf 'host_ctest %s\n' "$CTEST_BIN"
  printf 'ninja %s\n' "$NINJA_INFO"
  printf 'sdk %s\n' "$SDK"
  printf 'sysroot %s\n' "$SDK_SYSROOT"
  printf 'dynamic_loader %s\n' "$SDK_LOADER"
  printf 'scratch %s\n' "$SCRATCH"
  printf 'jobs %s\n' "$JOBS"
  if [[ $OVERLAY_TESTS == 1 ]]; then
    printf 'overlay_tests enabled\n'
  else
    printf 'overlay_tests disabled\n'
  fi
} > "$SCRATCH/build-environment.txt"
RPATH="$DEPS/lib;$DEPS/lib64;$SDK_TRIPLE/lib64;$SDK_SYSROOT/lib64"
EXE_LINKER_FLAGS="-Wl,--dynamic-linker=$SDK_LOADER -Wl,-rpath,$DEPS/lib:$DEPS/lib64:$SDK_TRIPLE/lib64:$SDK_SYSROOT/lib64"
export PATH="$DEPS/bin:$SDK/bin:$PATH"

if [[ ! -d $GRPC_SOURCE/.git ]]; then
  if [[ -e $GRPC_SOURCE ]]; then
    echo "Refusing to replace non-git path: $GRPC_SOURCE" >&2
    exit 1
  fi
  git clone --filter=blob:none --depth 1 --branch "$GRPC_TAG" --no-checkout \
    https://github.com/grpc/grpc.git "$GRPC_SOURCE" \
    2>&1 | tee "$LOGS/grpc-clone.log"
  git -C "$GRPC_SOURCE" checkout --detach "$GRPC_TAG" \
    2>&1 | tee "$LOGS/grpc-checkout.log"
fi
ACTUAL_GRPC_COMMIT=$(git -C "$GRPC_SOURCE" rev-parse HEAD)
if [[ $ACTUAL_GRPC_COMMIT != "$GRPC_COMMIT" ]]; then
  echo "gRPC checkout mismatch: expected $GRPC_COMMIT, found $ACTUAL_GRPC_COMMIT" >&2
  exit 1
fi
PROTOBUF_INSTALL_CMAKE="$GRPC_SOURCE/third_party/protobuf/cmake/install.cmake"
check_grpc_source_tree() {
python3 - "$GRPC_SOURCE" "$PROTOBUF_INSTALL_CMAKE" "$1" <<'PY'
from pathlib import Path
import subprocess
import sys


def git(repo, *args, check=True):
    return subprocess.run(
        ["git", "-C", str(repo), *args], check=check, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )


def git_bytes(repo, *args):
    return subprocess.run(
        ["git", "-C", str(repo), *args], check=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE
    ).stdout


grpc_root = Path(sys.argv[1]).resolve()
protobuf_file = Path(sys.argv[2]).resolve()
allow_uninitialized = sys.argv[3] == "allow-uninitialized"
protobuf_repo = protobuf_file.parents[1]
zlib_repo = (grpc_root / "third_party/zlib").resolve()
protobuf_initialized = (protobuf_repo / ".git").exists()
zlib_initialized = (zlib_repo / ".git").exists()
if protobuf_initialized and git(protobuf_repo, "rev-parse", "--show-toplevel").stdout.strip() != str(protobuf_repo):
    raise SystemExit(f"protobuf source is not its own checked-out submodule: {protobuf_repo}")
if not protobuf_initialized and not allow_uninitialized:
    raise SystemExit(f"protobuf submodule was not initialized: {protobuf_repo}")
if zlib_initialized and git(zlib_repo, "rev-parse", "--show-toplevel").stdout.strip() != str(zlib_repo):
    raise SystemExit(f"zlib source is not its own checked-out submodule: {zlib_repo}")
if not zlib_initialized and not allow_uninitialized:
    raise SystemExit(f"zlib submodule was not initialized: {zlib_repo}")

submodules = git(grpc_root, "submodule", "status", "--recursive").stdout.splitlines()
unresolved = [line for line in submodules
              if line and (line[0] in "+U" or (line[0] == "-" and not allow_uninitialized))]
if unresolved:
    raise SystemExit("uninitialized or mismatched recursive submodule revision(s):\n" +
                     "\n".join(unresolved))

repositories = [grpc_root]
seen = {grpc_root}
for repo in repositories:
    modules_file = repo / ".gitmodules"
    if not modules_file.exists():
        continue
    paths = git(repo, "config", "--file", str(modules_file), "--get-regexp",
                r"^submodule\..*\.path$", check=False)
    if paths.returncode not in (0, 1):
        raise SystemExit(paths.stderr.strip() or f"cannot read submodules in {repo}")
    for line in paths.stdout.splitlines():
        _, relative_path = line.split(None, 1)
        child = (repo / relative_path).resolve()
        if child in seen or not child.is_dir() or not (child / ".git").exists():
            continue
        if git(child, "rev-parse", "--show-toplevel", check=False).returncode == 0:
            repositories.append(child)
            seen.add(child)

if protobuf_initialized:
    pristine = git(protobuf_repo, "show", "HEAD:cmake/install.cmake").stdout
    suffix = ";${CMAKE_INSTALL_RPATH}"
    patched = pristine
    for value in ("$ORIGIN", "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"):
        original = f'"{value}"'
        replacement = f'"{value}{suffix}"'
        if original in patched:
            patched = patched.replace(original, replacement)
        elif replacement not in patched:
            raise SystemExit(f"pinned protobuf install.cmake lacks expected RPATH value {value}")
    if protobuf_file.read_text() not in (pristine, patched):
        raise SystemExit("protobuf install.cmake differs from its pinned source and approved RPATH patch")

allowed_zlib_rename = False
if zlib_initialized:
    zlib_status = git(zlib_repo, "status", "--porcelain=v1", "--untracked-files=all",
                      "--ignore-submodules=none").stdout.splitlines()
    expected_status = {(" D", "zconf.h"), ("??", "zconf.h.included")}
    actual_status = {(line[:2], line[3:]) for line in zlib_status}
    original_header = git_bytes(zlib_repo, "show", "HEAD:zconf.h")
    renamed_header = zlib_repo / "zconf.h.included"
    allowed_zlib_rename = (
        actual_status == expected_status and
        not (zlib_repo / "zconf.h").exists() and
        renamed_header.is_file() and renamed_header.read_bytes() == original_header
    )

allowed_submodule = protobuf_repo.relative_to(grpc_root).as_posix()
allowed_zlib_submodule = zlib_repo.relative_to(grpc_root).as_posix()
dirty = []
for repo in repositories:
    result = git(repo, "status", "--porcelain=v1", "--untracked-files=all",
                 "--ignore-submodules=none")
    for line in result.stdout.splitlines():
        status, path = line[:2], line[3:]
        if repo == protobuf_repo and status == " M" and path == "cmake/install.cmake":
            continue
        if repo == grpc_root and status == " M" and path == allowed_submodule:
            continue
        if repo == zlib_repo and allowed_zlib_rename and (status, path) in expected_status:
            continue
        if repo == grpc_root and allowed_zlib_rename and status == " M" and path == allowed_zlib_submodule:
            continue
        dirty.append(f"{repo}: {line}")
if dirty:
    raise SystemExit("refusing dirty pinned gRPC source tree; only the exact "
                     "protobuf RPATH patch and zlib header rename are allowed:\n" +
                     "\n".join(dirty))
PY
}

# Check existing checkouts before submodule update can change their revisions.
check_grpc_source_tree allow-uninitialized
git -C "$GRPC_SOURCE" submodule update --init --recursive --depth 1 \
  2>&1 | tee "$LOGS/grpc-submodules.log"
# Then require every recursive submodule at its pinned revision and clean.
check_grpc_source_tree require-initialized
{
  printf 'grpc %s %s\n' "$GRPC_TAG" "$ACTUAL_GRPC_COMMIT"
  git -C "$GRPC_SOURCE" submodule status --recursive
} > "$SCRATCH/dependency-revisions.txt"

python3 - "$PROTOBUF_INSTALL_CMAKE" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()
suffix = ";${CMAKE_INSTALL_RPATH}"
values = ("$ORIGIN", "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
for value in values:
    original = f'"{value}"'
    patched = f'"{value}{suffix}"'
    if original in text:
        text = text.replace(original, patched)
    elif patched not in text:
        raise SystemExit(f"expected protobuf INSTALL_RPATH value not found: {value}")
path.write_text(text)
PY

"$CMAKE_BIN" -S "$GRPC_SOURCE" -B "$GRPC_BUILD" "${GENERATOR_ARGS[@]}" \
  -DCMAKE_C_COMPILER="$SDK/bin/clang" \
  -DCMAKE_CXX_COMPILER="$SDK/bin/clang++" \
  -DCMAKE_SYSROOT="$SDK_SYSROOT" \
  -DCMAKE_CXX_STANDARD=17 \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$DEPS" \
  -DCMAKE_PREFIX_PATH="$DEPS" \
  -DCMAKE_BUILD_RPATH="$RPATH" \
  -DCMAKE_INSTALL_RPATH="$RPATH" \
  -DCMAKE_EXE_LINKER_FLAGS="$EXE_LINKER_FLAGS" \
  -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-rpath,$DEPS/lib:$DEPS/lib64:$SDK_TRIPLE/lib64:$SDK_SYSROOT/lib64" \
  -DABSL_PROPAGATE_CXX_STD=ON \
  -DgRPC_INSTALL=ON \
  -DgRPC_BUILD_TESTS=OFF \
  -DgRPC_BUILD_GRPC_CPP_PLUGIN=ON \
  -DgRPC_BUILD_CSHARP_EXT=OFF \
  -DgRPC_BUILD_GRPC_CSHARP_PLUGIN=OFF \
  -DgRPC_BUILD_GRPC_NODE_PLUGIN=OFF \
  -DgRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN=OFF \
  -DgRPC_BUILD_GRPC_PHP_PLUGIN=OFF \
  -DgRPC_BUILD_GRPC_PYTHON_PLUGIN=OFF \
  -DgRPC_BUILD_GRPC_RUBY_PLUGIN=OFF \
  -Dprotobuf_BUILD_TESTS=OFF \
  -Dprotobuf_BUILD_SHARED_LIBS=OFF \
  2>&1 | tee "$LOGS/grpc-configure.log"
"$CMAKE_BIN" --build "$GRPC_BUILD" --parallel "$JOBS" \
  2>&1 | tee "$LOGS/grpc-build.log"
"$CMAKE_BIN" --install "$GRPC_BUILD" \
  2>&1 | tee "$LOGS/grpc-install.log"

if [[ ! -f $SQLITE_ARCHIVE ]]; then
  curl --fail --location --retry 3 "$SQLITE_URL" --output "$SQLITE_ARCHIVE" \
    2>&1 | tee "$LOGS/sqlite-download.log"
fi
SQLITE_SHA256=$(sha256sum "$SQLITE_ARCHIVE" | awk '{print $1}')
if [[ -n ${SQLITE_EXPECTED_SHA256:-} && $SQLITE_SHA256 != "$SQLITE_EXPECTED_SHA256" ]]; then
  echo "SQLite archive checksum mismatch: expected $SQLITE_EXPECTED_SHA256, found $SQLITE_SHA256" >&2
  exit 1
fi
printf '%s  %s\nURL: %s\n' "$SQLITE_SHA256" "$(basename -- "$SQLITE_ARCHIVE")" "$SQLITE_URL" \
  > "$SCRATCH/sqlite3-checksum.txt"
if [[ ! -f $SQLITE_SOURCE/configure ]]; then
  tar -xzf "$SQLITE_ARCHIVE" -C "$DEPS/src"
fi
(
  cd "$SQLITE_SOURCE"
  CC="$SDK/bin/clang" \
  CFLAGS="-O2 -fPIC --sysroot=$SDK_SYSROOT" \
  LDFLAGS="$EXE_LINKER_FLAGS" \
    ./configure --prefix="$SQLITE_PREFIX" --disable-readline --disable-tcl
  make -j"$JOBS"
  make install
) 2>&1 | tee "$LOGS/sqlite-build-install.log"

"$DEPS/bin/protoc" --version | tee "$LOGS/protoc-version.log"
APP_RPATH="$DEPS/lib;$DEPS/lib64;$SDK_TRIPLE/lib64;$SDK_SYSROOT/lib64;$SDK/lib.llvm;$SDK/lib.llvm/lib"
"$CMAKE_BIN" -S "$SOURCE_ROOT" -B "$APP_BUILD" "${GENERATOR_ARGS[@]}" \
  -DCMAKE_TOOLCHAIN_FILE="$SOURCE_ROOT/cmake/toolchains/gcc85-sdk.cmake" \
  -DTRACER_SDK_ROOT="$SDK" \
  -DCMAKE_SYSROOT="$SDK_SYSROOT" \
  -DTRACER_DYNAMIC_LOADER="$SDK_LOADER" \
  -DCMAKE_PREFIX_PATH="$DEPS" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DProtobuf_USE_STATIC_LIBS=ON \
  -DCMAKE_INSTALL_PREFIX="$APP_PREFIX" \
  -DCMAKE_BUILD_RPATH="$APP_RPATH" \
  -DCMAKE_INSTALL_RPATH="$APP_RPATH" \
  -DBUILD_TESTING=ON \
  2>&1 | tee "$LOGS/tracer-configure.log"
"$CMAKE_BIN" --build "$APP_BUILD" --parallel "$JOBS" \
  2>&1 | tee "$LOGS/tracer-build.log"
"$CTEST_BIN" --test-dir "$APP_BUILD" --output-on-failure \
  2>&1 | tee "$LOGS/tracer-tests.log"
if [[ $OVERLAY_TESTS == 1 ]]; then
  PATH="$(dirname -- "$CTEST_BIN"):$PATH" \
    "$SOURCE_ROOT/tests/overlay_integration.sh" "$APP_BUILD" \
    2>&1 | tee "$LOGS/tracer-overlay-tests.log"
fi
"$CMAKE_BIN" --install "$APP_BUILD" \
  2>&1 | tee "$LOGS/tracer-install.log"

printf 'Build installed under %s\nLogs: %s\n' "$APP_PREFIX" "$LOGS"

#!/usr/bin/env bash
set -euo pipefail

# Exercise gcc85-build.sh without downloading, compiling dependencies, or
# mounting OverlayFS. All dependency-facing commands are bounded test doubles.

REPO_ROOT=$(cd -- "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)
TMP_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/tracer-gcc85-build-script.XXXXXX")
TMP_ROOT=$(cd -- "$TMP_ROOT" && pwd -P)
cleanup() {
  rm -rf "$TMP_ROOT"
}
trap cleanup EXIT

fail() {
  echo "gcc85-build script test failed: $*" >&2
  exit 1
}

record() {
  printf '%s\n' "$1" >> "$TEST_EVENTS"
}

assert_order() {
  local file=$1
  shift
  local previous=0 event line
  for event in "$@"; do
    line=$(grep -nFx "$event" "$file" | head -n 1 | cut -d: -f1 || true)
    [[ -n $line ]] || fail "missing event '$event' in $file"
    (( line > previous )) || fail "event '$event' occurred out of order in $file"
    previous=$line
  done
}

setup_case() {
  local name=$1 sibling_ctest=$2
  CASE_ROOT="$TMP_ROOT/$name"
  PROJECT_ROOT="$CASE_ROOT/project root"
  SDK_ROOT="$CASE_ROOT/fake SDK"
  SCRATCH_ROOT="$CASE_ROOT/scratch root"
  PRE_TOOLS="$CASE_ROOT/pre tools"
  HOST_TOOLS="$CASE_ROOT/host tools"
  TEST_EVENTS="$CASE_ROOT/events.log"
  TEST_APP_BUILD="$SCRATCH_ROOT/build/clang-tracer"
  TEST_DEPS="$SCRATCH_ROOT/deps"
  TEST_INSTALL_MARKER="$SCRATCH_ROOT/install/app-installed"
  TEST_GRPC_ROOT="$TEST_DEPS/src/grpc"
  EXPECTED_CTEST="$PRE_TOOLS/ctest"
  if [[ $sibling_ctest == yes ]]; then
    EXPECTED_CTEST="$HOST_TOOLS/ctest"
  fi
  mkdir -p "$PROJECT_ROOT/scripts" "$PROJECT_ROOT/tests" \
    "$SDK_ROOT/bin" "$SDK_ROOT/x86_64-1a-linux-gnu/sysroot/lib64" \
    "$PRE_TOOLS" "$HOST_TOOLS" "$TEST_DEPS/src" \
    "$TEST_GRPC_ROOT/.git" "$TEST_GRPC_ROOT/third_party/protobuf/.git" \
    "$TEST_GRPC_ROOT/third_party/protobuf/cmake" \
    "$TEST_GRPC_ROOT/third_party/zlib/.git" \
    "$TEST_DEPS/src/sqlite-autoconf-3340100"
  cp "$REPO_ROOT/scripts/gcc85-build.sh" \
    "$PROJECT_ROOT/scripts/gcc85-build.sh"
  chmod +x "$PROJECT_ROOT/scripts/gcc85-build.sh"
  : > "$TEST_EVENTS"
  : > "$CASE_ROOT/cmake-args.log"

  cat > "$TEST_GRPC_ROOT/.gitmodules" <<'EOF'
[submodule "third_party/protobuf"]
  path = third_party/protobuf
[submodule "third_party/zlib"]
  path = third_party/zlib
EOF
  cat > "$TEST_GRPC_ROOT/third_party/protobuf/cmake/install.cmake" <<'EOF'
set(CMAKE_INSTALL_RPATH "$ORIGIN")
set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
EOF
  printf 'fixture zlib header\n' > "$TEST_GRPC_ROOT/third_party/zlib/zconf.h"
  printf 'sqlite fixture archive\n' > \
    "$TEST_DEPS/src/sqlite-autoconf-3340100.tar.gz"
  cat > "$TEST_DEPS/src/sqlite-autoconf-3340100/configure" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' sqlite-configure >> "$TEST_EVENTS"
EOF
  chmod +x "$TEST_DEPS/src/sqlite-autoconf-3340100/configure"
  for compiler in clang clang++; do
    cat > "$SDK_ROOT/bin/$compiler" <<'EOF'
#!/usr/bin/env bash
exit 0
EOF
    chmod +x "$SDK_ROOT/bin/$compiler"
  done
  cat > "$SDK_ROOT/x86_64-1a-linux-gnu/sysroot/lib64/ld-linux-x86-64.so.2" <<'EOF'
#!/usr/bin/env bash
exit 0
EOF
  chmod +x "$SDK_ROOT/x86_64-1a-linux-gnu/sysroot/lib64/ld-linux-x86-64.so.2"

  cat > "$PRE_TOOLS/git" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
repo=''
if [[ ${1:-} == -C ]]; then
  repo=$2
  shift 2
fi
cmd=${1:-}
shift || true
case $cmd in
  rev-parse)
    if [[ ${1:-} == --show-toplevel ]]; then
      printf '%s\n' "$repo"
    elif [[ $repo == "$TEST_GRPC_ROOT" ]]; then
      echo 02384e39185f109bd299eb8482306229967dc970
    else
      printf '%s\n' "$repo"
    fi
    ;;
  submodule)
    if [[ ${1:-} == status ]]; then
      printf ' 1111111111111111111111111111111111111111 third_party/protobuf\n'
      printf ' 2222222222222222222222222222222222222222 third_party/zlib\n'
    fi
    ;;
  config)
    if [[ $repo == "$TEST_GRPC_ROOT" ]]; then
      printf 'submodule.third_party/protobuf.path third_party/protobuf\n'
      printf 'submodule.third_party/zlib.path third_party/zlib\n'
    else
      exit 1
    fi
    ;;
  status)
    if [[ $repo == "$TEST_GRPC_ROOT/third_party/protobuf" ]] && \
       ! cmp -s "$repo/cmake/install.cmake" "$TEST_GRPC_ROOT/protobuf-pristine.cmake"; then
      printf ' M cmake/install.cmake\n'
    elif [[ $repo == "$TEST_GRPC_ROOT" ]] && \
         ! cmp -s "$TEST_GRPC_ROOT/third_party/protobuf/cmake/install.cmake" \
                 "$TEST_GRPC_ROOT/protobuf-pristine.cmake"; then
      printf ' M third_party/protobuf\n'
    fi
    ;;
  show)
    case $repo:$1 in
      "$TEST_GRPC_ROOT/third_party/protobuf":HEAD:cmake/install.cmake)
        cat "$TEST_GRPC_ROOT/protobuf-pristine.cmake" ;;
      "$TEST_GRPC_ROOT/third_party/zlib":HEAD:zconf.h)
        printf 'fixture zlib header\n' ;;
      *) exit 1 ;;
    esac
    ;;
  *)
    echo "unexpected git operation: $cmd $*" >&2
    exit 90
    ;;
esac
EOF
  cp "$TEST_GRPC_ROOT/third_party/protobuf/cmake/install.cmake" \
    "$TEST_GRPC_ROOT/protobuf-pristine.cmake"
  chmod +x "$PRE_TOOLS/git"

  cat > "$PRE_TOOLS/cmake" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
record() { printf '%s\n' "$1" >> "$TEST_EVENTS"; }
if [[ ${1:-} == --version ]]; then
  echo 'cmake version 3.28.0 (stub)'
  exit 0
fi
if [[ ${1:-} == --build ]]; then
  if [[ $2 == "$TEST_APP_BUILD" ]]; then record app-build; else record grpc-build; fi
  exit 0
fi
if [[ ${1:-} == --install ]]; then
  if [[ $2 == "$TEST_APP_BUILD" ]]; then
    record app-install
    mkdir -p "$(dirname "$TEST_INSTALL_MARKER")"
    : > "$TEST_INSTALL_MARKER"
  else
    record grpc-install
    mkdir -p "$TEST_DEPS/bin"
    cat > "$TEST_DEPS/bin/protoc" <<'PROTOC'
#!/usr/bin/env bash
echo 'libprotoc 3.20.0 (stub)'
PROTOC
    chmod +x "$TEST_DEPS/bin/protoc"
  fi
  exit 0
fi
source_dir=''
build_dir=''
args=("$@")
for ((index = 0; index < ${#args[@]}; ++index)); do
  case ${args[index]} in
    -S) source_dir=${args[index+1]} ;;
    -B) build_dir=${args[index+1]} ;;
  esac
done
if [[ $source_dir == "$TEST_PROJECT" ]]; then
  record app-configure
  mkdir -p -- "$build_dir"
  : > "$build_dir/CTestTestfile.cmake"
  for arg in "${args[@]}"; do
    case $arg in
      -DTRACER_SDK_ROOT=*) printf 'sdk-root=%s\n' "${arg#*=}" >> "$TEST_CMAKE_ARGS" ;;
      -DCMAKE_SYSROOT=*) printf 'sysroot=%s\n' "${arg#*=}" >> "$TEST_CMAKE_ARGS" ;;
      -DTRACER_DYNAMIC_LOADER=*) printf 'loader=%s\n' "${arg#*=}" >> "$TEST_CMAKE_ARGS" ;;
    esac
  done
else
  record grpc-configure
  for arg in "${args[@]}"; do
    [[ $arg == -DCMAKE_CXX_STANDARD=17 ]] && record grpc-cxx17
  done
fi
exit 0
EOF
  chmod +x "$PRE_TOOLS/cmake"
  cp "$PRE_TOOLS/cmake" "$HOST_TOOLS/cmake"
  chmod +x "$HOST_TOOLS/cmake"

  cat > "$PRE_TOOLS/ctest" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
record() { printf '%s\n' "$1" >> "$TEST_EVENTS"; }
kind=${MOCK_CTEST_KIND:-normal}
record "${kind}-ctest:${BASH_SOURCE[0]}"
[[ ${1:-} == --test-dir && ${3:-} == --output-on-failure ]] || {
  echo "wrong ctest arguments: $*" >&2
  exit 91
}
[[ $2 == "$TEST_APP_BUILD" && -f $2/CTestTestfile.cmake ]] || {
  echo 'CTest ran before app configure' >&2
  exit 92
}
if [[ $kind == normal && ${MOCK_CTEST_EXIT:-0} != 0 ]]; then
  echo "mock ${kind} CTest failing with status ${MOCK_CTEST_EXIT}"
  exit "$MOCK_CTEST_EXIT"
fi
echo "mock ${kind} CTest passed"
EOF
  chmod +x "$PRE_TOOLS/ctest"
  if [[ $sibling_ctest == yes ]]; then
    cp "$PRE_TOOLS/ctest" "$HOST_TOOLS/ctest"
    chmod +x "$HOST_TOOLS/ctest"
  fi
  cat > "$SDK_ROOT/bin/ctest" <<'EOF'
#!/usr/bin/env bash
printf 'sdk-ctest\n' >> "$TEST_EVENTS"
echo 'SDK CTest trap executed' >&2
exit 97
EOF
  chmod +x "$SDK_ROOT/bin/ctest"

  cat > "$PRE_TOOLS/overlay_integration.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
record() { printf '%s\n' "$1" >> "$TEST_EVENTS"; }
[[ $# -eq 1 && $1 == "$TEST_APP_BUILD" ]] || exit 93
[[ $(command -v ctest) == "$TEST_EXPECTED_CTEST" ]] || {
  echo "overlay PATH selected wrong CTest: $(command -v ctest)" >&2
  exit 94
}
record overlay-runner
MOCK_CTEST_KIND=overlay ctest --test-dir "$1" --output-on-failure
if [[ ${MOCK_OVERLAY_EXIT:-0} != 0 ]]; then
  echo "mock overlay runner failing with status ${MOCK_OVERLAY_EXIT}"
  exit "$MOCK_OVERLAY_EXIT"
fi
echo 'mock overlay runner passed'
EOF
  chmod +x "$PRE_TOOLS/overlay_integration.sh"
  cp "$PRE_TOOLS/overlay_integration.sh" "$PROJECT_ROOT/tests/overlay_integration.sh"
  chmod +x "$PROJECT_ROOT/tests/overlay_integration.sh"

  cat > "$PRE_TOOLS/make" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
record() { printf '%s\n' "$1" >> "$TEST_EVENTS"; }
if [[ $PWD == */sqlite-autoconf-3340100 ]]; then
  if [[ ${1:-} == install ]]; then record sqlite-install; else record sqlite-build; fi
fi
EOF
  cat > "$PRE_TOOLS/curl" <<'EOF'
#!/usr/bin/env bash
record() { printf '%s\n' "$1" >> "$TEST_EVENTS"; }
echo 'unexpected curl/download' >&2
record curl-called
exit 95
EOF
  cat > "$PRE_TOOLS/tar" <<'EOF'
#!/usr/bin/env bash
echo 'unexpected archive extraction' >&2
record tar-called
exit 96
EOF
  cat > "$PRE_TOOLS/sha256sum" <<'EOF'
#!/usr/bin/env bash
printf '%s  %s\n' 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef "$1"
EOF
  cat > "$PRE_TOOLS/ninja" <<'EOF'
#!/usr/bin/env bash
echo '1.11.1 (stub)'
EOF
  chmod +x "$PRE_TOOLS/make" "$PRE_TOOLS/curl" "$PRE_TOOLS/tar" \
    "$PRE_TOOLS/sha256sum" "$PRE_TOOLS/ninja"

  export TEST_EVENTS TEST_APP_BUILD TEST_DEPS TEST_INSTALL_MARKER TEST_GRPC_ROOT
  export TEST_PROJECT="$PROJECT_ROOT" TEST_SDK="$SDK_ROOT"
  export TEST_CMAKE_ARGS="$CASE_ROOT/cmake-args.log"
  export TEST_EXPECTED_CTEST="$EXPECTED_CTEST"
  export PATH="$PRE_TOOLS:$PATH"
  export SQLITE_EXPECTED_SHA256=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
  export TRACER_CMAKE="$HOST_TOOLS/cmake"
  export MOCK_CTEST_EXIT=0 MOCK_OVERLAY_EXIT=0
  export MOCK_CTEST_KIND=normal
  RUN_SCRIPT="$PROJECT_ROOT/scripts/gcc85-build.sh"
}

run_script() {
  local expected_status=$1
  shift
  local output="$CASE_ROOT/build-output.log" actual_status
  set +e
  "$RUN_SCRIPT" "$SDK_ROOT" "$SCRATCH_ROOT" 1 "$@" > "$output" 2>&1
  actual_status=$?
  set -e
  [[ $actual_status -eq $expected_status ]] || {
    cat "$output" >&2
    fail "build script returned $actual_status, expected $expected_status"
  }
  BUILD_OUTPUT=$output
}

check_no_install_or_success() {
  [[ ! -e $TEST_INSTALL_MARKER ]] || fail 'app install ran after a test failure'
  ! grep -q 'Build installed under' "$BUILD_OUTPUT" || \
    fail 'build script printed final success after a test failure'
}

help_output=$("$REPO_ROOT/scripts/gcc85-build.sh" --help)
[[ $help_output == *--overlay-tests* ]] || fail '--help omits --overlay-tests'

# SDK/scratch paths and optional path overrides contain spaces. CMake sees
# each as one intact argument, while the host CTest sibling is selected.
setup_case default-sibling yes
set +e
"$RUN_SCRIPT" "$SDK_ROOT" "$SCRATCH_ROOT" 1 --unknown-option \
  > "$CASE_ROOT/parse-unknown.log" 2>&1
unknown_status=$?
"$RUN_SCRIPT" relative-sdk "$SCRATCH_ROOT" 1 \
  > "$CASE_ROOT/parse-relative-sdk.log" 2>&1
relative_status=$?
set -e
[[ $unknown_status -eq 2 ]] || fail 'unknown option did not return status 2'
[[ $relative_status -eq 2 ]] || fail 'relative SDK path did not return status 2'
custom_sysroot="$CASE_ROOT/custom sysroot"
custom_loader="$CASE_ROOT/custom loader"
  mkdir -p "$custom_sysroot"
printf '#!/usr/bin/env bash\nexit 0\n' > "$custom_loader"
chmod +x "$custom_loader"
run_script 0 --sysroot "$custom_sysroot" --dynamic-loader "$custom_loader"
assert_order "$TEST_EVENTS" grpc-configure grpc-cxx17 grpc-build grpc-install \
  sqlite-configure sqlite-build sqlite-install app-configure app-build \
  "normal-ctest:$HOST_TOOLS/ctest" app-install
! grep -q '^overlay-runner$' "$TEST_EVENTS" || \
  fail 'overlay runner ran without --overlay-tests'
[[ -f $SCRATCH_ROOT/logs/tracer-tests.log ]] || fail 'normal CTest log missing'
grep -q 'mock normal CTest passed' "$SCRATCH_ROOT/logs/tracer-tests.log" || \
  fail 'normal CTest output missing from log'
grep -Fx "sdk-root=$SDK_ROOT" "$TEST_CMAKE_ARGS" >/dev/null || \
  fail 'SDK root with spaces was split or changed'
grep -Fx "sysroot=$custom_sysroot" "$TEST_CMAKE_ARGS" >/dev/null || \
  fail 'explicit sysroot with spaces was split or changed'
grep -Fx "loader=$custom_loader" "$TEST_CMAKE_ARGS" >/dev/null || \
  fail 'explicit loader with spaces was split or changed'
! grep -q '^sdk-ctest$' "$TEST_EVENTS" || fail 'SDK CTest trap ran'
[[ ! -e $SCRATCH_ROOT/logs/tracer-overlay-tests.log ]] || \
  fail 'overlay CTest log exists without --overlay-tests'
[[ -e $TEST_INSTALL_MARKER ]] || fail 'normal build did not install app'

# Optional overlay path must follow normal CTest and precede app install.
setup_case overlay-success yes
run_script 0 --overlay-tests
assert_order "$TEST_EVENTS" app-build "normal-ctest:$HOST_TOOLS/ctest" \
  overlay-runner "overlay-ctest:$HOST_TOOLS/ctest" app-install
[[ -f $SCRATCH_ROOT/logs/tracer-tests.log ]] || fail 'normal CTest log missing in overlay run'
[[ -f $SCRATCH_ROOT/logs/tracer-overlay-tests.log ]] || fail 'overlay CTest log missing'
grep -q 'mock overlay runner passed' "$SCRATCH_ROOT/logs/tracer-overlay-tests.log" || \
  fail 'mock overlay output missing from log'
[[ -e $TEST_INSTALL_MARKER ]] || fail 'overlay build did not install app'
! grep -q '^sdk-ctest$' "$TEST_EVENTS" || fail 'SDK CTest trap ran in overlay mode'

# pipefail must preserve CTest's status and stop before overlay or install.
setup_case ctest-failure yes
MOCK_CTEST_EXIT=23
export MOCK_CTEST_EXIT
run_script 23 --overlay-tests
assert_order "$TEST_EVENTS" app-build "normal-ctest:$HOST_TOOLS/ctest"
! grep -q '^overlay-runner$' "$TEST_EVENTS" || fail 'overlay ran after normal CTest failure'
[[ -f $SCRATCH_ROOT/logs/tracer-tests.log ]] || fail 'failed CTest log missing'
check_no_install_or_success

# Overlay failure also propagates through tee and prevents app installation.
setup_case overlay-failure yes
MOCK_OVERLAY_EXIT=29
export MOCK_OVERLAY_EXIT
run_script 29 --overlay-tests
assert_order "$TEST_EVENTS" app-build "normal-ctest:$HOST_TOOLS/ctest" \
  overlay-runner "overlay-ctest:$HOST_TOOLS/ctest"
[[ -f $SCRATCH_ROOT/logs/tracer-tests.log && \
   -f $SCRATCH_ROOT/logs/tracer-overlay-tests.log ]] || \
  fail 'normal or overlay test log missing after overlay failure'
check_no_install_or_success

# With no sibling CTest, selection falls back to the original host PATH before
# the SDK bin directory is prepended. The SDK trap must still remain unused.
setup_case path-fallback no
run_script 0
assert_order "$TEST_EVENTS" app-build "normal-ctest:$PRE_TOOLS/ctest" app-install
[[ -e $TEST_INSTALL_MARKER ]] || fail 'PATH fallback build did not install app'
! grep -q '^sdk-ctest$' "$TEST_EVENTS" || fail 'SDK CTest trap ran in PATH fallback mode'

echo 'gcc85-build script harness passed (normal, failures, overlay opt-in, CTest selection, and spaced SDK paths)'

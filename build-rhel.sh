#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
build_dir="$source_dir/build-rhel"
prefix=/usr/local; bindir=bin; libdir=lib64; jobs=2; build_type=RelWithDebInfo
cc=${CC:-}; cxx=${CXX:-}; llvm_root=${TRACER_LLVM_ROOT:-}; llvm_dir=; clang_dir=; toolchain_file=
examples=OFF; tests=OFF; package=; extra=()
usage() {
  cat <<'TXT'
Usage: ./build-rhel.sh [options] [-- additional CMake arguments]
  --build-dir DIR          Build directory (default: build-rhel)
  --toolchain-root DIR     Select DIR/bin/clang{,++} and LLVM packages in DIR
  --llvm-root DIR          Select LLVM/Clang package prefix only
  --llvm-dir DIR           Explicit directory containing LLVMConfig.cmake
  --clang-dir DIR          Explicit directory containing ClangConfig.cmake
  --cc FILE --cxx FILE     C and C++ compilers; CC/CXX also work
  --toolchain-file FILE    Native CMake toolchain file
  --prefix DIR            Install prefix (default: /usr/local)
  --bindir DIR --libdir DIR  Binary/library install directories (bin, lib64)
  --build-type TYPE        CMake build type (default: RelWithDebInfo)
  --jobs N                Parallel compile jobs (default: 2)
  --examples              Build runnable example targets
  --with-tests            Build integration fixtures; does not run them
  --package TGZ|RPM        Also create an installable archive/RPM
  --help                  Show this help
Use a fresh build directory when changing compilers or toolchains.
TXT
}
while (($#)); do
  case "$1" in
    --help|-h) usage; exit 0 ;;
    --examples) examples=ON; shift; continue ;;
    --with-tests) tests=ON; shift; continue ;;
    --) shift; extra=("$@"); break ;;
  esac
  [[ $# -ge 2 && -n "$2" ]] || { echo "Missing value for $1" >&2; exit 2; }
  case "$1" in
    --build-dir) build_dir=$2 ;;
    --toolchain-root) llvm_root=$2; cc=${cc:-$2/bin/clang}; cxx=${cxx:-$2/bin/clang++} ;;
    --llvm-root) llvm_root=$2 ;;
    --llvm-dir) llvm_dir=$2 ;;
    --clang-dir) clang_dir=$2 ;;
    --cc) cc=$2 ;;
    --cxx) cxx=$2 ;;
    --toolchain-file) toolchain_file=$(realpath -- "$2") ;;
    --prefix) prefix=$2 ;;
    --bindir) bindir=$2 ;;
    --libdir) libdir=$2 ;;
    --build-type) build_type=$2 ;;
    --jobs) jobs=$2 ;;
    --package) package=$2 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
  shift 2
done
[[ $(uname -s) == Linux && $(uname -m) == x86_64 ]] || { echo 'Build on Linux x86_64.' >&2; exit 2; }
# RHEL derivatives identify themselves through ID_LIKE.
. /etc/os-release
[[ " ${ID:-} ${ID_LIKE:-} " =~ (rhel|rocky|almalinux|centos) ]] || { echo 'This script targets RHEL-compatible distributions.' >&2; exit 2; }
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo '--jobs must be positive.' >&2; exit 2; }
[[ -z "$package" || "$package" == TGZ || "$package" == RPM ]] || { echo '--package must be TGZ or RPM.' >&2; exit 2; }
if [[ "$package" == RPM ]] && ! command -v rpmbuild >/dev/null; then
  echo 'RPM packaging requires the rpm-build package.' >&2; exit 2
fi
args=(-S "$source_dir" -B "$build_dir" -G Ninja
  "-DCMAKE_BUILD_TYPE=$build_type" "-DCMAKE_INSTALL_PREFIX=$prefix"
  "-DCMAKE_INSTALL_BINDIR=$bindir" "-DCMAKE_INSTALL_LIBDIR=$libdir"
  "-DTRACER_BUILD_EXAMPLES=$examples" "-DBUILD_TESTING=$tests")
[[ -z "$cc" ]] || args+=("-DCMAKE_C_COMPILER=$cc")
[[ -z "$cxx" ]] || args+=("-DCMAKE_CXX_COMPILER=$cxx")
[[ -z "$llvm_root" ]] || args+=("-DTRACER_LLVM_ROOT=$llvm_root")
[[ -z "$llvm_dir" ]] || args+=("-DLLVM_DIR=$llvm_dir")
[[ -z "$clang_dir" ]] || args+=("-DClang_DIR=$clang_dir")
[[ -z "$toolchain_file" ]] || args+=("-DCMAKE_TOOLCHAIN_FILE=$toolchain_file")
cmake "${args[@]}" "${extra[@]}"
cmake --build "$build_dir" --parallel "$jobs"
if [[ -n "$package" ]]; then
  cpack --config "$build_dir/CPackConfig.cmake" -G "$package" -B "$build_dir/packages"
fi

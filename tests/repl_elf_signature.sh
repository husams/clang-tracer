#!/usr/bin/env bash
set -euo pipefail

repl=$1
server=$2
agent=$3
target=$4
objcopy=$5
unset CLANG_TRACER_ENDPOINT

run=$(mktemp -d)
pid=
cleanup() {
  if [[ -n "$pid" ]]; then
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
  fi
  find "$run" -depth -delete
}
trap cleanup EXIT
cd "$run"

"$objcopy" --strip-debug "$target" target
if "$repl" --fail-fast > rejected.log 2>&1 <<'REPL'
%probe prepare "Configuration<int>::get() const" --signature "const int&(int)" --module target --output wrong
REPL
then
  echo 'mismatched parameters were accepted' >&2
  exit 1
fi
grep -q 'signature parameters differ from the ELF symbol' rejected.log
test ! -e wrong

"$repl" --fail-fast <<'REPL'
%probe prepare "Configuration<int>::get() const" --signature "const int&()" --module target --output getter
%probe prepare "echo<int>(int)" --signature "int(int)" --module target --output function
%quit
REPL
test -s getter/prepared.pb
test -s function/prepared.pb

cat > observer.cpp <<'CPP'
template <> class Interceptor<tracer::ResolvedSignature> {
public:
  void post(const Call &call) { (void)call.result(); }
};
CPP

./target &
pid=$!
"$repl" --server "$server" --agent "$agent" --attach "$pid" --timeout 45 \
  --fail-fast <<'REPL'
%probe inject "Configuration<int>::get() const" --prepared getter --prepared-mode require
%collect --name "Configuration<int>::get() const" --phase POST --count 1 --timeout 15 --as captured
if (tracer.batch("captured").at(0).result_as<int>() != 42) throw std::runtime_error("wrong ELF reference result");

%probe inject "echo<int>(int)" --callback observer.cpp --prepared function --prepared-mode require
%collect --name "echo<int>(int)" --phase POST --count 1 --timeout 15 --as captured
if (tracer.batch("captured").at(0).result_as<int>() != 43) throw std::runtime_error("wrong ELF template result");
%quit
REPL

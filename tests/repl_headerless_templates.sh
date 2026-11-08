#!/usr/bin/env bash
set -euo pipefail

repl=$1
server=$2
agent=$3
target=$4
unset CLANG_TRACER_ENDPOINT

run=$(mktemp -d)
pid=
cleanup() {
  if [[ -n "$pid" ]]; then
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
  fi
  rm -rf -- "$run"
}
trap cleanup EXIT
cd "$run"

cat > observer.cpp <<'CPP'
template <> class Interceptor<tracer::ResolvedSignature> {
public:
  void post(const Call &call) { (void)call.result(); }
};
CPP

"$target" &
pid=$!
"$repl" --server "$server" --agent "$agent" --attach "$pid" --timeout 45 \
  --fail-fast <<'REPL'
%probe inject "Configuration<int>::get() const" --callback observer.cpp --prepared-mode off
%collect --name "Configuration<int>::get() const" --phase POST --count 1 --timeout 15 --as ref_result
if (tracer.batch("ref_result").at(0).result_as<int>() != 42) throw std::runtime_error("wrong reference result");

%probe inject "AdjustedConfiguration<int, 5>::Converter::convert<long>() const" --callback observer.cpp --prepared-mode off
%collect --name "AdjustedConfiguration<int, 5>::Converter::convert<long>() const" --phase POST --count 1 --timeout 15 --as method_result
if (tracer.batch("method_result").at(0).result_as<long>() != 47) throw std::runtime_error("wrong member template result");

%probe inject "echo<int>(int)" --callback observer.cpp --prepared-mode off
%collect --name "echo<int>(int)" --phase POST --count 1 --timeout 15 --as function_result
if (tracer.batch("function_result").at(0).result_as<int>() != 43) throw std::runtime_error("wrong function template result");
%quit
REPL
kill -0 "$pid"

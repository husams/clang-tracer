#!/usr/bin/env bash
# A JIT callback may call BoundedProtoWriter, whose out-of-line members the agent's
# version script keeps local. The agent must bind them in each probe's JIT symbol table.
set -euo pipefail

repl=$1
server=$2
agent=$3
target=$4
header=$5
callback=$6
compdb=$7
source_dir=$8
include_dir=$9
unset CLANG_TRACER_ENDPOINT

run=$(mktemp -d)
echo "Hidden helper symbol test: $run"
cd "$run"
ln -s "$(dirname -- "$header")" demo
cp "$callback" callback.cpp
ln -s "$(realpath -- "$compdb")" compile_commands.json
"$target" >target.log 2>&1 &
target_pid=$!
cleanup() {
  kill "$target_pid" 2>/dev/null || true
  wait "$target_pid" 2>/dev/null || true
}
trap cleanup EXIT

options=(--callback callback.cpp --extra-arg "-I$source_dir" --extra-arg "-I$include_dir")
"$repl" --server "$server" --agent "$agent" --attach "$target_pid" --timeout 60 --fail-fast <<REPL | tee smoke.log
%probe prepare "prepared_demo::compute(int)" --include demo/calculator.hpp --output prepared
%probe validate "prepared_demo::compute(int)" ${options[*]}
%probe inject "prepared_demo::compute(int)" ${options[*]} --print-arguments --print-result
%collect --count 4 --timeout 15 --phase POST --as captured
auto captured = tracer.batch("captured");
bool correct = captured.size() == 4;
for (const auto& e : captured) { int value = e.argument_as<int>(0); correct = correct && e.result_as<int>() == value * 3 + 7; }
std::cout << "HIDDEN_HELPER_RESULTS=" << correct << std::endl;
tracer.detach(session);
%quit
REPL
grep -q '^HIDDEN_HELPER_RESULTS=1$' smoke.log
! grep -q 'Symbols not found' smoke.log

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
mismatch=${10}
unset CLANG_TRACER_ENDPOINT

run=$(mktemp -d)
echo "Hidden helper symbol test: $run"
cd "$run"
ln -s "$(dirname -- "$header")" demo
cp "$callback" callback.cpp
ln -s "$(realpath -- "$compdb")" compile_commands.json
export HIDDEN_WRITER_OUT="$run/encoded.log"
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
# u64(1,7) s64(2,-3) boolean(3,true) f32(4,1.5) f64(5,2.5) string(6,"hidden") empty message(7);
# every call succeeded (9 bits), 4 items selected, 30 bytes, status ok
expected="0807 1005 1801 250000c03f 290000000000000440 320668696464656e 3a00"
expected="${expected// /} 30 4 511 0"
test "$(wc -l <encoded.log)" -ge 4
test "$(sort -u encoded.log)" = "$expected"
! grep -q 'Symbols not found' smoke.log

# A declaration whose parameter types differ from the agent's member must stay unresolved.
cp "$mismatch" mismatch.cpp
mismatch_options=(--callback mismatch.cpp)
if "$repl" --server "$server" --agent "$agent" --attach "$target_pid" --timeout 60 --fail-fast >mismatch.log 2>&1 <<REPL
%probe validate "prepared_demo::compute(int)" ${mismatch_options[*]} --prepared prepared
%quit
REPL
then
  cat mismatch.log
  echo "mismatched BoundedProtoWriter::string signature was bound"
  exit 1
fi
cat mismatch.log
grep -q 'Symbols not found' mismatch.log
grep -q '6stringEjPKc' mismatch.log
